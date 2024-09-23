// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_connection_to_host.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/audio_decode_scheduler.h"
#include "remoting/protocol/audio_reader.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/client_control_dispatcher.h"
#include "remoting/protocol/client_event_dispatcher.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/client_video_dispatcher.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/ice_transport.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_renderer.h"

namespace remoting::protocol {

IceConnectionToHost::IceConnectionToHost() = default;

IceConnectionToHost::~IceConnectionToHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IceConnectionToHost::Connect(
    std::unique_ptr<Session> session,
    scoped_refptr<TransportContext> transport_context,
    HostEventCallback* event_callback) {
  DCHECK(client_stub_);
  DCHECK(clipboard_stub_);
  DCHECK(video_renderer_);

  transport_ = std::make_unique<IceTransport>(transport_context, this);

  session_ = std::move(session);
  session_->SetEventHandler(this);
  session_->SetTransport(transport_.get());

  event_callback_ = event_callback;

  SetState(CONNECTING, ErrorCode::OK);
}

void IceConnectionToHost::Disconnect(ErrorCode error) {
  session_->Close(error);
}

void IceConnectionToHost::ApplyNetworkSettings(
    const NetworkSettings& settings) {
  transport_->ApplyNetworkSettings(settings);
}

const SessionConfig& IceConnectionToHost::config() {
  return session_->config();
}

ClipboardStub* IceConnectionToHost::clipboard_forwarder() {
  return &clipboard_forwarder_;
}

HostStub* IceConnectionToHost::host_stub() {
  // TODO(wez): Add a HostFilter class, equivalent to input filter.
  return control_dispatcher_.get();
}

InputStub* IceConnectionToHost::input_stub() {
  return &event_forwarder_;
}

void IceConnectionToHost::set_client_stub(ClientStub* client_stub) {
  client_stub_ = client_stub;
}

void IceConnectionToHost::set_clipboard_stub(ClipboardStub* clipboard_stub) {
  clipboard_stub_ = clipboard_stub;
}

void IceConnectionToHost::set_video_renderer(VideoRenderer* video_renderer) {
  DCHECK(video_renderer);
  DCHECK(!monitored_video_stub_);
  video_renderer_ = video_renderer;
}

void IceConnectionToHost::InitializeAudio(
    scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
    base::WeakPtr<AudioStub> audio_stub) {
  audio_decode_scheduler_ = std::make_unique<AudioDecodeScheduler>(
      audio_decode_task_runner, audio_stub);
}

void IceConnectionToHost::OnSessionStateChange(Session::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(event_callback_);

  switch (state) {
    case Session::INITIALIZING:
    case Session::CONNECTING:
    case Session::ACCEPTING:
    case Session::ACCEPTED:
    case Session::AUTHENTICATING:
      // Don't care about these events.
      break;

    case Session::AUTHENTICATED:
      SetState(AUTHENTICATED, ErrorCode::OK);

      // Setup control channel.
      control_dispatcher_ = std::make_unique<ClientControlDispatcher>();
      control_dispatcher_->Init(transport_->GetMultiplexedChannelFactory(),
                                this);
      control_dispatcher_->set_client_stub(client_stub_);
      control_dispatcher_->set_clipboard_stub(clipboard_stub_);

      // Setup event channel.
      event_dispatcher_ = std::make_unique<ClientEventDispatcher>();
      event_dispatcher_->Init(transport_->GetMultiplexedChannelFactory(), this);

      // Configure video pipeline.
      video_renderer_->OnSessionConfig(session_->config());
      monitored_video_stub_ = std::make_unique<MonitoredVideoStub>(
          video_renderer_->GetVideoStub(),
          base::Seconds(MonitoredVideoStub::kConnectivityCheckDelaySeconds),
          base::BindRepeating(&IceConnectionToHost::OnVideoChannelStatus,
                              base::Unretained(this)));
      video_dispatcher_ = std::make_unique<ClientVideoDispatcher>(
          monitored_video_stub_.get(), client_stub_);
      video_dispatcher_->Init(transport_->GetChannelFactory(), this);

      // Configure audio pipeline if necessary.
      if (session_->config().is_audio_enabled()) {
        audio_reader_ =
            std::make_unique<AudioReader>(audio_decode_scheduler_.get());
        audio_reader_->Init(transport_->GetMultiplexedChannelFactory(), this);
        audio_decode_scheduler_->Initialize(session_->config());
      }
      break;

    case Session::CLOSED:
      CloseChannels();
      SetState(CLOSED, ErrorCode::OK);
      break;

    case Session::FAILED:
      // If we were connected then treat signaling timeout error as if
      // the connection was closed by the peer.
      //
      // TODO(sergeyu): This logic belongs to the webapp, but we
      // currently don't expose this error code to the webapp, and it
      // would be hard to add it because client plugin and webapp
      // versions may not be in sync. It should be easy to do after we
      // are finished moving the client plugin to NaCl.
      CloseChannels();
      if (state_ == CONNECTED &&
          session_->error() == ErrorCode::SIGNALING_TIMEOUT) {
        SetState(CLOSED, ErrorCode::OK);
      } else {
        SetState(FAILED, session_->error());
      }
      break;
  }
}

void IceConnectionToHost::OnIceTransportRouteChange(
    const std::string& channel_name,
    const TransportRoute& route) {
  event_callback_->OnRouteChanged(channel_name, route);
}

void IceConnectionToHost::OnIceTransportError(ErrorCode error) {
  session_->Close(error);
}

void IceConnectionToHost::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  NotifyIfChannelsReady();
}

void IceConnectionToHost::OnChannelClosed(
    ChannelDispatcherBase* channel_dispatcher) {
  session_->Close(ErrorCode::OK);
}

void IceConnectionToHost::OnVideoChannelStatus(bool active) {
  event_callback_->OnConnectionReady(active);
}

ConnectionToHost::State IceConnectionToHost::state() const {
  return state_;
}

void IceConnectionToHost::NotifyIfChannelsReady() {
  if (!control_dispatcher_.get() || !control_dispatcher_->is_connected()) {
    return;
  }
  if (!event_dispatcher_.get() || !event_dispatcher_->is_connected()) {
    return;
  }
  if (!video_dispatcher_.get() || !video_dispatcher_->is_connected()) {
    return;
  }
  if ((!audio_reader_.get() || !audio_reader_->is_connected()) &&
      session_->config().is_audio_enabled()) {
    return;
  }
  if (state_ != AUTHENTICATED) {
    return;
  }

  // Start forwarding clipboard and input events.
  clipboard_forwarder_.set_clipboard_stub(control_dispatcher_.get());
  event_forwarder_.set_input_stub(event_dispatcher_.get());
  SetState(CONNECTED, ErrorCode::OK);
}

void IceConnectionToHost::CloseChannels() {
  control_dispatcher_.reset();
  event_dispatcher_.reset();
  clipboard_forwarder_.set_clipboard_stub(nullptr);
  event_forwarder_.set_input_stub(nullptr);
  video_dispatcher_.reset();
  audio_reader_.reset();
}

void IceConnectionToHost::SetState(State state, ErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |error| should be specified only when |state| is set to FAILED.
  DCHECK(state == FAILED || error == ErrorCode::OK);

  if (state != state_) {
    state_ = state;
    error_ = error;
    event_callback_->OnConnectionState(state_, error_);
  }
}

}  // namespace remoting::protocol
