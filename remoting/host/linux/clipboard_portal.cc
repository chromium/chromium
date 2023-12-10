// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/clipboard_portal.h"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib-object.h>

#include "base/check.h"
#include "remoting/base/logging.h"
#include "third_party/webrtc/modules/portal/portal_request_response.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"
#include "third_party/webrtc/modules/portal/xdg_desktop_portal_utils.h"

namespace remoting::xdg_portal {
namespace {

constexpr char kClipboardInterfaceName[] = "org.freedesktop.portal.Clipboard";

using webrtc::Scoped;
using webrtc::xdg_portal::RequestResponse;
using webrtc::xdg_portal::RequestSessionProxy;
using webrtc::xdg_portal::SessionDetails;

}  // namespace

ClipboardPortal::ClipboardPortal(PortalNotifier* notifier)
    : notifier_(notifier) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ClipboardPortal::~ClipboardPortal() {
  if (cancellable_) {
    g_cancellable_cancel(cancellable_);
    g_object_unref(cancellable_);
  }

  // connection_ is owned by proxy_ and does not need to be freed.
  if (proxy_) {
    g_object_unref(proxy_);
  }
}

void ClipboardPortal::SetSessionDetails(const SessionDetails& session_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Desktop portal session details received on the clipboard portal";

  if (session_details.proxy) {
    proxy_ = session_details.proxy;
    connection_ = g_dbus_proxy_get_connection(proxy_);
  }
  if (session_details.cancellable) {
    cancellable_ = session_details.cancellable;
  }
  if (!session_details.session_handle.empty()) {
    session_handle_ = session_details.session_handle;
  }
}

void ClipboardPortal::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancellable_ = g_cancellable_new();
  RequestSessionProxy(kClipboardInterfaceName, OnClipboardPortalProxyRequested,
                      cancellable_, this);
}

// static
void ClipboardPortal::OnClipboardPortalProxyRequested(GObject* /* object */,
                                                      GAsyncResult* result,
                                                      gpointer user_data) {
  ClipboardPortal* that = static_cast<ClipboardPortal*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  Scoped<GError> error;
  GDBusProxy* proxy = g_dbus_proxy_new_finish(result, error.receive());
  if (!proxy) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      return;
    }
    LOG(ERROR) << "Failed to create a proxy for the clipboard portal: "
               << error->message;
    that->OnPortalDone(RequestResponse::kError);
    return;
  }
  that->SetSessionDetails({.proxy = proxy});

  HOST_LOG << "Successfully created proxy for clipboard portal.";
}

void ClipboardPortal::OnPortalDone(RequestResponse result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Clipboard portal is done setting up.";

  notifier_->OnClipboardPortalDone(result);
}

void ClipboardPortal::RequestClipboard() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Request clipboard for the remote desktop session.";

  GVariantBuilder options_builder;
  g_variant_builder_init(&options_builder, G_VARIANT_TYPE_VARDICT);

  g_dbus_proxy_call(
      proxy_, "RequestClipboard",
      g_variant_new("(oa{sv})", session_handle_.c_str(), &options_builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_, OnClipboardRequest,
      this);
}

// static
void ClipboardPortal::OnClipboardRequest(GObject* object,
                                         GAsyncResult* result,
                                         gpointer user_data) {
  auto* proxy = reinterpret_cast<GDBusProxy*>(object);
  auto* that = static_cast<ClipboardPortal*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    LOG(ERROR) << "Failed to request clipboard: " << error->message;
    that->OnPortalDone(RequestResponse::kError);
    return;
  }

  that->OnPortalDone(RequestResponse::kSuccess);
}

SessionDetails ClipboardPortal::GetSessionDetails() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  HOST_LOG << "Getting session details from the wayland clipboard";

  return {.proxy = proxy_.get(),
          .cancellable = cancellable_.get(),
          .session_handle = session_handle_};
}

}  // namespace remoting::xdg_portal
