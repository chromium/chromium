// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gdbus_connection_ref.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/linux/gdbus_fd_list.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "ui/base/glib/scoped_gobject.h"

namespace remoting {

GDBusConnectionRef::GDBusConnectionRef() = default;
GDBusConnectionRef::GDBusConnectionRef(const GDBusConnectionRef& other) =
    default;
GDBusConnectionRef::GDBusConnectionRef(GDBusConnectionRef&& other) = default;
GDBusConnectionRef& GDBusConnectionRef::operator=(
    const GDBusConnectionRef& other) = default;
GDBusConnectionRef& GDBusConnectionRef::operator=(GDBusConnectionRef&& other) =
    default;
GDBusConnectionRef::~GDBusConnectionRef() = default;

GDBusConnectionRef::GDBusConnectionRef(
    ScopedGObject<GDBusConnection> connection)
    : connection_(connection) {}

bool GDBusConnectionRef::is_initialized() const {
  return connection_;
}

// static
void GDBusConnectionRef::CreateForSessionBus(CreateCallback callback) {
  return CreateForBus(G_BUS_TYPE_SESSION, std::move(callback));
}

// static
void GDBusConnectionRef::CreateForSystemBus(CreateCallback callback) {
  return CreateForBus(G_BUS_TYPE_SYSTEM, std::move(callback));
}

// static
void GDBusConnectionRef::CreateForBus(GBusType bus, CreateCallback callback) {
  auto* bound_callback = new CreateCallback(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback)));

  // May run on a different sequence.
  auto on_complete = [](GObject* source, GAsyncResult* result,
                        gpointer user_data) {
    auto callback =
        base::WrapUnique(static_cast<decltype(bound_callback)>(user_data));
    GError* error = nullptr;
    GDBusConnection* connection = g_bus_get_finish(result, &error);
    // Will post back to the proper sequence thanks to BindPostTask above.
    if (connection) {
      std::move(*callback).Run(
          base::ok(GDBusConnectionRef(TakeGObject(connection))));
    } else {
      std::move(*callback).Run(base::unexpected(Loggable(
          FROM_HERE,
          base::StrCat({"Failed to connect to bus: ", error->message}))));
      g_error_free(error);
    }
  };

  g_bus_get(bus, nullptr, on_complete, bound_callback);
}

void GDBusConnectionRef::CallInternal(const char* bus_name,
                                      gvariant::ObjectPathCStr object_path,
                                      const char* interface_name,
                                      const char* method_name,
                                      const GVariantRef<"r">& arguments,
                                      GDBusFdList fds,
                                      CallFdCallback<GVariantRef<"r">> callback,
                                      GDBusCallFlags flags,
                                      gint timeout_msec) const {
  auto* bound_callback =
      new CallFdCallback<GVariantRef<"r">>(base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback)));

  // May run on a different sequence.
  auto on_complete = [](GObject* source, GAsyncResult* result,
                        gpointer user_data) {
    auto callback =
        base::WrapUnique(static_cast<decltype(bound_callback)>(user_data));
    GUnixFDList* fd_list = nullptr;
    GError* error = nullptr;
    GVariant* variant = g_dbus_connection_call_with_unix_fd_list_finish(
        G_DBUS_CONNECTION(source), &fd_list, result, &error);
    GDBusFdList out_fds;
    if (fd_list != nullptr) {
      out_fds = GDBusFdList::StealFromGUnixFDList(fd_list);
      g_object_unref(fd_list);
    }
    // Will post back to the proper sequence thanks to BindPostTask above.
    if (variant != nullptr) {
      std::move(*callback).Run(std::pair(
          GVariantRef<"r">::TakeUnchecked(variant), std::move(out_fds)));
    } else {
      std::move(*callback).Run(base::unexpected(
          Loggable(FROM_HERE,
                   base::StrCat({"Error invoking method: ", error->message}))));
      g_error_free(error);
    }
  };

  g_dbus_connection_call_with_unix_fd_list(
      connection_, bus_name, object_path.c_str(), interface_name, method_name,
      arguments.raw(), G_VARIANT_TYPE_TUPLE, flags, timeout_msec,
      std::move(fds).IntoGUnixFDList(), nullptr, on_complete, bound_callback);
}

GDBusConnectionRef::SignalSubscription::~SignalSubscription() {
  g_dbus_connection_signal_unsubscribe(connection_.raw(), subscription_id_);
}

GDBusConnectionRef::SignalSubscription::SignalSubscription(
    GDBusConnectionRef connection,
    const char* sender,
    std::optional<gvariant::ObjectPathCStr> object_path,
    const char* interface_name,
    const char* signal_name,
    DetailedSignalCallback<GVariantRef<"r">> callback)
    : connection_(std::move(connection)),
      callback_(std::move(callback)),
      weak_factory_(this) {
  auto* bound_callback = new DetailedSignalCallback<GVariantRef<"r">>(
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindRepeating(&SignalSubscription::OnSignal,
                                             weak_factory_.GetWeakPtr())));

  // May run on a different sequence.
  auto on_signal = [](GDBusConnection* connection, const gchar* sender_name,
                      const gchar* object_path, const gchar* interface_name,
                      const gchar* signal_name, GVariant* arguments,
                      gpointer user_data) {
    auto* callback = static_cast<decltype(bound_callback)>(user_data);
    // Will post back to the proper sequence thanks to BindPostTask above.
    callback->Run(sender_name ? std::string(sender_name) : std::string(),
                  gvariant::ObjectPath::TryFrom(object_path).value(),
                  interface_name, signal_name,
                  GVariantRef<"r">::RefSinkUnchecked(arguments));
  };

  auto free_func = [](gpointer data) {
    delete static_cast<decltype(bound_callback)>(data);
  };

  subscription_id_ = g_dbus_connection_signal_subscribe(
      connection_.raw(), sender, interface_name, signal_name,
      object_path ? object_path->c_str() : nullptr, nullptr,
      G_DBUS_SIGNAL_FLAGS_NONE, on_signal, bound_callback, free_func);
}

void GDBusConnectionRef::SignalSubscription::OnSignal(
    std::string sender,
    gvariant::ObjectPath object_path,
    std::string interface_name,
    std::string signal_name,
    GVariantRef<"r"> arguments) {
  callback_.Run(std::move(sender), std::move(object_path),
                std::move(interface_name), std::move(signal_name),
                std::move(arguments));
}

}  // namespace remoting
