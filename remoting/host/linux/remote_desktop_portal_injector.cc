// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/remote_desktop_portal_injector.h"

#include <glib-object.h>

#include <utility>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/portal_request_response.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/scoped_glib.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"

namespace remoting::xdg_portal {

using webrtc::Scoped;

RemoteDesktopPortalInjector::RemoteDesktopPortalInjector() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RemoteDesktopPortalInjector::~RemoteDesktopPortalInjector() {}

// static
void RemoteDesktopPortalInjector::ValidateGDPBusProxyResult(
    GObject* proxy,
    GAsyncResult* result,
    gpointer user_data) {
  RemoteDesktopPortalInjector* that =
      static_cast<RemoteDesktopPortalInjector*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  Scoped<GError> error;
  Scoped<GVariant> variant(g_dbus_proxy_call_finish(
      reinterpret_cast<GDBusProxy*>(proxy), result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      return;
    }
    LOG(ERROR) << "Error in input injection";
  }
}

void RemoteDesktopPortalInjector::InjectMouseButton(int code, bool pressed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call(proxy_, "NotifyPointerButton",
                    g_variant_new("(oa{sv}iu)", session_handle_.c_str(),
                                  &builder, code, pressed),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    ValidateGDPBusProxyResult, this);
}

void RemoteDesktopPortalInjector::InjectMouseScroll(int axis, int steps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call(proxy_, "NotifyPointerAxisDiscrete",
                    g_variant_new("(oa{sv}ui)", session_handle_.c_str(),
                                  &builder, axis, steps),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    ValidateGDPBusProxyResult, this);
}

void RemoteDesktopPortalInjector::MovePointerBy(int delta_x, int delta_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call(
      proxy_, "NotifyPointerMotion",
      g_variant_new("(oa{sv}dd)", session_handle_.c_str(), &builder,
                    static_cast<double>(delta_x), static_cast<double>(delta_y)),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      ValidateGDPBusProxyResult, this);
}

void RemoteDesktopPortalInjector::MovePointerTo(int x, int y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  VLOG(6) << "session handle: " << session_handle_
          << ", stream node id: " << pipewire_stream_node_id_;
  g_dbus_proxy_call(
      proxy_, "NotifyPointerMotionAbsolute",
      g_variant_new("(oa{sv}udd)", session_handle_.c_str(), &builder,
                    pipewire_stream_node_id_, static_cast<double>(x),
                    static_cast<double>(y)),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      ValidateGDPBusProxyResult, this);
}

void RemoteDesktopPortalInjector::InjectKeyPress(int code,
                                                 bool pressed,
                                                 bool is_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  VLOG(6) << "session handle: " << session_handle_;
  g_dbus_proxy_call(proxy_,
                    is_code ? "NotifyKeyboardKeycode" : "NotifyKeyboardKeysym",
                    g_variant_new("(oa{sv}iu)", session_handle_.c_str(),
                                  &builder, code, pressed),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    ValidateGDPBusProxyResult, this);
}

void RemoteDesktopPortalInjector::SetSessionDetails(
    webrtc::xdg_portal::SessionDetails session_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Desktop portal session details received";
  proxy_ = session_details.proxy;
  cancellable_ = session_details.cancellable;
  session_handle_ = session_details.session_handle;
  pipewire_stream_node_id_ = session_details.pipewire_stream_node_id;
}

}  // namespace remoting::xdg_portal
