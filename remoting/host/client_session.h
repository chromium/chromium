// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_H_
#define REMOTING_HOST_CLIENT_SESSION_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_environment_options.h"
#include "remoting/host/host_experiment_session_plugin.h"
#include "remoting/host/host_extension_session_manager.h"
#include "remoting/host/remote_input_filter.h"
#include "remoting/proto/action.pb.h"
#include "remoting/protocol/clipboard_echo_filter.h"
#include "remoting/protocol/clipboard_filter.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/data_channel_manager.h"
#include "remoting/protocol/display_size.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/mouse_input_filter.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/video_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"

namespace remoting {

class AudioStream;
class DesktopEnvironment;
class DesktopEnvironmentFactory;
class InputInjector;
class MouseShapePump;
class ScreenControls;

namespace protocol {
class VideoLayout;
}  // namespace protocol

// A ClientSession keeps a reference to a connection to a client, and maintains
// per-client state.
class ClientSession : public protocol::HostStub,
                      public protocol::ConnectionToClient::EventHandler,
                      public protocol::VideoStream::Observer,
                      public ClientSessionControl,
                      public ClientSessionDetails {
 public:
  // Callback interface for passing events to the ChromotingHost.
  class EventHandler {
   public:
    // Called after authentication has started.
    virtual void OnSessionAuthenticating(ClientSession* client) = 0;

    // Called after authentication has finished successfully.
    virtual void OnSessionAuthenticated(ClientSession* client) = 0;

    // Called after we've finished connecting all channels.
    virtual void OnSessionChannelsConnected(ClientSession* client) = 0;

    // Called after authentication has failed. Must not tear down this
    // object. OnSessionClosed() is notified after this handler
    // returns.
    virtual void OnSessionAuthenticationFailed(ClientSession* client) = 0;

    // Called after connection has failed or after the client closed it.
    virtual void OnSessionClosed(ClientSession* client) = 0;

    // Called on notification of a route change event, when a channel is
    // connected.
    virtual void OnSessionRouteChange(
        ClientSession* client,
        const std::string& channel_name,
        const protocol::TransportRoute& route) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // |event_handler| and |desktop_environment_factory| must outlive |this|.
  // All |HostExtension|s in |extensions| must outlive |this|.
  ClientSession(
      EventHandler* event_handler,
      std::unique_ptr<protocol::ConnectionToClient> connection,
      DesktopEnvironmentFactory* desktop_environment_factory,
      const DesktopEnvironmentOptions& desktop_environment_options,
      const base::TimeDelta& max_duration,
      scoped_refptr<protocol::PairingRegistry> pairing_registry,
      const std::vector<HostExtension*>& extensions);
  ~ClientSession() override;

  // Returns the set of capabilities negotiated between client and host.
  const std::string& capabilities() const { return capabilities_; }

  // protocol::HostStub interface.
  void NotifyClientResolution(
      const protocol::ClientResolution& resolution) override;
  void ControlVideo(const protocol::VideoControl& video_control) override;
  void ControlAudio(const protocol::AudioControl& audio_control) override;
  void SetCapabilities(const protocol::Capabilities& capabilities) override;
  void RequestPairing(
      const remoting::protocol::PairingRequest& pairing_request) override;
  void DeliverClientMessage(const protocol::ExtensionMessage& message) override;
  void SelectDesktopDisplay(
      const protocol::SelectDesktopDisplayRequest& select_display) override;

  // protocol::ConnectionToClient::EventHandler interface.
  void OnConnectionAuthenticating() override;
  void OnConnectionAuthenticated() override;
  void CreateMediaStreams() override;
  void OnConnectionChannelsConnected() override;
  void OnConnectionClosed(protocol::ErrorCode error) override;
  void OnRouteChange(const std::string& channel_name,
                     const protocol::TransportRoute& route) override;
  void OnIncomingDataChannel(
      const std::string& channel_name,
      std::unique_ptr<protocol::MessagePipe> pipe) override;

  // ClientSessionControl interface.
  const std::string& client_jid() const override;
  void DisconnectSession(protocol::ErrorCode error) override;
  void OnLocalKeyPressed(uint32_t usb_keycode) override;
  void OnLocalPointerMoved(const webrtc::DesktopVector& position,
                           ui::EventType type) override;
  void SetDisableInputs(bool disable_inputs) override;
  void OnDesktopDisplayChanged(
      std::unique_ptr<protocol::VideoLayout> layout) override;

  // ClientSessionDetails interface.
  uint32_t desktop_session_id() const override;
  ClientSessionControl* session_control() override;

  protocol::ConnectionToClient* connection() const { return connection_.get(); }

  bool is_authenticated() { return is_authenticated_; }

  const std::string* client_capabilities() const {
    return client_capabilities_.get();
  }

  // Registers a DataChannelManager callback for testing.
  void RegisterCreateHandlerCallbackForTesting(
      const std::string& prefix,
      protocol::DataChannelManager::CreateHandlerCallback constructor);

  void SetEventTimestampsSourceForTests(
      scoped_refptr<protocol::InputEventTimestampsSource>
          event_timestamp_source);

  // Public for tests.
  void UpdateMouseClampingFilterOffset();

 private:
  // Creates a proxy for sending clipboard events to the client.
  std::unique_ptr<protocol::ClipboardStub> CreateClipboardProxy();

  void SetMouseClampingFilter(const DisplaySize& size);

  // protocol::VideoStream::Observer implementation.
  void OnVideoSizeChanged(protocol::VideoStream* stream,
                          const webrtc::DesktopSize& size,
                          const webrtc::DesktopVector& dpi) override;

  void CreateActionMessageHandler(
      std::vector<protocol::ActionRequest::Action> capabilities,
      const std::string& channel_name,
      std::unique_ptr<protocol::MessagePipe> pipe);

  void CreateFileTransferMessageHandler(
      const std::string& channel_name,
      std::unique_ptr<protocol::MessagePipe> pipe);

  EventHandler* event_handler_;

  // The connection to the client.
  std::unique_ptr<protocol::ConnectionToClient> connection_;

  std::string client_jid_;

  // Used to create a DesktopEnvironment instance for this session.
  DesktopEnvironmentFactory* desktop_environment_factory_;

  // The DesktopEnvironmentOptions used to initialize DesktopEnvironment.
  DesktopEnvironmentOptions desktop_environment_options_;

  // The DesktopEnvironment instance for this session.
  std::unique_ptr<DesktopEnvironment> desktop_environment_;

  // Filter used as the final element in the input pipeline.
  protocol::InputFilter host_input_filter_;

  // Tracker used to release pressed keys and buttons when disconnecting.
  protocol::InputEventTracker input_tracker_;

  // Filter used to disable remote inputs during local input activity.
  RemoteInputFilter remote_input_filter_;

  // Filter used to clamp mouse events to the current display dimensions.
  protocol::MouseInputFilter mouse_clamping_filter_;

  // Filter to used to stop clipboard items sent from the client being echoed
  // back to it.  It is the final element in the clipboard (client -> host)
  // pipeline.
  protocol::ClipboardEchoFilter clipboard_echo_filter_;

  // Filters used to manage enabling & disabling of input & clipboard.
  protocol::InputFilter disable_input_filter_;
  protocol::ClipboardFilter disable_clipboard_filter_;

  // Factory for weak pointers to the client clipboard stub.
  // This must appear after |clipboard_echo_filter_|, so that it won't outlive
  // it.
  base::WeakPtrFactory<protocol::ClipboardStub> client_clipboard_factory_;

  // The maximum duration of this session.
  // There is no maximum if this value is <= 0.
  base::TimeDelta max_duration_;

  // A timer that triggers a disconnect when the maximum session duration
  // is reached.
  base::OneShotTimer max_duration_timer_;

  // Objects responsible for sending video, audio and mouse shape.
  std::unique_ptr<protocol::VideoStream> video_stream_;
  std::unique_ptr<protocol::AudioStream> audio_stream_;
  std::unique_ptr<MouseShapePump> mouse_shape_pump_;

  // The set of all capabilities supported by the client.
  std::unique_ptr<std::string> client_capabilities_;

  // The set of all capabilities supported by the host.
  std::string host_capabilities_;

  // The set of all capabilities negotiated between client and host.
  std::string capabilities_;

  // Used to inject mouse and keyboard input and handle clipboard events.
  std::unique_ptr<InputInjector> input_injector_;

  // Used to apply client-requested changes in screen resolution.
  std::unique_ptr<ScreenControls> screen_controls_;

  // Contains the most recently gathered info about the desktop displays;
  DesktopDisplayInfo desktop_display_info_;

  // Default DPI values to use if a display reports 0 for DPI.
  int default_x_dpi_;
  int default_y_dpi_;

  // The id of the desktop display to show to the user.
  // Default is webrtc::kFullDesktopScreenId which shows all displays.
  webrtc::ScreenId show_display_id_ = webrtc::kFullDesktopScreenId;

  // The pairing registry for PIN-less authentication.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  // Used to manage extension functionality.
  std::unique_ptr<HostExtensionSessionManager> extension_manager_;

  // Used to dispatch new data channels to factory methods.
  protocol::DataChannelManager data_channel_manager_;

  // Set to true if the client was authenticated successfully.
  bool is_authenticated_ = false;

  // Set to true after all data channels have been connected.
  bool channels_connected_ = false;

  // Used to store video channel pause & lossless parameters.
  bool pause_video_ = false;
  bool lossless_video_encode_ = false;
  bool lossless_video_color_ = false;

  // VideoLayout is sent only after the control channel is connected. Until
  // then it's stored in |pending_video_layout_message_|.
  std::unique_ptr<protocol::VideoLayout> pending_video_layout_message_;

  scoped_refptr<protocol::InputEventTimestampsSource>
      event_timestamp_source_for_tests_;

  HostExperimentSessionPlugin host_experiment_session_plugin_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to disable callbacks to |this| once DisconnectSession() has been
  // called.
  base::WeakPtrFactory<ClientSessionControl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_H_
