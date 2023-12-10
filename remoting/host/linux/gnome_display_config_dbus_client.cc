// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_display_config_dbus_client.h"

#include <string>
#include <utility>

#include "base/ranges/algorithm.h"
#include "remoting/base/logging.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"

namespace remoting {

namespace {

constexpr char kDisplayConfigInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
constexpr char kDisplayConfigObjectPath[] = "/org/gnome/Mutter/DisplayConfig";

std::string VariantToString(GVariant* variant) {
  webrtc::Scoped<char> print_result(g_variant_print(variant, FALSE));
  if (print_result) {
    return print_result.get();
  } else {
    return std::string();
  }
}

}  // namespace

GnomeDisplayConfigDBusClient::GnomeDisplayConfigDBusClient() {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GnomeDisplayConfigDBusClient::~GnomeDisplayConfigDBusClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cancellable_) {
    g_cancellable_cancel(cancellable_.get());
  }
}

void GnomeDisplayConfigDBusClient::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  caller_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  cancellable_ = TakeGObject(g_cancellable_new());
  g_bus_get(G_BUS_TYPE_SESSION, cancellable_.get(),
            &GnomeDisplayConfigDBusClient::OnDBusGetReply, this);
}

void GnomeDisplayConfigDBusClient::GetMonitorsConfig(
    GnomeDisplayConfigDBusClient::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!dbus_connection_) {
    // The DBus connection is not yet made. When the connection is made,
    // OnDBusGet() will check if there is any pending callback. If so, it
    // will trigger a new call to the DBus GetCurrentState() method.
    pending_callback_ = std::move(callback);
    return;
  }

  bool need_new_call = pending_callback_.is_null();
  pending_callback_ = std::move(callback);
  if (need_new_call) {
    CallDBusGetCurrentState();
  }
}

void GnomeDisplayConfigDBusClient::ApplyMonitorsConfig(
    GnomeDisplayConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedGVariant parameters = config.BuildMonitorsConfigParameters();
  HOST_LOG << "Applying monitors config: " << VariantToString(parameters.get());
  g_dbus_connection_call(
      dbus_connection_.get(), kDisplayConfigInterfaceName,
      kDisplayConfigObjectPath, kDisplayConfigInterfaceName,
      "ApplyMonitorsConfig", parameters.get(),
      /*reply_type=*/nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
      /*timeout_msec=*/-1, cancellable_.get(),
      &GnomeDisplayConfigDBusClient::OnApplyMonitorsConfigReply, this);
}

void GnomeDisplayConfigDBusClient::FakeDisplayConfigForTest(
    ScopedGVariant config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnDisplayConfigCurrentState(std::move(config));
}

// static
void GnomeDisplayConfigDBusClient::OnDBusGetReply(GObject* object,
                                                  GAsyncResult* result,
                                                  gpointer user_data) {
  webrtc::Scoped<GError> error;
  ScopedGObject<GDBusConnection> dbus_connection =
      TakeGObject(g_bus_get_finish(result, error.receive()));
  if (!dbus_connection) {
    LOG(ERROR) << "Failed to connect to the D-Bus session bus: "
               << error->message;
    return;
  }

  auto* that = static_cast<GnomeDisplayConfigDBusClient*>(user_data);
  that->caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GnomeDisplayConfigDBusClient::OnDBusGet,
                                that->weak_ptr_, std::move(dbus_connection)));
}

// static
void GnomeDisplayConfigDBusClient::OnDisplayConfigCurrentStateReply(
    GObject* object,
    GAsyncResult* result,
    gpointer user_data) {
  auto* connection = reinterpret_cast<GDBusConnection*>(object);
  webrtc::Scoped<GError> error;
  ScopedGVariant config = TakeGVariant(
      g_dbus_connection_call_finish(connection, result, error.receive()));

  auto* that = static_cast<GnomeDisplayConfigDBusClient*>(user_data);

  if (!config) {
    LOG(ERROR) << "Failed to get current display configuration: "
               << error->message;
    that->caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GnomeDisplayConfigDBusClient::OnDisplayConfigCurrentStateError,
            that->weak_ptr_));
    return;
  }

  that->caller_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GnomeDisplayConfigDBusClient::OnDisplayConfigCurrentState,
                     that->weak_ptr_, std::move(config)));
}

// static
void GnomeDisplayConfigDBusClient::OnApplyMonitorsConfigReply(
    GObject* source_object,
    GAsyncResult* result,
    gpointer user_data) {
  auto* connection = reinterpret_cast<GDBusConnection*>(source_object);
  webrtc::Scoped<GError> error;
  ScopedGVariant method_result = TakeGVariant(
      g_dbus_connection_call_finish(connection, result, error.receive()));
  if (!method_result) {
    LOG(ERROR) << "Failed to apply monitors config: " << error->message;
  }
}

void GnomeDisplayConfigDBusClient::CallDBusGetCurrentState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dbus_connection_.get());
  g_dbus_connection_call(
      dbus_connection_.get(), kDisplayConfigInterfaceName,
      kDisplayConfigObjectPath, kDisplayConfigInterfaceName, "GetCurrentState",
      /*parameters=*/nullptr,
      /*reply_type=*/nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
      /*timeout_msec=*/-1, cancellable_.get(),
      &GnomeDisplayConfigDBusClient::OnDisplayConfigCurrentStateReply, this);
}

void GnomeDisplayConfigDBusClient::OnDBusGet(
    ScopedGObject<GDBusConnection> dbus_connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dbus_connection_ = std::move(dbus_connection);
  HOST_LOG << "Got session D-Bus";

  if (pending_callback_) {
    CallDBusGetCurrentState();
  }
}

void GnomeDisplayConfigDBusClient::OnDisplayConfigCurrentState(
    ScopedGVariant config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string pretty_config = VariantToString(config.get());
  if (!pretty_config.empty()) {
    HOST_LOG << "Got current display config: " << pretty_config;
  } else {
    LOG(ERROR) << "Unable to print display config";
  }

  GnomeDisplayConfig display_config;

  webrtc::Scoped<GVariantIter> monitors;
  webrtc::Scoped<GVariantIter> logical_monitors;
  webrtc::Scoped<GVariant> properties;

  constexpr char kCurrentStateFormat[] =
      "(u"                             // serial
      "a((ssss)a(siiddada{sv})a{sv})"  // monitors
      "a(iiduba(ssss)a{sv})"           // logical_monitors
      "@a{sv})";                       // properties

  if (!g_variant_check_format_string(config.get(), kCurrentStateFormat,
                                     /*copy_only=*/FALSE)) {
    LOG(ERROR) << __func__ << " : config has incorrect type.";
    pending_callback_.Reset();
    return;
  }

  g_variant_get(config.get(), kCurrentStateFormat, &display_config.serial,
                monitors.receive(), logical_monitors.receive(),
                properties.receive());

  while (true) {
    webrtc::Scoped<GVariant> monitor;
    if (!g_variant_iter_next(monitors.get(), "@((ssss)a(siiddada{sv})a{sv})",
                             monitor.receive())) {
      break;
    }
    display_config.AddMonitorFromVariant(monitor.get());
  }

  while (true) {
    webrtc::Scoped<GVariant> logical_monitor;
    if (!g_variant_iter_next(logical_monitors.get(), "@(iiduba(ssss)a{sv})",
                             logical_monitor.receive())) {
      break;
    }
    display_config.AddLogicalMonitorFromVariant(logical_monitor.get());
  }

  gboolean global_scale_required = FALSE;
  g_variant_lookup(properties.get(), "global-scale-required", "b",
                   &global_scale_required);
  display_config.global_scale_required = global_scale_required;
  HOST_LOG << "Global scale required: "
           << (global_scale_required ? "yes" : "no");

  std::move(pending_callback_).Run(display_config);
}

void GnomeDisplayConfigDBusClient::OnDisplayConfigCurrentStateError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reset the callback, so that subsequent calls to GetMonitorsConfig() will
  // actually send a D-Bus request.
  pending_callback_.Reset();
}

}  // namespace remoting
