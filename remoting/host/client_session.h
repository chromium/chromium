// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_H_
#define REMOTING_HOST_CLIENT_SESSION_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "remoting/base/constants.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/client_session_events.h"
#include "remoting/host/desktop_and_cursor_composer_notifier.h"
#include "remoting/host/desktop_and_cursor_conditional_composer.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/host_experiment_session_plugin.h"
#include "remoting/host/host_extension_session_manager.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/host/remote_input_filter.h"
#include "remoting/proto/action.pb.h"
#include "remoting/protocol/clipboard_echo_filter.h"
#include "remoting/protocol/clipboard_filter.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/data_channel_manager.h"
#include "remoting/protocol/display_size.h"
#include "remoting/protocol/fractional_input_filter.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/mouse_input_filter.h"
#include "remoting/protocol/observing_input_filter.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/video_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "ui/events/event.h"

namespace remoting {

class ActiveDisplayMonitor;
class DesktopEnvironment;
class DesktopEnvironmentFactory;
class InputInjector;
class KeyboardLayoutMonitor;
class MouseShapePump;
class RemoteOpenUrlMessageHandler;
class RemoteWebAuthnMessageHandler;
class ScreenControls;

namespace protocol {
class AudioStream;
class VideoLayout;
}  // namespace protocol

// A ClientSession keeps a reference to a connection to a client, and maintains
// per-client state.
class ClientSession : public protocol::HostStub,
                      public protocol::ConnectionToClient::EventHandler,
                      public protocol::VideoStream::Observer,
                      public ClientSessionControl,
                      public ClientSessionDetails,
                      public ClientSessionEvents,
                      public DesktopAndCursorComposerNotifier::EventHandler,
                      public webrtc::MouseCursorMonitor::Callback,
                      public mojom::ChromotingSessionServices {
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
      scoped_refptr<protocol::PairingRegistry> pairing_registry,
      const std::vector<raw_ptr<HostExtension, VectorExperimental>>& extensions,
      const LocalSessionPoliciesProvider* local_session_policies_provider);

  ClientSession(const ClientSession&) = delete;
  ClientSession& operator=(const ClientSession&) = delete;

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
  void ControlPeerConnection(
      const protocol::PeerConnectionParameters& parameters) override;
  void SetVideoLayout(const protocol::VideoLayout& video_layout) override;

  // protocol::ConnectionToClient::EventHandler interface.
  void OnConnectionAuthenticating() override;
  void OnConnectionAuthenticated(
      const SessionPolicies* session_policies) override;
  void CreateMediaStreams() override;
  void OnConnectionChannelsConnected() override;
  void OnConnectionClosed(protocol::ErrorCode error) override;
  void OnTransportProtocolChange(const std::string& protocol) override;
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

  // ClientSessionEvents interface.
  void OnDesktopAttached(uint32_t session_id) override;
  void OnDesktopDetached() override;

  // ClientSessionDetails interface.
  uint32_t desktop_session_id() const override;
  ClientSessionControl* session_control() override;

  // DesktopAndCursorComposerNotifier::EventHandler interface
  void SetComposeEnabled(bool enabled) override;

  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* mouse_cursor) override;
  void OnMouseCursorPosition(const webrtc::DesktopVector& position) override;

  // mojom::ChromotingSessionServices implementation.
  void BindWebAuthnProxy(
      mojo::PendingReceiver<mojom::WebAuthnProxy> receiver) override;
  void BindRemoteUrlOpener(
      mojo::PendingReceiver<mojom::RemoteUrlOpener> receiver) override;
#if BUILDFLAG(IS_WIN)
  void BindSecurityKeyForwarder(
      mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) override;
#endif

  void BindReceiver(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver);

  protocol::ConnectionToClient* connection() const { return connection_.get(); }

  bool is_authenticated() const { return is_authenticated_; }

  bool channels_connected() const { return channels_connected_; }

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

  const SessionPolicies& effective_policies_for_tests() const {
    return effective_policies_;
  }

 private:
  void OnLocalSessionPoliciesChanged(const SessionPolicies& new_policies);

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

  void CreateRtcLogTransferMessageHandler(
      const std::string& channel_name,
      std::unique_ptr<protocol::MessagePipe> pipe);

  void CreateRemoteOpenUrlMessageHandler(
      const std::string& channel_name,
      std::unique_ptr<protocol::MessagePipe> pipe);

  void CreateUrlForwarderControlMessageHandler(
      const std::string& channel_name,
      std::unique_ptr<protocol::MessagePipe> pipe);

  void CreateRemoteWebAuthnMessageHandler(
      const std::string& channel_name,
      std::unique_ptr<protocol::MessagePipe> pipe);

  void CreatePerMonitorVideoStreams();

  // True if |index| corresponds with an existing display (or the combined
  // display).
  bool IsValidDisplayIndex(webrtc::ScreenId index) const;

  // Boosts the framerate using |capture_interval| for |boost_duration| based on
  // the type of input |event| received.
  void BoostFramerateOnInput(base::TimeDelta capture_interval,
                             base::TimeDelta boost_duration,
                             bool& mouse_button_down,
                             protocol::ObservingInputFilter::Event event);

  // Sends the new active display to the client. Called by ActiveDisplayMonitor
  // whenever the screen id associated with the active window changes.
  void OnActiveDisplayChanged(webrtc::ScreenId display);

  // Sets the fallback geometry on `fractional_input_filter_` according to the
  // current display-layout and selected display index. This is only used for
  // single-stream mode, when the client provides fractional-coordinates without
  // any screen_id.
  void UpdateFractionalFilterFallback();

  raw_ptr<EventHandler> event_handler_;

  // Used to create a DesktopEnvironment instance for this session.
  raw_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;

  // The DesktopEnvironmentOptions used to initialize DesktopEnvironment.
  DesktopEnvironmentOptions desktop_environment_options_;

  // The DesktopEnvironment instance for this session.
  std::unique_ptr<DesktopEnvironment> desktop_environment_;

  // Tracker used to release pressed keys and buttons when disconnecting.
  protocol::InputEventTracker input_tracker_;

  // Filter used to disable remote inputs during local input activity.
  RemoteInputFilter remote_input_filter_;

  // Filter used to convert any fractional coordinates to input-injection
  // coordinates.
  protocol::FractionalInputFilter fractional_input_filter_;

  // Filter used to clamp mouse events to the current display dimensions.
  protocol::MouseInputFilter mouse_clamping_filter_;

  // Filter used to notify listeners when remote input events are received.
  protocol::ObservingInputFilter observing_input_filter_;

  // Filter used to detect transitions into and out of client-side pointer lock,
  // and to monitor local input to determine whether or not to include the mouse
  // cursor in the desktop image.
  DesktopAndCursorComposerNotifier desktop_and_cursor_composer_notifier_;

  // Filter to used to stop clipboard items sent from the client being echoed
  // back to it.  It is the final element in the clipboard (client -> host)
  // pipeline.
  protocol::ClipboardEchoFilter clipboard_echo_filter_;

  // Filters used to manage enabling & disabling of input.
  protocol::InputFilter disable_input_filter_;

  // Used to enable/disable clipboard sync and to restrict payload size.
  protocol::ClipboardFilter host_clipboard_filter_;
  protocol::ClipboardFilter client_clipboard_filter_;

  // Factory for weak pointers to the client clipboard stub.
  // This must appear after |clipboard_echo_filter_|, so that it won't outlive
  // it.
  base::WeakPtrFactory<protocol::ClipboardStub> client_clipboard_factory_;

  // A timer that triggers a disconnect when the maximum session duration
  // is reached.
  base::OneShotTimer max_duration_timer_;

  // Objects responsible for sending video, audio.
  std::map<webrtc::ScreenId, std::unique_ptr<protocol::VideoStream>>
      video_streams_;
  std::unique_ptr<protocol::AudioStream> audio_stream_;

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
  int default_x_dpi_ = kDefaultDpi;
  int default_y_dpi_ = kDefaultDpi;

  // The index of the desktop display to show to the user.
  // Default is webrtc::kInvalidScreenScreenId because we need to perform
  // an initial capture to determine if the current setup support capturing
  // the entire desktop or if it is restricted to a single display.
  // This value is either an index into |desktop_display_info_| or one of
  // the special values webrtc::kInvalidScreenId, webrtc::kFullDesktopScreenId.
  webrtc::ScreenId selected_display_index_ = webrtc::kInvalidScreenId;

  // The initial video size captured by WebRTC.
  // This will be the full desktop unless webrtc cannot capture the entire
  // desktop (e.g., because the DPIs don't match). In that case, it will
  // be equal to the dimensions of the default display.
  DisplaySize default_webrtc_desktop_size_;

  // The current size of the area being captured by webrtc. This will be
  // equal to the size of the entire desktop, or to a single display.
  DisplaySize webrtc_capture_size_;

  // Set to true if the current display configuration supports capturing the
  // entire desktop.
  bool can_capture_full_desktop_ = true;

  // The pairing registry for PIN-less authentication.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  // Used to dispatch new data channels to factory methods.
  protocol::DataChannelManager data_channel_manager_;

  // Set to true if the client was authenticated successfully.
  bool is_authenticated_ = false;

  // Set to true after all data channels have been connected.
  bool channels_connected_ = false;

  // Used to store the video channel pause parameter.
  bool pause_video_ = false;

  // Used to store the target framerate control parameter.
  int target_framerate_ = kTargetFrameRate;

  // VideoLayout is sent only after the control channel is connected. Until
  // then it's stored in |pending_video_layout_message_|.
  std::unique_ptr<protocol::VideoLayout> pending_video_layout_message_;

  scoped_refptr<protocol::InputEventTimestampsSource>
      event_timestamp_source_for_tests_;

  HostExperimentSessionPlugin host_experiment_session_plugin_;

  // The connection to the client.
  std::unique_ptr<protocol::ConnectionToClient> connection_;

  std::string client_jid_;

  // Used to manage extension functionality.
  std::unique_ptr<HostExtensionSessionManager> extension_manager_;

  // Objects to monitor and send updates for mouse shape and keyboard layout.
  std::unique_ptr<MouseShapePump> mouse_shape_pump_;
  std::unique_ptr<KeyboardLayoutMonitor> keyboard_layout_monitor_;

  base::WeakPtr<RemoteWebAuthnMessageHandler> remote_webauthn_message_handler_;
  base::WeakPtr<RemoteOpenUrlMessageHandler> remote_open_url_message_handler_;

  mojo::ReceiverSet<mojom::ChromotingSessionServices>
      session_services_receivers_;

  std::unique_ptr<ActiveDisplayMonitor> active_display_monitor_;

  SessionPolicies effective_policies_;

  raw_ptr<const LocalSessionPoliciesProvider> local_session_policies_provider_;

  // If `effective_policies` does not come from local session policies, the
  // subscription will be null and OnLocalSessionPoliciesChanged() will never
  // be called.
  base::CallbackListSubscription local_session_policy_update_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to disable callbacks to |this| once DisconnectSession() has been
  // called.
  base::WeakPtrFactory<ClientSession> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_H_
