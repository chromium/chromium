// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_HOST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/clipboard_filter.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/webrtc_transport.h"

namespace remoting::protocol {

class ClientControlDispatcher;
class ClientEventDispatcher;
class SessionConfig;
class WebrtcVideoRendererAdapter;
class WebrtcAudioSinkAdapter;

class WebrtcConnectionToHost : public ConnectionToHost,
                               public Session::EventHandler,
                               public WebrtcTransport::EventHandler,
                               public ChannelDispatcherBase::EventHandler {
 public:
  WebrtcConnectionToHost();

  WebrtcConnectionToHost(const WebrtcConnectionToHost&) = delete;
  WebrtcConnectionToHost& operator=(const WebrtcConnectionToHost&) = delete;

  ~WebrtcConnectionToHost() override;

  // ConnectionToHost interface.
  void set_client_stub(ClientStub* client_stub) override;
  void set_clipboard_stub(ClipboardStub* clipboard_stub) override;
  void set_video_renderer(VideoRenderer* video_renderer) override;
  void InitializeAudio(
      scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
      base::WeakPtr<AudioStub> audio_consumer) override;
  void Connect(std::unique_ptr<Session> session,
               scoped_refptr<TransportContext> transport_context,
               HostEventCallback* event_callback) override;
  void Disconnect(ErrorCode error) override;
  void ApplyNetworkSettings(const NetworkSettings& settings) override;
  const SessionConfig& config() override;
  ClipboardStub* clipboard_forwarder() override;
  HostStub* host_stub() override;
  InputStub* input_stub() override;
  State state() const override;

 private:
  // Session::EventHandler interface.
  void OnSessionStateChange(Session::State state) override;

  // WebrtcTransport::EventHandler interface.
  void OnWebrtcTransportConnecting() override;
  void OnWebrtcTransportConnected() override;
  void OnWebrtcTransportError(ErrorCode error) override;
  void OnWebrtcTransportProtocolChanged() override;
  void OnWebrtcTransportIncomingDataChannel(
      const std::string& name,
      std::unique_ptr<MessagePipe> pipe) override;
  void OnWebrtcTransportMediaStreamAdded(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnWebrtcTransportMediaStreamRemoved(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnWebrtcTransportRouteChanged(const TransportRoute& route) override;

  // ChannelDispatcherBase::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelClosed(ChannelDispatcherBase* channel_dispatcher) override;

  void NotifyIfChannelsReady();

  WebrtcVideoRendererAdapter* GetOrCreateVideoAdapter(const std::string& label);

  void CloseChannels();

  void OnFrameRendered(uint32_t frame_id,
                       base::TimeTicks event_timestamp,
                       base::TimeTicks frame_rendered_time);

  void SetState(State state, ErrorCode error);

  raw_ptr<HostEventCallback> event_callback_ = nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner_;

  // Stub for incoming messages.
  raw_ptr<ClientStub> client_stub_ = nullptr;
  raw_ptr<VideoRenderer> video_renderer_ = nullptr;
  base::WeakPtr<AudioStub> audio_consumer_;
  raw_ptr<ClipboardStub> clipboard_stub_ = nullptr;

  std::unique_ptr<Session> session_;
  std::unique_ptr<WebrtcTransport> transport_;

  std::unique_ptr<ClientControlDispatcher> control_dispatcher_;
  std::unique_ptr<ClientEventDispatcher> event_dispatcher_;
  ClipboardFilter clipboard_forwarder_;
  InputFilter event_forwarder_;

  std::unique_ptr<WebrtcVideoRendererAdapter> video_adapter_;
  std::unique_ptr<WebrtcAudioSinkAdapter> audio_adapter_;

  // Internal state of the connection.
  State state_ = INITIALIZING;
  ErrorCode error_ = ErrorCode::OK;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_HOST_H_
