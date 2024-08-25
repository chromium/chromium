// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_CLIENT_H_
#define REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_video_stats_dispatcher.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/webrtc_transport.h"

namespace remoting::protocol {

class WebrtcVideoEncoderFactory;
class HostControlDispatcher;
class HostEventDispatcher;

class WebrtcConnectionToClient : public ConnectionToClient,
                                 public Session::EventHandler,
                                 public WebrtcTransport::EventHandler,
                                 public ChannelDispatcherBase::EventHandler {
 public:
  WebrtcConnectionToClient(
      std::unique_ptr<Session> session,
      scoped_refptr<protocol::TransportContext> transport_context,
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner);

  WebrtcConnectionToClient(const WebrtcConnectionToClient&) = delete;
  WebrtcConnectionToClient& operator=(const WebrtcConnectionToClient&) = delete;

  ~WebrtcConnectionToClient() override;

  // ConnectionToClient interface.
  void SetEventHandler(
      ConnectionToClient::EventHandler* event_handler) override;
  Session* session() override;
  void Disconnect(ErrorCode error) override;
  std::unique_ptr<VideoStream> StartVideoStream(
      webrtc::ScreenId screen_id,
      std::unique_ptr<DesktopCapturer> desktop_capturer) override;
  std::unique_ptr<AudioStream> StartAudioStream(
      std::unique_ptr<AudioSource> audio_source) override;
  ClientStub* client_stub() override;
  void set_clipboard_stub(ClipboardStub* clipboard_stub) override;
  void set_host_stub(HostStub* host_stub) override;
  void set_input_stub(InputStub* input_stub) override;
  void ApplySessionOptions(const SessionOptions& options) override;
  void ApplyNetworkSettings(const NetworkSettings& settings) override;
  PeerConnectionControls* peer_connection_controls() override;
  WebrtcEventLogData* rtc_event_log() override;

  // Session::EventHandler interface.
  void OnSessionStateChange(Session::State state) override;

  // WebrtcTransport::EventHandler interface
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

 private:
  bool allChannelsConnected();

  // Event handler for handling events sent from this object.
  raw_ptr<ConnectionToClient::EventHandler> event_handler_ = nullptr;

  std::unique_ptr<WebrtcTransport> transport_;

  std::unique_ptr<Session> session_;

  raw_ptr<WebrtcVideoEncoderFactory, AcrossTasksDanglingUntriaged>
      video_encoder_factory_;

  HostVideoStatsDispatcher video_stats_dispatcher_;

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  SessionOptions session_options_;

  std::unique_ptr<HostControlDispatcher> control_dispatcher_;
  std::unique_ptr<HostEventDispatcher> event_dispatcher_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<WebrtcConnectionToClient> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_CLIENT_H_
