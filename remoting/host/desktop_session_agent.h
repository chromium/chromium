// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_AGENT_H_
#define REMOTING_HOST_DESKTOP_SESSION_AGENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/file_transfer/session_file_operations_handler.h"
#include "remoting/host/mojo_video_capturer_list.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/remoting_mojom_traits.h"
#include "remoting/host/mouse_shape_pump.h"
#include "remoting/proto/url_forwarder_control.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "ui/events/event.h"

namespace base {
class Location;
}

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

namespace remoting {

class ActionExecutor;
class AudioCapturer;
class AudioPacket;
class AutoThreadTaskRunner;
class DesktopEnvironment;
class DesktopEnvironmentFactory;
class InputInjector;
class KeyboardLayoutMonitor;
class RemoteInputFilter;
class RemoteWebAuthnStateChangeNotifier;
class ScreenControls;
class ScreenResolution;
class UrlForwarderConfigurator;

namespace protocol {
class InputEventTracker;
}  // namespace protocol

// Provides screen/audio capturing and input injection services for
// the network process.
class DesktopSessionAgent
    : public base::RefCountedThreadSafe<DesktopSessionAgent>,
      public IPC::Listener,
      public webrtc::MouseCursorMonitor::Callback,
      public ClientSessionControl,
      public mojom::DesktopSessionAgent,
      public mojom::DesktopSessionControl {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Returns an instance of desktop environment factory used.
    virtual DesktopEnvironmentFactory& desktop_environment_factory() = 0;

    // Notifies the delegate that the network-to-desktop channel has been
    // disconnected.
    virtual void OnNetworkProcessDisconnected() = 0;

    // Allows the desktop process to ask the daemon process to crash the network
    // process. This should be called any time the network process sends an
    // invalid IPC message to the desktop process (indicating that the network
    // process might have been compromised).
    virtual void CrashNetworkProcess(const base::Location& location) = 0;
  };

  DesktopSessionAgent(
      scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner);

  DesktopSessionAgent(const DesktopSessionAgent&) = delete;
  DesktopSessionAgent& operator=(const DesktopSessionAgent&) = delete;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* cursor) override;
  void OnMouseCursorPosition(const webrtc::DesktopVector& position) override;

  // Forwards a local clipboard event to the network process over IPC.
  void OnClipboardEvent(const protocol::ClipboardEvent& event);

  // Forwards an audio packet though the IPC channel to the network process.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet);

  // mojom::DesktopSessionAgent implementation.
  void Start(const std::string& authenticated_jid,
             const ScreenResolution& resolution,
             const DesktopEnvironmentOptions& options,
             StartCallback callback) override;

  // mojom::DesktopSessionControl implementation.
  void CreateVideoCapturer(int64_t desktop_display_id,
                           CreateVideoCapturerCallback callback) override;
  void SetScreenResolution(const ScreenResolution& resolution) override;
  void LockWorkstation() override;
  void InjectSendAttentionSequence() override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;
  void SetUpUrlForwarder() override;
  void SignalWebAuthnExtension() override;
  void BeginFileRead(BeginFileReadCallback callback) override;
  void BeginFileWrite(const base::FilePath& file_path,
                      BeginFileWriteCallback callback) override;

  // Creates desktop integration components and a connected IPC channel to be
  // used to access them. The client end of the channel is returned.
  mojo::ScopedMessagePipeHandle Initialize(
      const base::WeakPtr<Delegate>& delegate);

  // Stops the agent asynchronously.
  void Stop();

 protected:
  friend class base::RefCountedThreadSafe<DesktopSessionAgent>;

  ~DesktopSessionAgent() override;

  // ClientSessionControl interface.
  const std::string& client_jid() const override;
  void DisconnectSession(protocol::ErrorCode error) override;
  void OnLocalKeyPressed(uint32_t usb_keycode) override;
  void OnLocalPointerMoved(const webrtc::DesktopVector& position,
                           ui::EventType type) override;
  void SetDisableInputs(bool disable_inputs) override;
  void OnDesktopDisplayChanged(
      std::unique_ptr<protocol::VideoLayout> layout) override;

  // Handles keyboard layout changes.
  void OnKeyboardLayoutChange(const protocol::KeyboardLayout& layout);

  // Posted to |audio_capture_task_runner_| to start the audio capturer.
  void StartAudioCapturer();

  // Posted to |audio_capture_task_runner_| to stop the audio capturer.
  void StopAudioCapturer();

 private:
  void OnCheckUrlForwarderSetUpResult(bool is_set_up);
  void OnUrlForwarderSetUpStateChanged(
      protocol::UrlForwarderControl::SetUpUrlForwarderResponse::State state);

  // Task runner dedicated to running methods of |audio_capturer_|.
  scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner_;

  // Task runner on which public methods of this class should be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Task runner on which keyboard/mouse input is injected.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner_;

  // Task runner used by the IPC channel.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Captures audio output.
  std::unique_ptr<AudioCapturer> audio_capturer_;

  std::string client_jid_;

  base::WeakPtr<Delegate> delegate_;

  // The DesktopEnvironment instance used by this agent.
  std::unique_ptr<DesktopEnvironment> desktop_environment_;

  // Executes action request events.
  std::unique_ptr<ActionExecutor> action_executor_;

  // Executes keyboard, mouse and clipboard events.
  std::unique_ptr<InputInjector> input_injector_;

  // Tracker used to release pressed keys and buttons when disconnecting.
  std::unique_ptr<protocol::InputEventTracker> input_tracker_;

  // Filter used to disable remote inputs during local input activity.
  std::unique_ptr<RemoteInputFilter> remote_input_filter_;

  // Used to apply client-requested changes in screen resolution.
  std::unique_ptr<ScreenControls> screen_controls_;

  // IPC channel connecting the desktop process with the network process.
  std::unique_ptr<IPC::ChannelProxy> network_channel_;

  // True if the desktop session agent has been started.
  bool started_ = false;

  // Per-display capturers which capture the screen and composite with the mouse
  // cursor if necessary.
  MojoVideoCapturerList video_capturers_;

  // Captures mouse shapes.
  std::unique_ptr<MouseShapePump> mouse_shape_pump_;

  // Watches for keyboard layout changes.
  std::unique_ptr<KeyboardLayoutMonitor> keyboard_layout_monitor_;

  // Routes file-transfer messages to the corresponding reader/writer to be
  // executed.
  std::optional<SessionFileOperationsHandler> session_file_operations_handler_;

  mojo::AssociatedRemote<mojom::DesktopSessionEventHandler>
      desktop_session_event_handler_;
  mojo::AssociatedRemote<mojom::DesktopSessionStateHandler>
      desktop_session_state_handler_;
  mojo::AssociatedReceiver<mojom::DesktopSessionAgent> desktop_session_agent_{
      this};
  mojo::AssociatedReceiver<mojom::DesktopSessionControl>
      desktop_session_control_{this};

  // Checks and configures the URL forwarder.
  std::unique_ptr<::remoting::UrlForwarderConfigurator>
      url_forwarder_configurator_;

  std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
      webauthn_state_change_notifier_;

  // Used to disable callbacks to |this|.
  base::WeakPtrFactory<DesktopSessionAgent> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_AGENT_H_
