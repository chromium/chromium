// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_CONNECTION_TO_CLIENT_H_
#define REMOTING_PROTOCOL_ICE_CONNECTION_TO_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/ice_transport.h"
#include "remoting/protocol/session.h"

namespace remoting::protocol {

class AudioWriter;
class HostControlDispatcher;
class HostEventDispatcher;
class HostVideoDispatcher;

// This class represents a remote viewer connection to the chromoting host. It
// sets up all protocol channels and connects them to the stubs.
class IceConnectionToClient : public ConnectionToClient,
                              public Session::EventHandler,
                              public IceTransport::EventHandler,
                              public ChannelDispatcherBase::EventHandler {
 public:
  IceConnectionToClient(
      std::unique_ptr<Session> session,
      scoped_refptr<TransportContext> transport_context,
      scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner);

  IceConnectionToClient(const IceConnectionToClient&) = delete;
  IceConnectionToClient& operator=(const IceConnectionToClient&) = delete;

  ~IceConnectionToClient() override;

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
  void ApplyNetworkSettings(const NetworkSettings& settings) override;
  ClientStub* client_stub() override;
  void set_clipboard_stub(ClipboardStub* clipboard_stub) override;
  void set_host_stub(HostStub* host_stub) override;
  void set_input_stub(InputStub* input_stub) override;
  PeerConnectionControls* peer_connection_controls() override;
  WebrtcEventLogData* rtc_event_log() override;

 private:
  // Session::EventHandler interface.
  void OnSessionStateChange(Session::State state) override;

  // IceTransport::EventHandler interface.
  void OnIceTransportRouteChange(const std::string& channel_name,
                                 const TransportRoute& route) override;
  void OnIceTransportError(ErrorCode error) override;

  // ChannelDispatcherBase::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelClosed(ChannelDispatcherBase* channel_dispatcher) override;

  // Callback passed to |event_dispatcher_|
  void OnInputEventReceived(int64_t timestamp);

  void NotifyIfChannelsReady();

  void CloseChannels();

  // Event handler for handling events sent from this object.
  raw_ptr<ConnectionToClient::EventHandler> event_handler_;

  std::unique_ptr<Session> session_;

  scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  IceTransport transport_;

  std::unique_ptr<HostControlDispatcher> control_dispatcher_;
  std::unique_ptr<HostEventDispatcher> event_dispatcher_;
  std::unique_ptr<HostVideoDispatcher> video_dispatcher_;
  std::unique_ptr<AudioWriter> audio_writer_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_ICE_CONNECTION_TO_CLIENT_H_
