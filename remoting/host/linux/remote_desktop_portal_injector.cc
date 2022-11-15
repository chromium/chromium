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

void RemoteDesktopPortalInjector::InjectMouseButton(int code, bool pressed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  Scoped<GError> error;
  g_dbus_proxy_call_sync(proxy_, "NotifyPointerButton",
                         g_variant_new("(oa{sv}iu)", session_handle_.c_str(),
                                       &builder, code, pressed),
                         G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                         error.receive());
  if (error.get()) {
    LOG(ERROR) << "Failed to " << (pressed ? "press" : "release")
               << " the mouse button, button code: " << code
               << ", error: " << error->message;
  }
}

void RemoteDesktopPortalInjector::InjectMouseScroll(int axis, int steps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  Scoped<GError> error;
  g_dbus_proxy_call_sync(proxy_, "NotifyPointerAxisDiscrete",
                         g_variant_new("(oa{sv}ui)", session_handle_.c_str(),
                                       &builder, axis, steps),
                         G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                         error.receive());
  if (error.get()) {
    LOG(ERROR) << "Failed to scroll "
               << (axis == ScrollType::VERTICAL_SCROLL ? "vertically"
                                                       : "horizontally")
               << ". Error: " << error->message;
  }
}

void RemoteDesktopPortalInjector::MovePointerBy(int delta_x, int delta_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  Scoped<GError> error;
  g_dbus_proxy_call_sync(
      proxy_, "NotifyPointerMotion",
      g_variant_new("(oa{sv}dd)", session_handle_.c_str(), &builder,
                    static_cast<double>(delta_x), static_cast<double>(delta_y)),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_, error.receive());
  if (error.get()) {
    LOG(ERROR) << "Failed to move pointer by delta_x: " << delta_x
               << ", delta_y: " << delta_y << ", error: " << error->message;
  }
}

void RemoteDesktopPortalInjector::MovePointerTo(int x, int y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  Scoped<GError> error;
  VLOG(6) << "session handle: " << session_handle_
          << ", stream node id: " << pipewire_stream_node_id_;
  g_dbus_proxy_call_sync(
      proxy_, "NotifyPointerMotionAbsolute",
      g_variant_new("(oa{sv}udd)", session_handle_.c_str(), &builder,
                    pipewire_stream_node_id_, static_cast<double>(x),
                    static_cast<double>(y)),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_, error.receive());
  if (error.get()) {
    LOG(ERROR) << "Failed to move pointer to x: " << x << ", y: " << y
               << ", error: " << error->message;
  }
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
  Scoped<GError> error;
  VLOG(6) << "session handle: " << session_handle_;
  g_dbus_proxy_call_sync(
      proxy_, is_code ? "NotifyKeyboardKeycode" : "NotifyKeyboardKeysym",
      g_variant_new("(oa{sv}iu)", session_handle_.c_str(), &builder, code,
                    pressed),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_, error.receive());
  if (error.get()) {
    LOG(ERROR) << "Failed to inject key press";
  }
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
