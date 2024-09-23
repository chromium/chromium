// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_PROXY_H_
#define REMOTING_HOST_DESKTOP_SESSION_PROXY_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/file_transfer/ipc_file_operations.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/remoting_mojom_traits.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/url_forwarder_control.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/errors.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

namespace webrtc {
class MouseCursor;
}  // namespace webrtc

namespace remoting {

class AudioPacket;
class ClientSessionControl;
class DesktopSessionConnector;
class IpcAudioCapturer;
class IpcMouseCursorMonitor;
class IpcKeyboardLayoutMonitor;
class IpcVideoFrameCapturer;
class ScreenControls;

// DesktopSessionProxy is created by an owning DesktopEnvironment to route
// requests from stubs to the DesktopSessionAgent instance through
// the IPC channel. DesktopSessionProxy is owned both by the DesktopEnvironment
// and the stubs, since stubs can out-live their DesktopEnvironment.
//
// DesktopSessionProxy objects are ref-counted but are always deleted on
// the |caller_task_runner| thread. This makes it possible to continue
// to receive IPC messages after the ref-count has dropped to zero, until
// the proxy is deleted. DesktopSessionProxy must therefore avoid creating new
// references to itself while handling IPC messages and desktop
// attach/detach notifications.
//
// All public methods of DesktopSessionProxy are called on
// the |caller_task_runner| thread unless it is specified otherwise.
class DesktopSessionProxy
    : public base::RefCountedDeleteOnSequence<DesktopSessionProxy>,
      public IPC::Listener,
      public IpcFileOperations::RequestHandler,
      public mojom::DesktopSessionEventHandler,
      public mojom::DesktopSessionStateHandler {
 public:
  DesktopSessionProxy(
      scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<ClientSessionEvents> client_session_events,
      base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
      const DesktopEnvironmentOptions& options);

  DesktopSessionProxy(const DesktopSessionProxy&) = delete;
  DesktopSessionProxy& operator=(const DesktopSessionProxy&) = delete;

  // Mirrors DesktopEnvironment.
  std::unique_ptr<ActionExecutor> CreateActionExecutor();
  std::unique_ptr<AudioCapturer> CreateAudioCapturer();
  std::unique_ptr<InputInjector> CreateInputInjector();
  std::unique_ptr<ScreenControls> CreateScreenControls();
  std::unique_ptr<DesktopCapturer> CreateVideoCapturer(webrtc::ScreenId id);
  std::unique_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor();
  std::unique_ptr<KeyboardLayoutMonitor> CreateKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback);
  std::unique_ptr<FileOperations> CreateFileOperations();
  std::unique_ptr<UrlForwarderConfigurator> CreateUrlForwarderConfigurator();
  std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
  CreateRemoteWebAuthnStateChangeNotifier();
  std::string GetCapabilities() const;
  void SetCapabilities(const std::string& capabilities);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // Connects to the desktop session agent.
  bool AttachToDesktop(mojo::ScopedMessagePipeHandle desktop_pipe,
                       int session_id);

  // Closes the connection to the desktop session agent and cleans up
  // the associated resources.
  void DetachFromDesktop();

  // Stores |audio_capturer| to be used to post captured audio packets. Called
  // on the |audio_capture_task_runner_| thread.
  void SetAudioCapturer(const base::WeakPtr<IpcAudioCapturer>& audio_capturer);

  // Stores |mouse_cursor_monitor| to be used to post mouse cursor changes.
  void SetMouseCursorMonitor(
      const base::WeakPtr<IpcMouseCursorMonitor>& mouse_cursor_monitor);

  // Stores |keyboard_layout_monitor| to be used to post keyboard layout
  // changes.
  void SetKeyboardLayoutMonitor(
      const base::WeakPtr<IpcKeyboardLayoutMonitor>& keyboard_layout_monitor);
  const std::optional<protocol::KeyboardLayout>& GetKeyboardCurrentLayout()
      const;

  // Implements SelectSource() for a single video-capture. Must only be called
  // in single-stream mode.
  // TODO: b/326319067 - When single-stream support is no longer needed,
  // remove this method, remove desktop_session_proxy_ from
  // IpcVideoFrameCapturer, and remove all SelectSource() methods.
  void RebindSingleVideoCapturer(
      webrtc::ScreenId new_id,
      base::WeakPtr<IpcVideoFrameCapturer> capturer_weakptr);

  // APIs used to implement the InputInjector interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event);
  void InjectKeyEvent(const protocol::KeyEvent& event);
  void InjectTextEvent(const protocol::TextEvent& event);
  void InjectMouseEvent(const protocol::MouseEvent& event);
  void InjectTouchEvent(const protocol::TouchEvent& event);
  void StartInputInjector(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard);

  // API used to implement the SessionController interface.
  void SetScreenResolution(const ScreenResolution& resolution);

  // API used to implement the ActionExecutor interface.
  void ExecuteAction(const protocol::ActionRequest& request);

  // IpcFileOperations::RequestHandler implementation.
  void BeginFileRead(IpcFileOperations::BeginFileReadCallback callback,
                     base::OnceClosure on_disconnect) override;
  void BeginFileWrite(const base::FilePath& file_path,
                      IpcFileOperations::BeginFileWriteCallback callback,
                      base::OnceClosure on_disconnect) override;

  // mojom::DesktopSessionEventHandler implementation.
  void OnClipboardEvent(const protocol::ClipboardEvent& event) override;
  void OnUrlForwarderStateChange(mojom::UrlForwarderState state) override;
  void OnAudioPacket(std::unique_ptr<AudioPacket> audio_packet) override;
  void OnDesktopDisplayChanged(const protocol::VideoLayout& layout) override;
  void OnMouseCursorChanged(const webrtc::MouseCursor& mouse_cursor) override;
  void OnKeyboardLayoutChanged(const protocol::KeyboardLayout& layout) override;

  // mojom::DesktopSessionStateHandler implementation.
  void DisconnectSession(protocol::ErrorCode error) override;

  // API used to implement the UrlForwarderConfigurator interface.
  void IsUrlForwarderSetUp(
      UrlForwarderConfigurator::IsUrlForwarderSetUpCallback callback);
  void SetUpUrlForwarder(
      const UrlForwarderConfigurator::SetUpUrlForwarderCallback& callback);

  uint32_t desktop_session_id() const { return desktop_session_id_; }

 private:
  friend class base::RefCountedDeleteOnSequence<DesktopSessionProxy>;
  friend class base::DeleteHelper<DesktopSessionProxy>;

  ~DesktopSessionProxy() override;

  // Called when the desktop agent has started and provides the remote used to
  // inject input events and control A/V capture.
  void OnDesktopSessionAgentStarted(
      mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>
          pending_remote);

  // Handles the BeginFileReadResult returned from the DesktopSessionAgent.
  void OnBeginFileReadResult(
      IpcFileOperations::BeginFileReadCallback callback,
      base::CallbackListSubscription disconnect_handler_subscription,
      mojom::BeginFileReadResultPtr result);

  // Handles the BeginFileWriteResult returned from the DesktopSessionAgent.
  void OnBeginFileWriteResult(
      IpcFileOperations::BeginFileWriteCallback callback,
      base::CallbackListSubscription disconnect_handler_subscription,
      mojom::BeginFileWriteResultPtr result);

  void SignalWebAuthnExtension();

  // This sends a Mojo CreateVideoCapturer() request to the desktop process.
  // The response will contain Mojo endpoints which will be provided to
  // `capturer`.
  void RequestMojoVideoCapturer(webrtc::ScreenId id,
                                base::WeakPtr<IpcVideoFrameCapturer> capturer);

  // Task runners:
  //   - |audio_capturer_| is called back on |audio_capture_task_runner_|.
  //   - public methods of this class (with some exceptions) are called on
  //     |caller_task_runner| passed in the constructor.
  //   - background I/O is served on |io_task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // Points to the audio capturer receiving captured audio packets.
  base::WeakPtr<IpcAudioCapturer> audio_capturer_;

  // Points to the client stub passed to StartInputInjector().
  std::unique_ptr<protocol::ClipboardStub> client_clipboard_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to disconnect the client session.
  base::WeakPtr<ClientSessionControl> client_session_control_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to trigger events on the client session.
  base::WeakPtr<ClientSessionEvents> client_session_events_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to create a desktop session and receive notifications every time
  // the desktop process is replaced.
  base::WeakPtr<DesktopSessionConnector> desktop_session_connector_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // List of video-capturers receiving captured video frames. These are
  // stored so that, when the Desktop process is re-attached, new Mojo
  // endpoints can be requested for each capturer. WeakPtrs are needed
  // since the capturers are owned by the VideoStreams, which can be
  // independently created or destroyed when displays are added or
  // removed.
  std::map<webrtc::ScreenId, base::WeakPtr<IpcVideoFrameCapturer>>
      video_capturers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Points to the mouse cursor monitor receiving mouse cursor changes.
  base::WeakPtr<IpcMouseCursorMonitor> mouse_cursor_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Points to the keyboard layout monitor receiving keyboard layout changes.
  base::WeakPtr<IpcKeyboardLayoutMonitor> keyboard_layout_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to create IpcFileOperations instances and route result messages.
  IpcFileOperationsFactory ipc_file_operations_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // IPC channel to the desktop session agent.
  std::unique_ptr<IPC::ChannelProxy> desktop_channel_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Keeps the desired screen resolution so it can be passed to a newly attached
  // desktop session agent.
  ScreenResolution screen_resolution_ GUARDED_BY_CONTEXT(sequence_checker_);

  // True if |this| has been connected to the desktop session.
  bool is_desktop_session_connected_ GUARDED_BY_CONTEXT(sequence_checker_);

  DesktopEnvironmentOptions options_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores the session id for the proxied desktop process.
  uint32_t desktop_session_id_ = UINT32_MAX;

  // Caches the last keyboard layout received so it can be provided when Start
  // is called on IpcKeyboardLayoutMonitor.
  std::optional<protocol::KeyboardLayout> keyboard_layout_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to notify registered handlers when the IPC channel is disconnected.
  base::OnceClosureList disconnect_handlers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // |desktop_session_agent_| is only valid when |desktop_channel_| is
  // connected.
  mojo::AssociatedRemote<mojom::DesktopSessionAgent> desktop_session_agent_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // |desktop_session_control_| is only valid when |desktop_channel_| is
  // connected. The desktop process can be detached and reattached several times
  // during a session (e.g. transitioning between the login screen and user
  // desktop) so the validity of this remote must be checked before calling a
  // method on it.
  mojo::AssociatedRemote<mojom::DesktopSessionControl> desktop_session_control_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::AssociatedReceiver<mojom::DesktopSessionEventHandler>
      desktop_session_event_handler_ GUARDED_BY_CONTEXT(sequence_checker_){
          this};
  mojo::AssociatedReceiver<mojom::DesktopSessionStateHandler>
      desktop_session_state_handler_ GUARDED_BY_CONTEXT(sequence_checker_){
          this};

  UrlForwarderConfigurator::IsUrlForwarderSetUpCallback
      is_url_forwarder_set_up_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  UrlForwarderConfigurator::SetUpUrlForwarderCallback
      set_up_url_forwarder_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  mojom::UrlForwarderState current_url_forwarder_state_ GUARDED_BY_CONTEXT(
      sequence_checker_) = mojom::UrlForwarderState::kUnknown;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_PROXY_H_
