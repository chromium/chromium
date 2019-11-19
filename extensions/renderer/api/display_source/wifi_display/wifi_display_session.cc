// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_session.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/wds/src/libwds/public/logging.h"
#include "third_party/wds/src/libwds/public/media_manager.h"

namespace {
const char kErrorInternal[] = "An internal error has occurred";
const char kErrorTimeout[] = "Sink became unresponsive";

static void LogWDSError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  char buffer[256];
  vsnprintf(buffer, 256, format, args);
  va_end(args);
  DVLOG(1) << "[WDS] " << buffer;
}

}  // namespace

namespace extensions {

using api::display_source::ErrorType;

WiFiDisplaySession::WiFiDisplaySession(const DisplaySourceSessionParams& params)
    : params_(params), cseq_(0), timer_id_(0), weak_factory_(this) {
  DCHECK(params_.render_frame);
  wds::LogSystem::set_error_func(&LogWDSError);
  params.render_frame->GetRemoteInterfaces()->GetInterface(&service_);
  service_.set_connection_error_handler(base::Bind(
          &WiFiDisplaySession::OnIPCConnectionError,
          weak_factory_.GetWeakPtr()));

  mojo::Remote<WiFiDisplaySessionServiceClient> client;
  receiver_.Bind(client.BindNewPipeAndPassReceiver());
  service_->SetClient(std::move(client));
  receiver_.set_disconnect_handler(base::Bind(
      &WiFiDisplaySession::OnIPCConnectionError, weak_factory_.GetWeakPtr()));
}

WiFiDisplaySession::~WiFiDisplaySession() {
}

void WiFiDisplaySession::Start(const CompletionCallback& callback) {
  DCHECK_EQ(DisplaySourceSession::Idle, state_);
  DCHECK(!terminated_callback_.is_null())
      << "Should be set with 'SetNotificationCallbacks'";
  DCHECK(!error_callback_.is_null())
      << "Should be set with 'SetNotificationCallbacks'";

  service_->Connect(params_.sink_id, params_.auth_method, params_.auth_data);
  state_ = DisplaySourceSession::Establishing;
  start_completion_callback_ = callback;
}

void WiFiDisplaySession::Terminate(const CompletionCallback& callback) {
  DCHECK_EQ(DisplaySourceSession::Established, state_);
  Terminate();
  teminate_completion_callback_ = callback;
}

void WiFiDisplaySession::OnConnected(const net::IPAddress& local_ip_address,
                                     const net::IPAddress& sink_ip_address) {
  DCHECK_EQ(DisplaySourceSession::Established, state_);
  local_ip_address_ = local_ip_address;
  media_manager_.reset(
      new WiFiDisplayMediaManager(
          params_.video_track,
          params_.audio_track,
          sink_ip_address,
          params_.render_frame->GetRemoteInterfaces(),
          base::Bind(
              &WiFiDisplaySession::OnMediaError,
              weak_factory_.GetWeakPtr())));
  wfd_source_.reset(wds::Source::Create(this, media_manager_.get(), this));
  wfd_source_->Start();
}

void WiFiDisplaySession::OnConnectRequestHandled(bool success,
                                                 const std::string& error) {
  DCHECK_EQ(DisplaySourceSession::Establishing, state_);
  state_ =
      success ? DisplaySourceSession::Established : DisplaySourceSession::Idle;
  RunStartCallback(success, error);
}

void WiFiDisplaySession::OnTerminated() {
  DCHECK_NE(DisplaySourceSession::Idle, state_);
  state_ = DisplaySourceSession::Idle;
  media_manager_.reset();
  wfd_source_.reset();
  terminated_callback_.Run();
}

void WiFiDisplaySession::OnDisconnectRequestHandled(bool success,
                                                    const std::string& error) {
  RunTerminateCallback(success, error);
}

void WiFiDisplaySession::OnError(int32_t type, const std::string& description) {
  DCHECK(type > api::display_source::ERROR_TYPE_NONE
         && type <= api::display_source::ERROR_TYPE_LAST);
  DCHECK_EQ(DisplaySourceSession::Established, state_);
  error_callback_.Run(static_cast<ErrorType>(type), description);
}

void WiFiDisplaySession::OnMessage(const std::string& data) {
  DCHECK_EQ(DisplaySourceSession::Established, state_);
  DCHECK(wfd_source_);
  wfd_source_->RTSPDataReceived(data);
}

std::string WiFiDisplaySession::GetLocalIPAddress() const {
  return local_ip_address_.ToString();
}

int WiFiDisplaySession::GetNextCSeq(int* initial_peer_cseq) const {
  return ++cseq_;
}

void WiFiDisplaySession::SendRTSPData(const std::string& message) {
  service_->SendMessage(message);
}

unsigned WiFiDisplaySession::CreateTimer(int seconds) {
  std::unique_ptr<base::RepeatingTimer> timer(new base::RepeatingTimer());
  auto insert_ret =
      timers_.insert(std::pair<int, std::unique_ptr<base::RepeatingTimer>>(
          ++timer_id_, std::move(timer)));
  DCHECK(insert_ret.second);
  insert_ret.first->second->Start(FROM_HERE,
               base::TimeDelta::FromSeconds(seconds),
               base::Bind(&wds::Source::OnTimerEvent,
                          base::Unretained(wfd_source_.get()),
                          timer_id_));
  return static_cast<unsigned>(timer_id_);
}

void WiFiDisplaySession::ReleaseTimer(unsigned timer_id) {
  auto it = timers_.find(static_cast<int>(timer_id));
  if (it != timers_.end())
    timers_.erase(it);
}

void WiFiDisplaySession::ErrorOccurred(wds::ErrorType error) {
  DCHECK_NE(DisplaySourceSession::Idle, state_);
  if (error == wds::TimeoutError) {
    error_callback_.Run(api::display_source::ERROR_TYPE_TIMEOUT_ERROR,
                        kErrorTimeout);
  } else {
    error_callback_.Run(api::display_source::ERROR_TYPE_UNKNOWN_ERROR,
                        kErrorInternal);
  }
  // The session cannot continue.
  Terminate();
}

void WiFiDisplaySession::SessionCompleted() {
  DCHECK_NE(DisplaySourceSession::Idle, state_);
  // The session has finished normally.
  Terminate();
}

void WiFiDisplaySession::OnIPCConnectionError() {
  // We must explicitly notify the session termination as it will never
  // arrive from browser process (IPC is broken).
  switch (state_) {
    case DisplaySourceSession::Idle:
    case DisplaySourceSession::Establishing:
      RunStartCallback(false, kErrorInternal);
      break;
    case DisplaySourceSession::Terminating:
    case DisplaySourceSession::Established:
      error_callback_.Run(api::display_source::ERROR_TYPE_UNKNOWN_ERROR,
                          kErrorInternal);
      state_ = DisplaySourceSession::Idle;
      terminated_callback_.Run();
      break;
    default:
      NOTREACHED();
  }
}

void WiFiDisplaySession::OnMediaError(const std::string& error) {
  DCHECK_NE(DisplaySourceSession::Idle, state_);
  error_callback_.Run(api::display_source::ERROR_TYPE_MEDIA_PIPELINE_ERROR,
                      error);
  Terminate();
}

void WiFiDisplaySession::Terminate() {
  if (state_ == DisplaySourceSession::Established) {
    service_->Disconnect();
    state_ = DisplaySourceSession::Terminating;
  }
}

void WiFiDisplaySession::RunStartCallback(bool success,
                                          const std::string& error_message) {
  if (!start_completion_callback_.is_null())
    start_completion_callback_.Run(success, error_message);
}

void WiFiDisplaySession::RunTerminateCallback(
    bool success,
    const std::string& error_message) {
  if (!teminate_completion_callback_.is_null())
    teminate_completion_callback_.Run(success, error_message);
}

}  // namespace extensions
