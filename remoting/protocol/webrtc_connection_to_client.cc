// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_connection_to_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/io_buffer.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/webrtc_video_encoder_vpx.h"
#include "remoting/protocol/audio_source.h"
#include "remoting/protocol/audio_stream.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/host_control_dispatcher.h"
#include "remoting/protocol/host_event_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_audio_stream.h"
#include "remoting/protocol/webrtc_transport.h"
#include "remoting/protocol/webrtc_video_stream.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/sctp_transport_interface.h"

namespace remoting {
namespace protocol {

// Currently the network thread is also used as worker thread for webrtc.
//
// TODO(sergeyu): Figure out if we would benefit from using a separate
// thread as a worker thread.
WebrtcConnectionToClient::WebrtcConnectionToClient(
    std::unique_ptr<protocol::Session> session,
    scoped_refptr<protocol::TransportContext> transport_context,
    scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner)
    : transport_(
          new WebrtcTransport(jingle_glue::JingleThreadWrapper::current(),
                              transport_context,
                              this)),
      session_(std::move(session)),
      video_encode_task_runner_(video_encode_task_runner),
      audio_task_runner_(audio_task_runner),
      control_dispatcher_(new HostControlDispatcher()),
      event_dispatcher_(new HostEventDispatcher()) {
  session_->SetEventHandler(this);
  session_->SetTransport(transport_.get());
}

WebrtcConnectionToClient::~WebrtcConnectionToClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcConnectionToClient::SetEventHandler(
    ConnectionToClient::EventHandler* event_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_handler_ = event_handler;
}

protocol::Session* WebrtcConnectionToClient::session() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return session_.get();
}

void WebrtcConnectionToClient::Disconnect(ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This should trigger OnConnectionClosed() event and this object
  // may be destroyed as the result.
  session_->Close(error);
}

std::unique_ptr<VideoStream> WebrtcConnectionToClient::StartVideoStream(
    std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(transport_);

  std::unique_ptr<WebrtcVideoStream> stream(
      new WebrtcVideoStream(session_options_));
  stream->Start(std::move(desktop_capturer), transport_.get(),
                video_encode_task_runner_);
  stream->SetEventTimestampsSource(
      event_dispatcher_->event_timestamps_source());
  return std::move(stream);
}

std::unique_ptr<AudioStream> WebrtcConnectionToClient::StartAudioStream(
    std::unique_ptr<AudioSource> audio_source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(transport_);

  std::unique_ptr<WebrtcAudioStream> stream(new WebrtcAudioStream());
  stream->Start(audio_task_runner_, std::move(audio_source), transport_.get());
  return std::move(stream);
}

// Return pointer to ClientStub.
ClientStub* WebrtcConnectionToClient::client_stub() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return control_dispatcher_.get();
}

void WebrtcConnectionToClient::set_clipboard_stub(
    protocol::ClipboardStub* clipboard_stub) {
  DCHECK(thread_checker_.CalledOnValidThread());
  control_dispatcher_->set_clipboard_stub(clipboard_stub);
}

void WebrtcConnectionToClient::set_host_stub(protocol::HostStub* host_stub) {
  DCHECK(thread_checker_.CalledOnValidThread());
  control_dispatcher_->set_host_stub(host_stub);
}

void WebrtcConnectionToClient::set_input_stub(protocol::InputStub* input_stub) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_dispatcher_->set_input_stub(input_stub);
}

void WebrtcConnectionToClient::ApplySessionOptions(
    const SessionOptions& options) {
  session_options_ = options;
  DCHECK(transport_);
  transport_->ApplySessionOptions(options);
}

void WebrtcConnectionToClient::OnSessionStateChange(Session::State state) {
  DCHECK(thread_checker_.CalledOnValidThread());

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
      event_handler_->OnConnectionAuthenticated();

      // OnConnectionAuthenticated() call above may result in the connection
      // being torn down.
      if (self)
        event_handler_->CreateMediaStreams();
      break;
    }

    case Session::CLOSED:
    case Session::FAILED:
      control_dispatcher_.reset();
      event_dispatcher_.reset();
      transport_->Close(state == Session::CLOSED ? OK : session_->error());
      transport_.reset();
      event_handler_->OnConnectionClosed(
          state == Session::CLOSED ? OK : session_->error());
      break;
  }
}

void WebrtcConnectionToClient::OnWebrtcTransportConnecting() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Create outgoing control channel. |event_dispatcher_| is initialized later
  // because event channel is expected to be created by the client.
  control_dispatcher_->Init(
      transport_->CreateOutgoingChannel(control_dispatcher_->channel_name()),
      this);
}

void WebrtcConnectionToClient::OnWebrtcTransportConnected() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto sctp_transport = transport_->peer_connection()->GetSctpTransport();
  if (sctp_transport) {
    absl::optional<double> max_message_size =
        sctp_transport->Information().MaxMessageSize();
    if (max_message_size && *max_message_size > 0) {
      control_dispatcher_->set_max_message_size(*max_message_size);
    }
  }
}

void WebrtcConnectionToClient::OnWebrtcTransportError(ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Disconnect(error);
}

void WebrtcConnectionToClient::OnWebrtcTransportIncomingDataChannel(
    const std::string& name,
    std::unique_ptr<MessagePipe> pipe) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(event_handler_);

  if (name == event_dispatcher_->channel_name() &&
      !event_dispatcher_->is_connected()) {
    event_dispatcher_->Init(std::move(pipe), this);
    return;
  }

  event_handler_->OnIncomingDataChannel(name, std::move(pipe));
}

void WebrtcConnectionToClient::OnWebrtcTransportMediaStreamAdded(
    scoped_refptr<webrtc::MediaStreamInterface> stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(WARNING) << "The client created an unexpected media stream.";
}

void WebrtcConnectionToClient::OnWebrtcTransportMediaStreamRemoved(
    scoped_refptr<webrtc::MediaStreamInterface> stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcConnectionToClient::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (control_dispatcher_ && control_dispatcher_->is_connected() &&
      event_dispatcher_ && event_dispatcher_->is_connected()) {
    event_handler_->OnConnectionChannelsConnected();
  }
}

void WebrtcConnectionToClient::OnChannelClosed(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK(thread_checker_.CalledOnValidThread());

  LOG(ERROR) << "Channel " << channel_dispatcher->channel_name()
             << " was closed unexpectedly.";
  Disconnect(INCOMPATIBLE_PROTOCOL);
}

}  // namespace protocol
}  // namespace remoting
