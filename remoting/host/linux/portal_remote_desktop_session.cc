// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/portal_remote_desktop_session.h"

#include <gio/gio.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/delegating_desktop_display_info_monitor.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_portal_RemoteDesktop.h"
#include "remoting/host/linux/ei_sender_session.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_dict_builder.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"
#include "remoting/host/linux/portal_display_info_loader.h"
#include "remoting/host/linux/portal_utils.h"
#include "remoting/host/polling_desktop_display_info_monitor.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"

namespace remoting {

// static
PortalRemoteDesktopSession* PortalRemoteDesktopSession::GetInstance() {
  static base::NoDestructor<PortalRemoteDesktopSession> instance;
  return instance.get();
}

PortalRemoteDesktopSession::PortalRemoteDesktopSession() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PortalRemoteDesktopSession::~PortalRemoteDesktopSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PortalRemoteDesktopSession::Init(InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialization_state_ == InitializationState::kInitialized) {
    HOST_LOG << "Portal remote desktop session is already initialized. "
             << "Posting a task to run the callback immediately.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::expected<void, std::string>()));
    return;
  }

  init_callbacks_.AddUnsafe(std::move(callback));
  if (initialization_state_ == InitializationState::kInitializing) {
    return;
  }

  HOST_LOG << "Starting Portal remote desktop session";
  initialization_state_ = InitializationState::kInitializing;
  GDBusConnectionRef::CreateForSessionBus(CheckResultAndContinue(
      &PortalRemoteDesktopSession::OnConnectionCreated,
      /*request=*/nullptr, "Failed to connect to D-Bus session bus"));

  // TODO: crbug.com/445973705 - Either the UI task runner, or use an event
  // based implementation. We currently cannot pass the UI task runner, because
  // it polls the PipeWire stream, which is bound to the network thread.
  display_info_monitor_ = std::make_unique<PollingDesktopDisplayInfoMonitor>(
      base::SequencedTaskRunner::GetCurrentDefault(),
      std::make_unique<PortalDisplayInfoLoader>(capture_stream_manager_));
  mouse_cursor_capturer_ = std::make_unique<PipewireMouseCursorCapturer>(
      std::make_unique<DelegatingDesktopDisplayInfoMonitor>(
          display_info_monitor_->GetWeakPtr()),
      capture_stream_manager_.GetWeakPtr());
  initialization_state_ = InitializationState::kInitializing;
}

template <typename SuccessType, typename String>
GDBusConnectionRef::CallCallback<SuccessType>
PortalRemoteDesktopSession::CheckResultAndContinue(
    void (PortalRemoteDesktopSession::*success_method)(SuccessType),
    std::unique_ptr<ScopedPortalRequest>* request,
    String&& error_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindOnce(
      [](base::WeakPtr<PortalRemoteDesktopSession> that,
         decltype(success_method) success_method,
         std::unique_ptr<ScopedPortalRequest>* request,
         std::string_view error_context,
         base::expected<SuccessType, Loggable> result) {
        if (!that) {
          return;
        }
        if (request) {
          request->reset();
        }
        if (result.has_value()) {
          (that.get()->*success_method)(std::move(result).value());
        } else {
          that->OnInitError(error_context, std::move(result).error());
        }
      },
      weak_ptr_factory_.GetWeakPtr(), success_method, request,
      std::forward<String>(error_context));
}

template <typename String>
GDBusConnectionRef::CallCallback<GVariantRef<"(o)">>
PortalRemoteDesktopSession::ResetRequestOnFailure(
    std::unique_ptr<ScopedPortalRequest>& request,
    String&& error_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindOnce(
      [](base::WeakPtr<PortalRemoteDesktopSession> that,
         std::unique_ptr<ScopedPortalRequest>& request,
         const std::string& error_context,
         base::expected<GVariantRef<"(o)">, Loggable> result) {
        if (!that) {
          return;
        }
        if (!result.has_value()) {
          request.reset();
          that->OnInitError(error_context, std::move(result).error());
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::ref(request),
      std::forward<String>(error_context));
}

void PortalRemoteDesktopSession::OnInitError(std::string_view error_message,
                                             Loggable error_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  initialization_state_ = InitializationState::kNotInitialized;
  portal_session_.reset();
  create_session_request_.reset();
  select_devices_request_.reset();
  connection_ = {};
  init_callbacks_.Notify(base::unexpected(
      base::StrCat({error_message, ": ", error_context.ToString()})));
  DCHECK(init_callbacks_.empty());
}

void PortalRemoteDesktopSession::OnConnectionCreated(
    GDBusConnectionRef connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_ = std::move(connection);

  create_session_request_ = std::make_unique<ScopedPortalRequest>(
      connection_,
      CheckResultAndContinue(
          &PortalRemoteDesktopSession::OnCreateSessionResponse,
          &create_session_request_, "Failed to create remote-desktop session"));

  portal_session_ = std::make_unique<ScopedPortalSession>(
      connection_, base::BindOnce(&PortalRemoteDesktopSession::OnSessionClosed,
                                  weak_ptr_factory_.GetWeakPtr()));

  gvariant::GVariantRef<"a{sv}"> options =
      GVariantDictBuilder()
          .Add("handle_token", create_session_request_->token())
          .Add("session_handle_token", portal_session_->token())
          .Build();

  connection_.Call<org_freedesktop_portal_RemoteDesktop::CreateSession>(
      kPortalBusName, kPortalObjectPath, std::make_tuple(options),
      ResetRequestOnFailure(create_session_request_,
                            "RemoteDesktop.CreateSession failed"),
      G_DBUS_CALL_FLAGS_NONE,
      // The portal backend sometimes takes a long time to start up, so we set
      // a large timeout.
      /*timeout_msec=*/base::Minutes(2).InMilliseconds());
}

void PortalRemoteDesktopSession::OnCreateSessionResponse(
    gvariant::GVariantRef<"a{sv}"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "RemoteDesktop.CreateSession succeeded.";

  select_devices_request_ = std::make_unique<ScopedPortalRequest>(
      connection_, CheckResultAndContinue(
                       &PortalRemoteDesktopSession::OnSelectDevicesResponse,
                       &select_devices_request_, "Failed to select devices"));

  gvariant::GVariantRef<"a{sv}"> options =
      GVariantDictBuilder()
          .Add("handle_token", select_devices_request_->token())
          .Add("types", /*KEYBOARD*/ 1u | /*MOUSE*/ 2u)
          .Build();

  connection_.Call<org_freedesktop_portal_RemoteDesktop::SelectDevices>(
      kPortalBusName, kPortalObjectPath,
      std::make_tuple(portal_session_->session_handle(), options),
      ResetRequestOnFailure(select_devices_request_,
                            "RemoteDesktop.SelectDevices failed"));
}

void PortalRemoteDesktopSession::OnSelectDevicesResponse(
    gvariant::GVariantRef<"a{sv}"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  capture_stream_manager_.Init(
      connection_, portal_session_->session_handle(),
      base::BindOnce(&PortalRemoteDesktopSession::OnCaptureStreamInitResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PortalRemoteDesktopSession::OnCaptureStreamInitResult(
    base::expected<void, std::string> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    OnInitError("Failed to initialize capture stream manager",
                Loggable(FROM_HERE, result.error()));
    return;
  }

  HOST_LOG << "Capture stream initialized.";

  connection_.Call<org_freedesktop_portal_RemoteDesktop::ConnectToEIS>(
      kPortalBusName, kPortalObjectPath,
      std::make_tuple(portal_session_->session_handle(),
                      gvariant::EmptyArrayOf<"{sv}">()),
      CheckResultAndContinue(&PortalRemoteDesktopSession::OnEisFd,
                             /*request=*/nullptr,
                             "RemoteDesktop.ConnectToEIS failed"));
}

void PortalRemoteDesktopSession::OnEisFd(
    std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto fd_list = std::move(args.second).MakeSparse();
  auto [handle] = args.first;
  auto eis_fd = fd_list.Extract(handle);
  if (!eis_fd.is_valid()) {
    OnInitError("Failed to get EIS FD",
                Loggable(FROM_HERE, "Handle not present in FD list"));
    return;
  }

  EiSenderSession::CreateWithFd(
      std::move(eis_fd),
      CheckResultAndContinue(&PortalRemoteDesktopSession::OnEiSession,
                             /*request=*/nullptr,
                             "Failed to create EI session"));
}

void PortalRemoteDesktopSession::OnEiSession(
    std::unique_ptr<EiSenderSession> ei_session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ei_session_ = std::move(ei_session);
  initialization_state_ = InitializationState::kInitialized;
  init_callbacks_.Notify(base::ok());
}

void PortalRemoteDesktopSession::OnSessionClosed(
    gvariant::GVariantRef<"a{sv}"> details) {
  webrtc::Scoped<char> details_string(g_variant_print(details.raw(), FALSE));
  OnInitError(
      "Portal session closed unexpectedly.",
      Loggable(FROM_HERE, details_string ? std::string{details_string.get()}
                                         : std::string{}));
}

}  // namespace remoting
