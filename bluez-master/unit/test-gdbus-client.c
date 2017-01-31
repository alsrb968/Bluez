/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011  Intel Corporation
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gdbus.h>

#define SERVICE_NAME "org.bluez.unit.test-gdbus-client"
#define SERVICE_NAME1 "org.bluez.unit.test-gdbus-client1"
#define SERVICE_PATH "/org/bluez/unit/test_gdbus_client"

struct context {
	GMainLoop *main_loop;
	DBusConnection *dbus_conn;
	GDBusClient *dbus_client;
	GDBusProxy *proxy;
	void *data;
	guint timeout_source;
};

static const GDBusMethodTable methods[] = {
	{ }
};

static const GDBusSignalTable signals[] = {
	{ }
};

static const GDBusPropertyTable properties[] = {
	{ }
};

static struct context *create_context(void)
{
	struct context *context = g_new0(struct context, 1);
	DBusError err;

	context->main_loop = g_main_loop_new(NULL, FALSE);
	if (context->main_loop == NULL) {
		g_free(context);
		return NULL;
	}

	dbus_error_init(&err);

	context->dbus_conn = g_dbus_setup_private(DBUS_BUS_SESSION,
							SERVICE_NAME, &err);
	if (context->dbus_conn == NULL) {
		if (dbus_error_is_set(&err)) {
			if (g_test_verbose())
				g_printerr("D-Bus setup failed: %s\n",
								err.message);
			dbus_error_free(&err);
		}

		g_main_loop_unref(context->main_loop);
		g_free(context);
		return NULL;
	}

	/* Avoid D-Bus library calling _exit() before next test finishes. */
	dbus_connection_set_exit_on_disconnect(context->dbus_conn, FALSE);

	g_dbus_attach_object_manager(context->dbus_conn);

	return context;
}

static void destroy_context(struct context *context)
{
	if (context == NULL)
		return;

	if (context->timeout_source > 0)
		g_source_remove(context->timeout_source);

	g_dbus_detach_object_manager(context->dbus_conn);

	dbus_connection_flush(context->dbus_conn);
	dbus_connection_close(context->dbus_conn);

	g_main_loop_unref(context->main_loop);

	g_free(context->data);
	g_free(context);
}

static gboolean timeout_handler(gpointer user_data)
{
	struct context *context = user_data;

	if (g_test_verbose())
		g_print("timeout triggered\n");

	context->timeout_source = 0;

	g_dbus_client_unref(context->dbus_client);

	return FALSE;
}

static void connect_handler(DBusConnection *connection, void *user_data)
{
	struct context *context = user_data;

	if (g_test_verbose())
		g_print("service connected\n");

	g_dbus_client_unref(context->dbus_client);
}

static void disconnect_handler(DBusConnection *connection, void *user_data)
{
	struct context *context = user_data;

	if (g_test_verbose())
		g_print("service disconnected\n");

	g_main_loop_quit(context->main_loop);
}

static void simple_client(void)
{
	struct context *context = create_context();

	if (context == NULL)
		return;

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_connect_watch(context->dbus_client,
						connect_handler, context);
	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);

	g_main_loop_run(context->main_loop);

	destroy_context(context);
}

static void client_connect_disconnect(void)
{
	struct context *context = create_context();

	if (context == NULL)
		return;

	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, properties, NULL, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_connect_watch(context->dbus_client,
						connect_handler, context);
	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);

	context->timeout_source = g_timeout_add_seconds(10, timeout_handler,
								context);

	g_main_loop_run(context->main_loop);

	g_dbus_unregister_interface(context->dbus_conn,
					SERVICE_PATH, SERVICE_NAME);

	destroy_context(context);
}

static void append_variant(DBusMessageIter *iter, int type, void *val)
{
	DBusMessageIter value;
	char sig[2] = { type, '\0' };

	dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, sig, &value);

	dbus_message_iter_append_basic(&value, type, val);

	dbus_message_iter_close_container(iter, &value);
}

static void dict_append_entry(DBusMessageIter *dict, const char *key, int type,
								void *val)
{
	DBusMessageIter entry;

	if (type == DBUS_TYPE_STRING) {
		const char *str = *((const char **) val);
		if (str == NULL)
			return;
	}

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
							NULL, &entry);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

	append_variant(&entry, type, val);

	dbus_message_iter_close_container(dict, &entry);
}

static gboolean get_dict(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *data)
{
	DBusMessageIter dict;
	const char *string = "value";
	dbus_bool_t boolean = TRUE;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	dict_append_entry(&dict, "String", DBUS_TYPE_STRING, &string);
	dict_append_entry(&dict, "Boolean", DBUS_TYPE_BOOLEAN, &boolean);

	dbus_message_iter_close_container(iter, &dict);

	return TRUE;
}

static void proxy_get_dict(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;
	DBusMessageIter iter, dict, var1, var2, entry1, entry2;
	const char *string;
	dbus_bool_t boolean;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	g_assert(g_dbus_proxy_get_property(proxy, "Dict", &iter));
	g_assert(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY);

	dbus_message_iter_recurse(&iter, &dict);
	g_assert(dbus_message_iter_get_arg_type(&dict) ==
							DBUS_TYPE_DICT_ENTRY);

	dbus_message_iter_recurse(&dict, &entry1);
	g_assert(dbus_message_iter_get_arg_type(&entry1) == DBUS_TYPE_STRING);

	dbus_message_iter_get_basic(&entry1, &string);
	g_assert(g_strcmp0(string, "String") == 0);

	dbus_message_iter_next(&entry1);
	g_assert(dbus_message_iter_get_arg_type(&entry1) == DBUS_TYPE_VARIANT);

	dbus_message_iter_recurse(&entry1, &var1);
	g_assert(dbus_message_iter_get_arg_type(&var1) == DBUS_TYPE_STRING);

	dbus_message_iter_get_basic(&var1, &string);
	g_assert(g_strcmp0(string, "value") == 0);

	dbus_message_iter_next(&dict);
	g_assert(dbus_message_iter_get_arg_type(&dict) ==
							DBUS_TYPE_DICT_ENTRY);

	dbus_message_iter_recurse(&dict, &entry2);
	g_assert(dbus_message_iter_get_arg_type(&entry2) == DBUS_TYPE_STRING);

	dbus_message_iter_get_basic(&entry2, &string);
	g_assert(g_strcmp0(string, "Boolean") == 0);

	dbus_message_iter_next(&entry2);
	g_assert(dbus_message_iter_get_arg_type(&entry2) == DBUS_TYPE_VARIANT);

	dbus_message_iter_recurse(&entry2, &var2);
	g_assert(dbus_message_iter_get_arg_type(&var2) == DBUS_TYPE_BOOLEAN);

	dbus_message_iter_get_basic(&var2, &boolean);
	g_assert(boolean == TRUE);

	dbus_message_iter_next(&dict);
	g_assert(dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_INVALID);

	g_dbus_client_unref(context->dbus_client);
}

static void client_get_dict_property(void)
{
	struct context *context = create_context();
	static const GDBusPropertyTable dict_properties[] = {
		{ "Dict", "a{sv}", get_dict },
		{ },
	};

	if (context == NULL)
		return;

	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, dict_properties,
				NULL, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);
	g_dbus_client_set_proxy_handlers(context->dbus_client, proxy_get_dict,
						NULL, NULL, context);

	g_main_loop_run(context->main_loop);

	g_dbus_unregister_interface(context->dbus_conn,
					SERVICE_PATH, SERVICE_NAME);

	destroy_context(context);
}

static void proxy_get_string(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;
	DBusMessageIter iter;
	const char *string;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	g_assert(g_dbus_proxy_get_property(proxy, "String", &iter));
	g_assert(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);

	dbus_message_iter_get_basic(&iter, &string);
	g_assert(g_strcmp0(string, "value") == 0);

	g_dbus_client_unref(context->dbus_client);
}

static gboolean get_string(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *data)
{
	struct context *context = data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &context->data);

	return TRUE;
}

static void client_get_string_property(void)
{
	struct context *context = create_context();
	static const GDBusPropertyTable string_properties[] = {
		{ "String", "s", get_string },
		{ },
	};

	if (context == NULL)
		return;

	context->data = g_strdup("value");
	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, string_properties,
				context, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);
	g_dbus_client_set_proxy_handlers(context->dbus_client, proxy_get_string,
						NULL, NULL, context);

	g_main_loop_run(context->main_loop);

	g_dbus_unregister_interface(context->dbus_conn,
					SERVICE_PATH, SERVICE_NAME);

	destroy_context(context);
}

static void proxy_get_boolean(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;
	DBusMessageIter iter;
	dbus_bool_t value;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	g_assert(g_dbus_proxy_get_property(proxy, "Boolean", &iter));
	g_assert(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_BOOLEAN);

	dbus_message_iter_get_basic(&iter, &value);
	g_assert(value == TRUE);

	g_dbus_client_unref(context->dbus_client);
}

static gboolean get_boolean(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *data)
{
	dbus_bool_t value = TRUE;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &value);

	return TRUE;
}

static void client_get_boolean_property(void)
{
	struct context *context = create_context();
	static const GDBusPropertyTable boolean_properties[] = {
		{ "Boolean", "b", get_boolean },
		{ },
	};

	if (context == NULL)
		return;

	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, boolean_properties,
				NULL, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_proxy_handlers(context->dbus_client,
						proxy_get_boolean,
						NULL, NULL, context);
	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);

	g_main_loop_run(context->main_loop);

	g_dbus_unregister_interface(context->dbus_conn,
					SERVICE_PATH, SERVICE_NAME);

	destroy_context(context);
}

static void proxy_get_array(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;
	DBusMessageIter iter, entry;
	const char *value1, *value2;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	g_assert(g_dbus_proxy_get_property(proxy, "Array", &iter));
	g_assert(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY);

	dbus_message_iter_recurse(&iter, &entry);
	g_assert(dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING);

	dbus_message_iter_get_basic(&entry, &value1);
	g_assert(g_strcmp0(value1, "value1") == 0);

	dbus_message_iter_next(&entry);
	g_assert(dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING);

	dbus_message_iter_get_basic(&entry, &value2);
	g_assert(g_strcmp0(value2, "value2") == 0);

	g_dbus_client_unref(context->dbus_client);
}

static gboolean get_array(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *data)
{
	const char *value[2] = { "value1", "value2" };
	DBusMessageIter array;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "s", &array);

	dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &value[0]);
	dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &value[1]);

	dbus_message_iter_close_container(iter, &array);

	return TRUE;
}

static void client_get_array_property(void)
{
	struct context *context = create_context();
	static const GDBusPropertyTable array_properties[] = {
		{ "Array", "as", get_array },
		{ },
	};

	if (context == NULL)
		return;

	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, array_properties,
				NULL, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_proxy_handlers(context->dbus_client, proxy_get_array,
						NULL, NULL, context);
	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);

	g_main_loop_run(context->main_loop);

	g_dbus_unregister_interface(context->dbus_conn,
					SERVICE_PATH, SERVICE_NAME);

	destroy_context(context);
}

static void proxy_get_uint64(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;
	DBusMessageIter iter;
	guint64 value;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	g_assert(g_dbus_proxy_get_property(proxy, "Number", &iter));
	g_assert(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT64);

	dbus_message_iter_get_basic(&iter, &value);
	g_assert(value == G_MAXUINT64);

	g_dbus_client_unref(context->dbus_client);
}

static gboolean get_uint64(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *data)
{
	guint64 value = G_MAXUINT64;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT64, &value);

	return TRUE;
}

static void client_get_uint64_property(void)
{
	struct context *context = create_context();
	static const GDBusPropertyTable uint64_properties[] = {
		{ "Number", "t", get_uint64 },
		{ },
	};

	if (context == NULL)
		return;

	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, uint64_properties,
				NULL, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_proxy_handlers(context->dbus_client,
						proxy_get_uint64,
						NULL, NULL, context);
	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);

	g_main_loop_run(context->main_loop);

	g_dbus_unregister_interface(context->dbus_conn,
					SERVICE_PATH, SERVICE_NAME);

	destroy_context(context);
}

static void property_set_success(const DBusError *err, void *user_data)
{
	g_assert(!dbus_error_is_set(err));
}

static void proxy_set_string(GDBusProxy *proxy, void *user_data)
{
	DBusMessageIter iter;
	const char *string;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	g_assert(g_dbus_proxy_get_property(proxy, "String", &iter));
	g_assert(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);

	dbus_message_iter_get_basic(&iter, &string);
	g_assert(g_strcmp0(string, "value") == 0);

	string = "value1";
	g_assert(g_dbus_proxy_set_property_basic(proxy, "String",
					DBUS_TYPE_STRING, &string,
					property_set_success, user_data,
					NULL));
}

static void property_string_changed(GDBusProxy *proxy, const char *name,
					DBusMessageIter *iter, void *user_data)
{
	struct context *context = user_data;
	const char *string;

	if (g_test_verbose())
		g_print("property %s changed\n", name);

	g_assert(g_strcmp0(name, "String") == 0);
	g_assert(dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING);

	dbus_message_iter_get_basic(iter, &string);
	g_assert(g_strcmp0(string, "value1") == 0);

	g_dbus_client_unref(context->dbus_client);
}

static void set_string(const GDBusPropertyTable *property,
			DBusMessageIter *iter, GDBusPendingPropertySet id,
			void *data)
{
	struct context *context = data;
	const char *string;

	g_assert(dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING);

	dbus_message_iter_get_basic(iter, &string);
	g_assert(g_strcmp0(string, "value1") == 0);

	g_free(context->data);
	context->data = g_strdup(string);

	g_dbus_emit_property_changed(context->dbus_conn, SERVICE_PATH,
						SERVICE_NAME, "String");

	g_dbus_pending_property_success(id);
}

static void client_set_string_property(void)
{
	struct context *context = create_context();
	static const GDBusPropertyTable string_properties[] = {
		{ "String", "s", get_string, set_string },
		{ },
	};

	if (context == NULL)
		return;

	context->data = g_strdup("value");
	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, string_properties,
				context, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);
	g_dbus_client_set_proxy_handlers(context->dbus_client, proxy_set_string,
						NULL, property_string_changed,
						context);

	g_main_loop_run(context->main_loop);

	g_dbus_unregister_interface(context->dbus_conn,
					SERVICE_PATH, SERVICE_NAME);

	destroy_context(context);
}

static gboolean string_exists(const GDBusPropertyTable *property, void *data)
{
	struct context *context = data;

	return context->data != NULL;
}

static gboolean timeout_test(gpointer user_data)
{
	struct context *context = user_data;

	if (g_test_verbose())
		g_print("timeout triggered\n");

	context->timeout_source = 0;

	g_assert_not_reached();

	return FALSE;
}

static gboolean emit_string_change(void *user_data)
{
	struct context *context = user_data;

	context->data = g_strdup("value1");

	g_dbus_emit_property_changed(context->dbus_conn, SERVICE_PATH,
						SERVICE_NAME, "String");

	context->timeout_source = g_timeout_add_seconds(2, timeout_test,
								context);

	return FALSE;
}

static void proxy_string_changed(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;
	DBusMessageIter iter;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	g_assert(!g_dbus_proxy_get_property(proxy, "String", &iter));

	g_idle_add(emit_string_change, context);
}

static void client_string_changed(void)
{
	struct context *context = create_context();
	static const GDBusPropertyTable string_properties[] = {
		{ "String", "s", get_string, NULL, string_exists },
		{ },
	};

	if (context == NULL)
		return;

	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, string_properties,
				context, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);
	g_dbus_client_set_proxy_handlers(context->dbus_client,
						proxy_string_changed, NULL,
						property_string_changed,
						context);

	g_main_loop_run(context->main_loop);

	g_dbus_unregister_interface(context->dbus_conn,
					SERVICE_PATH, SERVICE_NAME);

	destroy_context(context);
}

static void property_check_order(const DBusError *err, void *user_data)
{
	struct context *context = user_data;
	GDBusProxy *proxy = context->proxy;
	DBusMessageIter iter;
	const char *string;

	g_assert(!dbus_error_is_set(err));

	g_assert(g_dbus_proxy_get_property(proxy, "String", &iter));

	dbus_message_iter_get_basic(&iter, &string);
	g_assert(g_strcmp0(string, "value1") == 0);

	g_dbus_client_unref(context->dbus_client);
}

static void proxy_check_order(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;
	const char *string;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	context->proxy = proxy;
	string = "value1";
	g_assert(g_dbus_proxy_set_property_basic(proxy, "String",
					DBUS_TYPE_STRING, &string,
					property_check_order, context,
					NULL));
}

static void client_check_order(void)
{
	struct context *context = create_context();
	static const GDBusPropertyTable string_properties[] = {
		{ "String", "s", get_string, set_string, string_exists },
		{ },
	};

	if (context == NULL)
		return;

	context->data = g_strdup("value");
	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, string_properties,
				context, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_disconnect_watch(context->dbus_client,
						disconnect_handler, context);
	g_dbus_client_set_proxy_handlers(context->dbus_client,
						proxy_check_order, NULL, NULL,
						context);

	g_main_loop_run(context->main_loop);

	g_dbus_unregister_interface(context->dbus_conn,
					SERVICE_PATH, SERVICE_NAME);

	destroy_context(context);
}

static void proxy_removed(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;

	if (g_test_verbose())
		g_print("proxy removed\n");

	g_main_loop_quit(context->main_loop);
}

static void proxy_set_removed(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	g_assert(g_dbus_proxy_set_removed_watch(proxy, proxy_removed, context));

	context->timeout_source = g_timeout_add_seconds(2, timeout_test,
								context);

	g_dbus_unregister_interface(context->dbus_conn, SERVICE_PATH,
								SERVICE_NAME);
}

static void client_proxy_removed(void)
{
	struct context *context = create_context();
	static const GDBusPropertyTable string_properties[] = {
		{ "String", "s", get_string, set_string, string_exists },
		{ },
	};

	if (context == NULL)
		return;

	g_dbus_register_interface(context->dbus_conn,
				SERVICE_PATH, SERVICE_NAME,
				methods, signals, string_properties,
				context, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME, SERVICE_PATH);

	g_dbus_client_set_proxy_handlers(context->dbus_client,
						proxy_set_removed, NULL, NULL,
						context);

	g_main_loop_run(context->main_loop);

	destroy_context(context);
}

static void proxy_force_disconnect(GDBusProxy *proxy, void *user_data)
{
	struct context *context = user_data;
	DBusConnection *conn = context->data;

	if (g_test_verbose())
		g_print("proxy %s found\n",
					g_dbus_proxy_get_interface(proxy));

	g_assert(g_dbus_proxy_set_removed_watch(proxy, proxy_removed, context));

	context->timeout_source = g_timeout_add_seconds(2, timeout_test,
								context);

	dbus_connection_flush(conn);
	dbus_connection_close(conn);
	context->data = NULL;
}

static void client_force_disconnect(void)
{
	struct context *context = create_context();
	DBusConnection *conn;
	static const GDBusPropertyTable string_properties[] = {
		{ "String", "s", get_string, set_string, string_exists },
		{ },
	};

	if (context == NULL)
		return;

	conn = g_dbus_setup_private(DBUS_BUS_SESSION, SERVICE_NAME1, NULL);
	g_assert(conn != NULL);

	/* Avoid D-Bus library calling _exit() before next test finishes. */
	dbus_connection_set_exit_on_disconnect(conn, FALSE);
	g_dbus_attach_object_manager(conn);
	context->data = conn;

	g_dbus_register_interface(conn, SERVICE_PATH, SERVICE_NAME1,
					methods, signals, string_properties,
					context, NULL);

	context->dbus_client = g_dbus_client_new(context->dbus_conn,
						SERVICE_NAME1, SERVICE_PATH);

	g_dbus_client_set_proxy_handlers(context->dbus_client,
					proxy_force_disconnect, NULL, NULL,
					context);

	g_main_loop_run(context->main_loop);

	destroy_context(context);
}

int main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/gdbus/simple_client", simple_client);

	g_test_add_func("/gdbus/client_connect_disconnect",
						client_connect_disconnect);

	g_test_add_func("/gdbus/client_get_string_property",
						client_get_string_property);

	g_test_add_func("/gdbus/client_get_boolean_property",
						client_get_boolean_property);

	g_test_add_func("/gdbus/client_get_uint64_property",
						client_get_uint64_property);

	g_test_add_func("/gdbus/client_get_array_property",
						client_get_array_property);

	g_test_add_func("/gdbus/client_get_dict_property",
						client_get_dict_property);

	g_test_add_func("/gdbus/client_set_string_property",
						client_set_string_property);

	g_test_add_func("/gdbus/client_string_changed",
						client_string_changed);

	g_test_add_func("/gdbus/client_check_order", client_check_order);

	g_test_add_func("/gdbus/client_proxy_removed", client_proxy_removed);

	g_test_add_func("/gdbus/client_force_disconnect",
						client_force_disconnect);

	return g_test_run();
}