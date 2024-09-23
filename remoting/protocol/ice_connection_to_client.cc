// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_connection_to_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/audio_encoder_opus.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/protocol/audio_pump.h"
#include "remoting/protocol/audio_source.h"
#include "remoting/protocol/audio_writer.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/host_control_dispatcher.h"
#include "remoting/protocol/host_event_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/host_video_dispatcher.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_frame_pump.h"

namespace remoting::protocol {

namespace {

std::unique_ptr<AudioEncoder> CreateAudioEncoder(
    const protocol::SessionConfig& config) {
#if BUILDFLAG(IS_IOS)
  // TODO(nicholss): iOS should not use Opus. This is to prevent us from
  // depending on //media. In the future we will use webrtc for connection
  // and this will be a non-issue.
  return nullptr;
#else
  const protocol::ChannelConfig& audio_config = config.audio_config();
  if (audio_config.codec == protocol::ChannelConfig::CODEC_OPUS) {
    return base::WrapUnique(new AudioEncoderOpus());
  }
#endif

  NOTREACHED();
}

}  // namespace

IceConnectionToClient::IceConnectionToClient(
    std::unique_ptr<protocol::Session> session,
    scoped_refptr<TransportContext> transport_context,
    scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner)
    : event_handler_(nullptr),
      session_(std::move(session)),
      video_encode_task_runner_(std::move(video_encode_task_runner)),
      audio_task_runner_(std::move(audio_task_runner)),
      transport_(transport_context, this),
      control_dispatcher_(new HostControlDispatcher()),
      event_dispatcher_(new HostEventDispatcher()),
      video_dispatcher_(new HostVideoDispatcher()) {
  session_->SetEventHandler(this);
  session_->SetTransport(&transport_);
}

IceConnectionToClient::~IceConnectionToClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void IceConnectionToClient::SetEventHandler(
    ConnectionToClient::EventHandler* event_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  event_handler_ = event_handler;
}

protocol::Session* IceConnectionToClient::session() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return session_.get();
}

void IceConnectionToClient::Disconnect(ErrorCode error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This should trigger OnConnectionClosed() event and this object
  // may be destroyed as the result.
  session_->Close(error);
}

std::unique_ptr<VideoStream> IceConnectionToClient::StartVideoStream(
    webrtc::ScreenId screen_id,
    std::unique_ptr<DesktopCapturer> desktop_capturer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<VideoEncoder> video_encoder =
      VideoEncoder::Create(session_->config());

  std::unique_ptr<VideoFramePump> pump(
      new VideoFramePump(video_encode_task_runner_, std::move(desktop_capturer),
                         std::move(video_encoder), video_dispatcher_.get()));
  pump->SetEventTimestampsSource(event_dispatcher_->event_timestamps_source());
  video_dispatcher_->set_video_feedback_stub(pump->video_feedback_stub());
  return std::move(pump);
}

std::unique_ptr<AudioStream> IceConnectionToClient::StartAudioStream(
    std::unique_ptr<AudioSource> audio_source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Audio channel is disabled.
  if (!audio_writer_) {
    return nullptr;
  }

  std::unique_ptr<AudioEncoder> audio_encoder =
      CreateAudioEncoder(session_->config());

  return base::WrapUnique(
      new AudioPump(audio_task_runner_, std::move(audio_source),
                    std::move(audio_encoder), audio_writer_.get()));
}

void IceConnectionToClient::ApplyNetworkSettings(
    const NetworkSettings& settings) {
  transport_.ApplyNetworkSettings(settings);
}

// Return pointer to ClientStub.
ClientStub* IceConnectionToClient::client_stub() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return control_dispatcher_.get();
}

void IceConnectionToClient::set_clipboard_stub(
    protocol::ClipboardStub* clipboard_stub) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  control_dispatcher_->set_clipboard_stub(clipboard_stub);
}

void IceConnectionToClient::set_host_stub(protocol::HostStub* host_stub) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  control_dispatcher_->set_host_stub(host_stub);
}

void IceConnectionToClient::set_input_stub(protocol::InputStub* input_stub) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  event_dispatcher_->set_input_stub(input_stub);
}

PeerConnectionControls* IceConnectionToClient::peer_connection_controls() {
  return nullptr;
}

WebrtcEventLogData* IceConnectionToClient::rtc_event_log() {
  return nullptr;
}

void IceConnectionToClient::OnSessionStateChange(Session::State state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(event_handler_);
  switch (state) {
    case Session::INITIALIZING:
    case Session::CONNECTING:
    case Session::ACCEPTING:
    case Session::ACCEPTED:
      // Don't care about these events.
      break;
    case Session::AUTHENTICATING:
      event_handler_->OnConnectionAuthenticating();
      break;
    case Session::AUTHENTICATED:
      // Initialize channels.
      control_dispatcher_->Init(transport_.GetMultiplexedChannelFactory(),
                                this);
      event_dispatcher_->Init(transport_.GetMultiplexedChannelFactory(), this);
      video_dispatcher_->Init(transport_.GetChannelFactory(), this);

      audio_writer_ = AudioWriter::Create(session_->config());
      if (audio_writer_) {
        audio_writer_->Init(transport_.GetMultiplexedChannelFactory(), this);
      }

      // Notify the handler after initializing the channels, so that
      // ClientSession can get a client clipboard stub.
      event_handler_->OnConnectionAuthenticated(
          session_->authenticator().GetSessionPolicies());
      break;

    case Session::CLOSED:
    case Session::FAILED:
      CloseChannels();
      event_handler_->OnConnectionClosed(
          state == Session::FAILED ? session_->error() : ErrorCode::OK);
      break;
  }
}

void IceConnectionToClient::OnIceTransportRouteChange(
    const std::string& channel_name,
    const TransportRoute& route) {
  event_handler_->OnRouteChange(channel_name, route);
}

void IceConnectionToClient::OnIceTransportError(ErrorCode error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Disconnect(error);
}

void IceConnectionToClient::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NotifyIfChannelsReady();
}

void IceConnectionToClient::OnChannelClosed(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Disconnect(ErrorCode::OK);
}

void IceConnectionToClient::NotifyIfChannelsReady() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!control_dispatcher_ || !control_dispatcher_->is_connected()) {
    return;
  }
  if (!event_dispatcher_ || !event_dispatcher_->is_connected()) {
    return;
  }
  if (!video_dispatcher_ || !video_dispatcher_->is_connected()) {
    return;
  }
  if ((!audio_writer_ || !audio_writer_->is_connected()) &&
      session_->config().is_audio_enabled()) {
    return;
  }
  event_handler_->OnConnectionChannelsConnected();
  event_handler_->CreateMediaStreams();
}

void IceConnectionToClient::CloseChannels() {
  control_dispatcher_.reset();
  event_dispatcher_.reset();
  video_dispatcher_.reset();
  audio_writer_.reset();
}

}  // namespace remoting::protocol
