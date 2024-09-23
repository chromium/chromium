// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_connection_to_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webrtc/thread_wrapper.h"
#include "net/base/io_buffer.h"
#include "remoting/base/logging.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/webrtc_video_encoder_vpx.h"
#include "remoting/protocol/audio_source.h"
#include "remoting/protocol/audio_stream.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/host_control_dispatcher.h"
#include "remoting/protocol/host_event_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_audio_stream.h"
#include "remoting/protocol/webrtc_transport.h"
#include "remoting/protocol/webrtc_video_encoder_factory.h"
#include "remoting/protocol/webrtc_video_stream.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/sctp_transport_interface.h"

namespace remoting::protocol {

namespace {

const char kVideoStatsStreamLabel[] = "screen_stream";

}  // namespace

// Currently the network thread is also used as the worker thread for webrtc.
//
// TODO(sergeyu): Figure out if we would benefit from using a separate thread as
// a worker thread.
WebrtcConnectionToClient::WebrtcConnectionToClient(
    std::unique_ptr<protocol::Session> session,
    scoped_refptr<protocol::TransportContext> transport_context,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner)
    : session_(std::move(session)),
      video_stats_dispatcher_(kVideoStatsStreamLabel),
      audio_task_runner_(audio_task_runner),
      control_dispatcher_(new HostControlDispatcher()),
      event_dispatcher_(new HostEventDispatcher()) {
  auto video_encoder_factory = std::make_unique<WebrtcVideoEncoderFactory>();
  video_encoder_factory_ = video_encoder_factory.get();
  transport_ = std::make_unique<WebrtcTransport>(
      webrtc::ThreadWrapper::current(), transport_context,
      std::move(video_encoder_factory), this);
  session_->SetEventHandler(this);
  session_->SetTransport(transport_.get());
}

WebrtcConnectionToClient::~WebrtcConnectionToClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void WebrtcConnectionToClient::SetEventHandler(
    ConnectionToClient::EventHandler* event_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  event_handler_ = event_handler;
}

protocol::Session* WebrtcConnectionToClient::session() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return session_.get();
}

void WebrtcConnectionToClient::Disconnect(ErrorCode error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This should trigger OnConnectionClosed() event and this object
  // may be destroyed as the result.
  session_->Close(error);
}

std::unique_ptr<VideoStream> WebrtcConnectionToClient::StartVideoStream(
    webrtc::ScreenId screen_id,
    std::unique_ptr<DesktopCapturer> desktop_capturer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(transport_);

  auto stream = std::make_unique<WebrtcVideoStream>(session_options_);
  stream->set_video_stats_dispatcher(video_stats_dispatcher_.GetWeakPtr());
  stream->Start(screen_id, std::move(desktop_capturer), transport_.get(),
                video_encoder_factory_);
  stream->SetEventTimestampsSource(
      event_dispatcher_->event_timestamps_source());
  return std::move(stream);
}

std::unique_ptr<AudioStream> WebrtcConnectionToClient::StartAudioStream(
    std::unique_ptr<AudioSource> audio_source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(transport_);

  std::unique_ptr<WebrtcAudioStream> stream(new WebrtcAudioStream());
  stream->Start(audio_task_runner_, std::move(audio_source), transport_.get());
  return std::move(stream);
}

// Return pointer to ClientStub.
ClientStub* WebrtcConnectionToClient::client_stub() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return control_dispatcher_.get();
}

void WebrtcConnectionToClient::set_clipboard_stub(
    protocol::ClipboardStub* clipboard_stub) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  control_dispatcher_->set_clipboard_stub(clipboard_stub);
}

void WebrtcConnectionToClient::set_host_stub(protocol::HostStub* host_stub) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  control_dispatcher_->set_host_stub(host_stub);
}

void WebrtcConnectionToClient::set_input_stub(protocol::InputStub* input_stub) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  event_dispatcher_->set_input_stub(input_stub);
}

void WebrtcConnectionToClient::ApplySessionOptions(
    const SessionOptions& options) {
  session_options_ = options;
  transport_->ApplySessionOptions(options);
  video_encoder_factory_->ApplySessionOptions(options);
}

void WebrtcConnectionToClient::ApplyNetworkSettings(
    const NetworkSettings& settings) {
  transport_->ApplyNetworkSettings(settings);
}

PeerConnectionControls* WebrtcConnectionToClient::peer_connection_controls() {
  return transport_.get();
}

WebrtcEventLogData* WebrtcConnectionToClient::rtc_event_log() {
  return transport_->rtc_event_log();
}

void WebrtcConnectionToClient::OnSessionStateChange(Session::State state) {
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

    case Session::AUTHENTICATED: {
      base::WeakPtr<WebrtcConnectionToClient> self = weak_factory_.GetWeakPtr();
      event_handler_->OnConnectionAuthenticated(
          session_->authenticator().GetSessionPolicies());

      // OnConnectionAuthenticated() call above may result in the connection
      // being torn down.
      if (self) {
        event_handler_->CreateMediaStreams();
      }
      break;
    }

    case Session::CLOSED:
    case Session::FAILED:
      control_dispatcher_.reset();
      event_dispatcher_.reset();
      transport_->Close(state == Session::CLOSED ? ErrorCode::OK
                                                 : session_->error());
      transport_.reset();
      event_handler_->OnConnectionClosed(
          state == Session::CLOSED ? ErrorCode::OK : session_->error());
      break;
  }
}

void WebrtcConnectionToClient::OnWebrtcTransportConnecting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Create outgoing control channel. |event_dispatcher_| is initialized later
  // because event channel is expected to be created by the client.
  control_dispatcher_->Init(
      transport_->CreateOutgoingChannel(control_dispatcher_->channel_name()),
      this);

  // Create channel for sending per-frame statistics. The video-stream will
  // only try to send any stats after this channel is connected.
  video_stats_dispatcher_.Init(
      transport_->CreateOutgoingChannel(video_stats_dispatcher_.channel_name()),
      this);
}

void WebrtcConnectionToClient::OnWebrtcTransportConnected() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto sctp_transport = transport_->peer_connection()->GetSctpTransport();
  if (sctp_transport) {
    std::optional<double> max_message_size =
        sctp_transport->Information().MaxMessageSize();
    if (max_message_size && *max_message_size > 0) {
      control_dispatcher_->set_max_message_size(*max_message_size);
    }
  }
}

void WebrtcConnectionToClient::OnWebrtcTransportError(ErrorCode error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Disconnect(error);
}

void WebrtcConnectionToClient::OnWebrtcTransportProtocolChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If not all channels are connected, this call will be deferred to
  // OnChannelInitialized() when all channels are connected.
  if (allChannelsConnected()) {
    event_handler_->OnTransportProtocolChange(transport_->transport_protocol());
  }
}

void WebrtcConnectionToClient::OnWebrtcTransportIncomingDataChannel(
    const std::string& name,
    std::unique_ptr<MessagePipe> pipe) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(event_handler_);

  if (name == event_dispatcher_->channel_name() &&
      !event_dispatcher_->is_connected()) {
    event_dispatcher_->Init(std::move(pipe), this);
    return;
  }

  event_handler_->OnIncomingDataChannel(name, std::move(pipe));
}

void WebrtcConnectionToClient::OnWebrtcTransportMediaStreamAdded(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(WARNING) << "The client created an unexpected media stream.";
}

void WebrtcConnectionToClient::OnWebrtcTransportMediaStreamRemoved(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void WebrtcConnectionToClient::OnWebrtcTransportRouteChanged(
    const TransportRoute& route) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(event_handler_);

  // WebRTC route-change events are triggered at the transport level, so the
  // channel name is not meaningful here.
  std::string channel_name;
  event_handler_->OnRouteChange(channel_name, route);
}

void WebrtcConnectionToClient::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (allChannelsConnected()) {
    event_handler_->OnConnectionChannelsConnected();
    if (!transport_->transport_protocol().empty()) {
      event_handler_->OnTransportProtocolChange(
          transport_->transport_protocol());
    }
  }
}

void WebrtcConnectionToClient::OnChannelClosed(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (channel_dispatcher == &video_stats_dispatcher_) {
    HOST_LOG << "video_stats channel was closed.";
    return;
  }

  // The control channel is closed when the user clicks disconnect on the client
  // or the client page is closed normally. If the client goes offline then the
  // channel will remain open. Hence it should be safe to report ErrorCode::OK
  // here.
  HOST_LOG << "Channel " << channel_dispatcher->channel_name()
           << " was closed.";
  Disconnect(ErrorCode::OK);
}

bool WebrtcConnectionToClient::allChannelsConnected() {
  return control_dispatcher_ && control_dispatcher_->is_connected() &&
         event_dispatcher_ && event_dispatcher_->is_connected();
}

}  // namespace remoting::protocol
