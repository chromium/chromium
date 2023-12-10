// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/remote_desktop_portal.h"

#include <glib-object.h>

#include <utility>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/clipboard_portal.h"
#include "remoting/host/linux/wayland_manager.h"
#include "third_party/webrtc/modules/portal/xdg_desktop_portal_utils.h"

namespace remoting::xdg_portal {
namespace {

constexpr char kRemoteDesktopInterfaceName[] =
    "org.freedesktop.portal.RemoteDesktop";
constexpr char kPortalPrefix[] = "remotedesktop";

using webrtc::Scoped;
using webrtc::xdg_portal::kDesktopObjectPath;
using webrtc::xdg_portal::kSessionInterfaceName;
using webrtc::xdg_portal::RequestResponse;

void UnsubscribeSignalHandler(GDBusConnection* connection, guint& signal_id) {
  if (signal_id) {
    g_dbus_connection_signal_unsubscribe(connection, signal_id);
    signal_id = 0;
  }
}

}  // namespace

RemoteDesktopPortal::RemoteDesktopPortal(
    webrtc::ScreenCastPortal::PortalNotifier* notifier,
    bool prefer_cursor_embedded)
    : notifier_(notifier) {
  screencast_portal_ = std::make_unique<webrtc::ScreenCastPortal>(
      webrtc::CaptureType::kScreen, this, OnScreenCastPortalProxyRequested,
      OnSourcesRequestResponseSignal, this, prefer_cursor_embedded);
  clipboard_portal_ = std::make_unique<xdg_portal::ClipboardPortal>(this);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RemoteDesktopPortal::~RemoteDesktopPortal() {
  Stop();
}

void RemoteDesktopPortal::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (context_) {
    g_main_context_pop_thread_default(context_);
    g_main_context_unref(context_);
  }
  if (screencast_portal_) {
    screencast_portal_.reset();
  }
  if (clipboard_portal_) {
    clipboard_portal_.reset();
  }
  UnsubscribeSignalHandlers();
  webrtc::xdg_portal::TearDownSession(std::move(session_handle_), proxy_,
                                      cancellable_, connection_);
  session_handle_.clear();
  proxy_ = nullptr;
  cancellable_ = nullptr;
  connection_ = nullptr;
}

void RemoteDesktopPortal::UnsubscribeSignalHandlers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnsubscribeSignalHandler(connection_, start_request_signal_id_);
  UnsubscribeSignalHandler(connection_, session_request_signal_id_);
  UnsubscribeSignalHandler(connection_, devices_request_signal_id_);
  UnsubscribeSignalHandler(connection_, session_closed_signal_id_);
}

void RemoteDesktopPortal::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We create and run our own thread-default context so that all the callbacks
  // are called on this thread rather than going to the main thread.
  context_ = g_main_context_new();
  g_main_context_push_thread_default(context_);

  HOST_LOG << "Starting screen cast portal";
  cancellable_ = g_cancellable_new();
  screencast_portal_->SetSessionDetails({.cancellable = cancellable_});
  screencast_portal_->Start();
  clipboard_portal_->SetSessionDetails({.cancellable = cancellable_});
  clipboard_portal_->Start();

  HOST_LOG << "Starting remote desktop portal";
  webrtc::xdg_portal::RequestSessionProxy(kRemoteDesktopInterfaceName,
                                          OnProxyRequested, cancellable_, this);

  // OpenPipewireRemote (defined on the screencast portal) is the last
  // asynchronous call in the combined portal setup so we wait for the
  // screencast portal to go into either failed/succeeded state before stopping
  // our loop.
  while (context_ && screencast_portal_status_ == RequestResponse::kUnknown) {
    g_main_context_iteration(context_, /*may_block=*/true);
  }
  if (context_) {
    g_main_context_pop_thread_default(context_);
    g_main_context_unref(context_);
    context_ = nullptr;
  }

  HOST_LOG << "Session setup finished";
}

uint32_t RemoteDesktopPortal::pipewire_stream_node_id() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(screencast_portal_);
  return screencast_portal_->pipewire_stream_node_id();
}

// static
void RemoteDesktopPortal::OnProxyRequested(GObject* gobject,
                                           GAsyncResult* result,
                                           gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);
  DCHECK(that);
  Scoped<GError> error;
  // We have to acquire the proxy object via "_finish" here since otherwise
  // the result will be freed upon return from this callback (and before newly
  // posted task on the task runner can start/finish).
  GDBusProxy* proxy = g_dbus_proxy_new_finish(result, error.receive());
  if (!proxy) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      return;
    }
    LOG(ERROR) << "Failed to get a proxy for the portal: " << error->message;
    that->OnPortalDone(RequestResponse::kError);
    return;
  }
  that->RequestSession(proxy);
}

// static
void RemoteDesktopPortal::OnScreenCastPortalProxyRequested(GObject* /*object*/,
                                                           GAsyncResult* result,
                                                           gpointer user_data) {
  webrtc::ScreenCastPortal* that =
      static_cast<webrtc::ScreenCastPortal*>(user_data);
  DCHECK(that);

  Scoped<GError> error;
  GDBusProxy* proxy = g_dbus_proxy_new_finish(result, error.receive());
  if (!proxy) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      return;
    }
    LOG(ERROR) << "Failed to create a proxy for the screen cast portal: "
               << error->message;
    that->OnPortalDone(RequestResponse::kError);
    return;
  }
  that->SetSessionDetails({.proxy = proxy});

  HOST_LOG << "Successfully created proxy for the screen cast portal.";
}

void RemoteDesktopPortal::RequestSession(GDBusProxy* proxy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!proxy_);
  DCHECK(!connection_);
  proxy_ = proxy;
  connection_ = g_dbus_proxy_get_connection(proxy);
  webrtc::xdg_portal::SetupSessionRequestHandlers(
      kPortalPrefix, OnSessionRequested, OnSessionRequestResponseSignal,
      connection_, proxy_, cancellable_, portal_handle_,
      session_request_signal_id_, this);
}

// static
void RemoteDesktopPortal::OnSessionRequested(GDBusProxy* proxy,
                                             GAsyncResult* result,
                                             gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);
  that->OnSessionRequestResult(proxy, result);
}

// static
void RemoteDesktopPortal::OnDevicesRequested(GObject* object,
                                             GAsyncResult* result,
                                             gpointer user_data) {
  auto* proxy = reinterpret_cast<GDBusProxy*>(object);
  auto* that = static_cast<RemoteDesktopPortal*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    LOG(ERROR) << "Failed to select the devices: " << error->message;
    return;
  }

  Scoped<gchar> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    LOG(ERROR) << "Failed to initialize the remote desktop session.";
    if (that->devices_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->devices_request_signal_id_);
      that->devices_request_signal_id_ = 0;
    }
    return;
  }
  HOST_LOG << "Subscribed to devices signal.";
}

void RemoteDesktopPortal::RequestSources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  screencast_portal_->SourcesRequest();
}

void RemoteDesktopPortal::RequestClipboard() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clipboard_portal_->RequestClipboard();
}

// static
void RemoteDesktopPortal::OnDevicesRequestImpl(GDBusConnection* connection,
                                               const gchar* sender_name,
                                               const gchar* object_path,
                                               const gchar* interface_name,
                                               const gchar* signal_name,
                                               GVariant* parameters,
                                               gpointer user_data) {
  HOST_LOG << "Received device selection signal from session.";
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  guint32 portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    LOG(ERROR) << "Failed to select devices for the remote desktop session.";
    return;
  }

  that->RequestClipboard();
}

void RemoteDesktopPortal::SelectDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GVariantBuilder builder;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&builder, "{sv}", "multiple",
                        g_variant_new_boolean(false));

  auto name = base::StringPrintf(
      "%s%d", kPortalPrefix, base::RandInt(0, std::numeric_limits<int>::max()));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(name.c_str()));
  devices_handle_ =
      webrtc::xdg_portal::PrepareSignalHandle(name.c_str(), connection_);
  devices_request_signal_id_ = webrtc::xdg_portal::SetupRequestResponseSignal(
      devices_handle_.c_str(), OnDevicesRequestImpl, this, connection_);

  HOST_LOG << "Selecting devices from the remote desktop session.";
  g_dbus_proxy_call(
      proxy_, "SelectDevices",
      g_variant_new("(oa{sv})", session_handle_.c_str(), &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_, OnDevicesRequested,
      this);
}

// static
void RemoteDesktopPortal::OnSessionRequestResponseSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  that->RegisterSessionClosedSignalHandler(
      OnSessionClosedSignal, parameters, that->connection_,
      that->session_handle_, that->session_closed_signal_id_);
  that->screencast_portal_->SetSessionDetails(
      {.session_handle = that->session_handle_});
  that->clipboard_portal_->SetSessionDetails(
      {.session_handle = that->session_handle_});

  that->SelectDevices();
}

// static
void RemoteDesktopPortal::OnSessionClosedSignal(GDBusConnection* connection,
                                                const char* sender_name,
                                                const char* object_path,
                                                const char* interface_name,
                                                const char* signal_name,
                                                GVariant* parameters,
                                                gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  HOST_LOG << "Received closed signal from session.";

  // Unsubscribe from the signal and free the session handle to avoid calling
  // Session::Close from the destructor since it's already closed
  g_dbus_connection_signal_unsubscribe(that->connection_,
                                       that->session_closed_signal_id_);
}

// static
void RemoteDesktopPortal::OnSourcesRequestResponseSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  HOST_LOG << "Received sources signal from session.";

  uint32_t portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    LOG(ERROR) << "Failed to select sources for the remote desktop session.";
    that->OnPortalDone(RequestResponse::kError);
    return;
  }
  that->StartRequest();
}

void RemoteDesktopPortal::StartRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  webrtc::xdg_portal::StartSessionRequest(
      kPortalPrefix, session_handle_, OnStartRequestResponseSignal,
      OnStartRequested, proxy_, connection_, cancellable_,
      start_request_signal_id_, start_handle_, this);
}

// static
void RemoteDesktopPortal::OnStartRequested(GDBusProxy* proxy,
                                           GAsyncResult* result,
                                           gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);
  that->OnStartRequestResult(proxy, result);
}

// static
void RemoteDesktopPortal::OnStartRequestResponseSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);
  HOST_LOG << "Start signal received.";
  uint32_t portal_response;
  Scoped<GVariant> response_data;
  Scoped<GVariantIter> iter;
  g_variant_get(parameters, "(u@a{sv})", &portal_response,
                response_data.receive());
  if (portal_response || !response_data) {
    LOG(ERROR) << "Failed to start the remote desktop session.";
    return;
  }

  if (g_variant_lookup(response_data.get(), "streams", "a(ua{sv})",
                       iter.receive())) {
    Scoped<GVariant> variant;

    while (g_variant_iter_next(iter.get(), "@(ua{sv})", variant.receive())) {
      uint32_t stream_id;
      Scoped<GVariant> options;

      g_variant_get(variant.get(), "(u@a{sv})", &stream_id, options.receive());
      DCHECK(options.get());

      that->screencast_portal_->SetSessionDetails(
          {.pipewire_stream_node_id = stream_id});
      HOST_LOG << "Pipewire stream node id has been set for the portal";
      break;
    }
  }

  that->screencast_portal_->OpenPipeWireRemote();
  that->OnPortalDone(RequestResponse::kSuccess);
  HOST_LOG << "Remote desktop portal start response successful";
}

void RemoteDesktopPortal::OnPortalDone(RequestResponse result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Remote desktop portal setup is done: "
            << webrtc::xdg_portal::RequestResponseToString(result);
  if (result != RequestResponse::kSuccess) {
    Stop();
  }
}

webrtc::xdg_portal::SessionDetails RemoteDesktopPortal::GetSessionDetails() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Getting session details from the wayland capturer";
  return {proxy_.get(), cancellable_.get(), session_handle_,
          pipewire_stream_node_id()};
}

void RemoteDesktopPortal::OnScreenCastRequestResult(RequestResponse result,
                                                    uint32_t stream_node_id,
                                                    int fd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  screencast_portal_status_ = result;
  notifier_->OnScreenCastRequestResult(result, stream_node_id, fd);
}

void RemoteDesktopPortal::OnScreenCastSessionClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  notifier_->OnScreenCastSessionClosed();
}

void RemoteDesktopPortal::OnClipboardPortalDone(
    webrtc::xdg_portal::RequestResponse result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clipboard_portal_status_ = result;

  if (result == RequestResponse::kSuccess) {
    WaylandManager::Get()->OnClipboardMetadata(
        {.session_details = clipboard_portal_->GetSessionDetails()});
  }

  RequestSources();
}

}  // namespace remoting::xdg_portal
