// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_proxy.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/base/capabilities.h"
#include "remoting/host/client_session.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/crash_process.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/ipc_action_executor.h"
#include "remoting/host/ipc_audio_capturer.h"
#include "remoting/host/ipc_input_injector.h"
#include "remoting/host/ipc_keyboard_layout_monitor.h"
#include "remoting/host/ipc_mouse_cursor_monitor.h"
#include "remoting/host/ipc_screen_controls.h"
#include "remoting/host/ipc_url_forwarder_configurator.h"
#include "remoting/host/ipc_video_frame_capturer.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/remote_open_url/remote_open_url_util.h"
#include "remoting/host/webauthn/remote_webauthn_delegated_state_change_notifier.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/capability_names.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

using SetUpUrlForwarderResponse =
    protocol::UrlForwarderControl::SetUpUrlForwarderResponse;

DesktopSessionProxy::DesktopSessionProxy(
    scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<ClientSessionEvents> client_session_events,
    base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
    const DesktopEnvironmentOptions& options)
    : base::RefCountedDeleteOnSequence<DesktopSessionProxy>(
          base::SequencedTaskRunner::GetCurrentDefault()),
      audio_capture_task_runner_(audio_capture_task_runner),
      io_task_runner_(io_task_runner),
      client_session_control_(client_session_control),
      client_session_events_(client_session_events),
      desktop_session_connector_(desktop_session_connector),
      ipc_file_operations_factory_(this),
      is_desktop_session_connected_(false),
      options_(options) {}

std::unique_ptr<ActionExecutor> DesktopSessionProxy::CreateActionExecutor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<IpcActionExecutor>(this);
}

std::unique_ptr<AudioCapturer> DesktopSessionProxy::CreateAudioCapturer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<IpcAudioCapturer>(this);
}

std::unique_ptr<InputInjector> DesktopSessionProxy::CreateInputInjector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<IpcInputInjector>(this);
}

std::unique_ptr<ScreenControls> DesktopSessionProxy::CreateScreenControls() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<IpcScreenControls>(this);
}

std::unique_ptr<DesktopCapturer> DesktopSessionProxy::CreateVideoCapturer(
    webrtc::ScreenId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cursor compositing is done by the desktop process if necessary so just
  // return a non-composing frame capturer.
  auto video_capturer = std::make_unique<IpcVideoFrameCapturer>(this);

  DCHECK(!base::FindPtrOrNull(video_capturers_, id))
      << "Multiple capturers created for screen-id " << id;
  auto capturer_weakptr = video_capturer->GetWeakPtr();
  video_capturers_[id] = capturer_weakptr;

  // If the session-control endpoint is not bound, the Mojo endpoints will
  // be requested for each IpcVideFrameCapturer when the Desktop process
  // becomes attached.
  if (desktop_session_control_) {
    RequestMojoVideoCapturer(id, capturer_weakptr);
  }

  return video_capturer;
}

std::unique_ptr<webrtc::MouseCursorMonitor>
DesktopSessionProxy::CreateMouseCursorMonitor() {
  return std::make_unique<IpcMouseCursorMonitor>(this);
}

std::unique_ptr<KeyboardLayoutMonitor>
DesktopSessionProxy::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<IpcKeyboardLayoutMonitor>(std::move(callback), this);
}

std::unique_ptr<FileOperations> DesktopSessionProxy::CreateFileOperations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return ipc_file_operations_factory_.CreateFileOperations();
}

std::unique_ptr<UrlForwarderConfigurator>
DesktopSessionProxy::CreateUrlForwarderConfigurator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<IpcUrlForwarderConfigurator>(this);
}

std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
DesktopSessionProxy::CreateRemoteWebAuthnStateChangeNotifier() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<RemoteWebAuthnDelegatedStateChangeNotifier>(
      base::BindRepeating(&DesktopSessionProxy::SignalWebAuthnExtension, this));
}

std::string DesktopSessionProxy::GetCapabilities() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string result = protocol::kRateLimitResizeRequests;
  result += " ";
  result += protocol::kMultiStreamCapability;

  // Ask the client to send its resolution unconditionally.
  if (options_.enable_curtaining()) {
    result += " ";
    result += protocol::kSendInitialResolution;
  }

  if (InputInjector::SupportsTouchEvents()) {
    result += " ";
    result += protocol::kTouchEventsCapability;
  }

  if (options_.enable_remote_webauthn()) {
    result += " ";
    result += protocol::kRemoteWebAuthnCapability;
  }

  return result;
}

void DesktopSessionProxy::SetCapabilities(const std::string& capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Delay creation of the desktop session until the client screen resolution is
  // received if the desktop session requires the initial screen resolution
  // (when enable_curtaining() is true) and the client is expected to
  // sent its screen resolution (the 'sendInitialResolution' capability is
  // supported).
  if (options_.enable_curtaining() &&
      HasCapability(capabilities, protocol::kSendInitialResolution)) {
    VLOG(1) << "Waiting for the client screen resolution.";
    return;
  }

  // Connect to the desktop session.
  if (!is_desktop_session_connected_) {
    is_desktop_session_connected_ = true;
    if (desktop_session_connector_.get()) {
      desktop_session_connector_->ConnectTerminal(this, screen_resolution_,
                                                  options_.enable_curtaining());
    }
  }
}

bool DesktopSessionProxy::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Received unexpected IPC type: " << message.type();
}

void DesktopSessionProxy::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "IPC: network <- desktop (" << peer_pid << ")";

  desktop_session_agent_->Start(
      client_session_control_->client_jid(), screen_resolution_, options_,
      base::BindOnce(&DesktopSessionProxy::OnDesktopSessionAgentStarted,
                     base::Unretained(this)));
}

void DesktopSessionProxy::OnChannelError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DetachFromDesktop();
}

void DesktopSessionProxy::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (interface_name == mojom::DesktopSessionEventHandler::Name_) {
    if (desktop_session_event_handler_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::DesktopSessionEventHandler::Name_;
      CrashProcess(base::Location::Current());
    }

    mojo::PendingAssociatedReceiver<mojom::DesktopSessionEventHandler>
        pending_receiver(std::move(handle));
    desktop_session_event_handler_.Bind(std::move(pending_receiver));
  } else if (interface_name == mojom::DesktopSessionStateHandler::Name_) {
    if (desktop_session_state_handler_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::DesktopSessionStateHandler::Name_;
      CrashProcess(base::Location::Current());
    }

    mojo::PendingAssociatedReceiver<mojom::DesktopSessionStateHandler>
        pending_receiver(std::move(handle));
    desktop_session_state_handler_.Bind(std::move(pending_receiver));
  } else {
    LOG(ERROR) << "Unknown associated interface requested: " << interface_name
               << ", crashing this process";
    CrashProcess(base::Location::Current());
  }
}

bool DesktopSessionProxy::AttachToDesktop(
    mojo::ScopedMessagePipeHandle desktop_pipe,
    int session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!desktop_channel_);

  // Ignore the attach event if the client session has already disconnected.
  if (!client_session_control_.get()) {
    return false;
  }

  // Connect to the desktop process.
  desktop_channel_ = IPC::ChannelProxy::Create(
      desktop_pipe.release(), IPC::Channel::MODE_CLIENT, this,
      io_task_runner_.get(), base::SingleThreadTaskRunner::GetCurrentDefault());

  // Reset the associated remote to allow us to connect to the new desktop
  // process. This is needed as the desktop may crash and the daemon process
  // will restart it however the remote will still be bound to the previous
  // process since DetachFromDesktop() will not be called.
  desktop_session_agent_.reset();
  desktop_channel_->GetRemoteAssociatedInterface(&desktop_session_agent_);

  desktop_session_id_ = session_id;

  return true;
}

void DesktopSessionProxy::DetachFromDesktop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  desktop_channel_.reset();
  desktop_session_agent_.reset();
  desktop_session_control_.reset();
  desktop_session_event_handler_.reset();
  desktop_session_state_handler_.reset();
  desktop_session_id_ = UINT32_MAX;

  current_url_forwarder_state_ = mojom::UrlForwarderState::kUnknown;
  // We don't reset |is_url_forwarder_set_up_callback_| here since the request
  // can come in before the DetachFromDesktop-AttachToDesktop sequence.

  // Notify interested folks that the IPC has been disconnected.
  disconnect_handlers_.Notify();

  if (client_session_events_) {
    client_session_events_->OnDesktopDetached();
  }
}

void DesktopSessionProxy::OnDesktopSessionAgentStarted(
    mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>
        pending_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reset the associated remote to allow us to connect to the new desktop
  // process. This is needed as the desktop may crash and the daemon process
  // will restart it however the remote will still be bound to the previous
  // process since DetachFromDesktop() will not be called.
  desktop_session_control_.reset();
  desktop_session_control_.Bind(std::move(pending_remote));

  // Create new capturers in the Desktop process and bind the Mojo endpoints
  // to each video-capturer.
  for (auto& [id, capturer] : video_capturers_) {
    if (capturer) {
      RequestMojoVideoCapturer(id, capturer);
    }
  }

  if (client_session_events_) {
    client_session_events_->OnDesktopAttached(desktop_session_id_);
  }
}

void DesktopSessionProxy::SetAudioCapturer(
    const base::WeakPtr<IpcAudioCapturer>& audio_capturer) {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  audio_capturer_ = audio_capturer;
}

void DesktopSessionProxy::SetMouseCursorMonitor(
    const base::WeakPtr<IpcMouseCursorMonitor>& mouse_cursor_monitor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mouse_cursor_monitor_ = mouse_cursor_monitor;
}

void DesktopSessionProxy::SetKeyboardLayoutMonitor(
    const base::WeakPtr<IpcKeyboardLayoutMonitor>& keyboard_layout_monitor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  keyboard_layout_monitor_ = std::move(keyboard_layout_monitor);
}

void DesktopSessionProxy::RebindSingleVideoCapturer(
    webrtc::ScreenId new_id,
    base::WeakPtr<IpcVideoFrameCapturer> capturer_weakptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // SelectSource() is not used in multi-stream mode.
  DCHECK_LE(video_capturers_.size(), 1U);

  if (base::FindPtrOrNull(video_capturers_, new_id) == capturer_weakptr.get()) {
    // The capturer is already bound to `new_id`, so there's no value in
    // recreating it.
    LOG(WARNING) << "Ignoring SelectSource() for the same ID: " << new_id;
    return;
  }

  video_capturers_.clear();
  video_capturers_[new_id] = capturer_weakptr;

  // If the session-control endpoint is not bound, the Mojo endpoints will
  // be requested for each IpcVideFrameCapturer when the Desktop process
  // becomes attached.
  if (desktop_session_control_) {
    RequestMojoVideoCapturer(new_id, capturer_weakptr);
  }
}

const std::optional<protocol::KeyboardLayout>&
DesktopSessionProxy::GetKeyboardCurrentLayout() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return keyboard_layout_;
}

void DesktopSessionProxy::DisconnectSession(protocol::ErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Disconnect the client session if it hasn't been disconnected yet.
  if (client_session_control_.get()) {
    client_session_control_->DisconnectSession(error);
  }
}

void DesktopSessionProxy::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (desktop_session_control_) {
    desktop_session_control_->InjectClipboardEvent(event);
  }
}

void DesktopSessionProxy::InjectKeyEvent(const protocol::KeyEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (desktop_session_control_) {
    desktop_session_control_->InjectKeyEvent(event);
  }
}

void DesktopSessionProxy::InjectTextEvent(const protocol::TextEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (desktop_session_control_) {
    desktop_session_control_->InjectTextEvent(event);
  }
}

void DesktopSessionProxy::InjectMouseEvent(const protocol::MouseEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (desktop_session_control_) {
    desktop_session_control_->InjectMouseEvent(event);
  }
}

void DesktopSessionProxy::InjectTouchEvent(const protocol::TouchEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (desktop_session_control_) {
    desktop_session_control_->InjectTouchEvent(event);
  }
}

void DesktopSessionProxy::StartInputInjector(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_clipboard_ = std::move(client_clipboard);
}

void DesktopSessionProxy::SetScreenResolution(
    const ScreenResolution& resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  screen_resolution_ = resolution;

  // Connect to the desktop session if it is not done yet.
  if (!is_desktop_session_connected_) {
    is_desktop_session_connected_ = true;
    if (desktop_session_connector_.get()) {
      desktop_session_connector_->ConnectTerminal(this, screen_resolution_,
                                                  options_.enable_curtaining());
    }
    return;
  }

  // Pass the client's resolution to both daemon and desktop session agent.
  // Depending on the session kind the screen resolution can be set by either
  // the daemon (for example RDP sessions on Windows) or by the desktop session
  // agent (when sharing the physical console).
  // Desktop-size-restore functionality (via an empty resolution param) does not
  // exist for the Daemon process.  Passing an empty resolution object is
  // treated as a critical error so we want to prevent that here.
  if (desktop_session_connector_.get() && !screen_resolution_.IsEmpty()) {
    desktop_session_connector_->SetScreenResolution(this, screen_resolution_);
  }

  // Passing an empty |screen_resolution_| value to the desktop process
  // indicates that the original resolution, if one exists, should be restored.
  if (desktop_session_control_) {
    desktop_session_control_->SetScreenResolution(screen_resolution_);
  }
}

void DesktopSessionProxy::ExecuteAction(
    const protocol::ActionRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!desktop_session_control_) {
    return;
  }

  switch (request.action()) {
    case protocol::ActionRequest::LOCK_WORKSTATION:
      desktop_session_control_->LockWorkstation();
      break;
    case protocol::ActionRequest::SEND_ATTENTION_SEQUENCE:
      desktop_session_control_->InjectSendAttentionSequence();
      break;
    default:
      LOG(WARNING) << "Unknown action requested: " << request.action();
  }
}

void DesktopSessionProxy::BeginFileRead(
    IpcFileOperations::BeginFileReadCallback callback,
    base::OnceClosure on_disconnect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!desktop_session_control_) {
    std::move(callback).Run(
        mojom::BeginFileReadResult::NewError(protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR)));
    return;
  }

  auto disconnect_handler_subscription =
      disconnect_handlers_.Add(std::move(on_disconnect));
  // Unretained is sound as DesktopSessionProxy owns |desktop_session_control_|
  // and the callback won't be invoked after the remote is destroyed.
  desktop_session_control_->BeginFileRead(base::BindOnce(
      &DesktopSessionProxy::OnBeginFileReadResult, base::Unretained(this),
      std::move(callback), std::move(disconnect_handler_subscription)));
}

void DesktopSessionProxy::BeginFileWrite(
    const base::FilePath& file_path,
    IpcFileOperations::BeginFileWriteCallback callback,
    base::OnceClosure on_disconnect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!desktop_session_control_) {
    std::move(callback).Run(
        mojom::BeginFileWriteResult::NewError(protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR)));
    return;
  }

  auto disconnect_handler_subscription =
      disconnect_handlers_.Add(std::move(on_disconnect));
  // Unretained is sound as DesktopSessionProxy owns |desktop_session_control_|
  // and the callback won't be invoked after the remote is destroyed.
  desktop_session_control_->BeginFileWrite(
      file_path, base::BindOnce(&DesktopSessionProxy::OnBeginFileWriteResult,
                                base::Unretained(this), std::move(callback),
                                std::move(disconnect_handler_subscription)));
}

void DesktopSessionProxy::IsUrlForwarderSetUp(
    UrlForwarderConfigurator::IsUrlForwarderSetUpCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (current_url_forwarder_state_) {
    case mojom::UrlForwarderState::kUnknown:
      // State is not known yet. Wait for OnUrlForwarderStateChange() to be
      // called.
      DCHECK(!is_url_forwarder_set_up_callback_);
      is_url_forwarder_set_up_callback_ = std::move(callback);
      break;
    case mojom::UrlForwarderState::kSetUp:
      std::move(callback).Run(true);
      break;
    default:
      std::move(callback).Run(false);
  }
}

void DesktopSessionProxy::SetUpUrlForwarder(
    const UrlForwarderConfigurator::SetUpUrlForwarderCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!set_up_url_forwarder_callback_);

  if (!desktop_session_control_) {
    LOG(ERROR) << "The UrlForwarderConfigurator remote is not connected. Setup "
               << "request ignored.";
    callback.Run(SetUpUrlForwarderResponse::FAILED);
    return;
  }
  set_up_url_forwarder_callback_ = callback;
  desktop_session_control_->SetUpUrlForwarder();
}

void DesktopSessionProxy::OnUrlForwarderStateChange(
    mojom::UrlForwarderState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_url_forwarder_state_ = state;

  if (is_url_forwarder_set_up_callback_) {
    std::move(is_url_forwarder_set_up_callback_)
        .Run(state == mojom::UrlForwarderState::kSetUp);
  }

  if (set_up_url_forwarder_callback_) {
    switch (state) {
      case mojom::UrlForwarderState::kSetUp:
        set_up_url_forwarder_callback_.Run(SetUpUrlForwarderResponse::COMPLETE);
        // Cleanup callback due to terminating state.
        set_up_url_forwarder_callback_.Reset();
        break;
      case mojom::UrlForwarderState::kNotSetUp:
        // The desktop session agent during the setup process will only report
        // SET_UP or FAILED. NOT_SET_UP must come from a freshly started agent.
        LOG(WARNING) << "Setup process failed because the previous desktop "
                     << "session agent has exited";
        [[fallthrough]];
      case mojom::UrlForwarderState::kFailed:
        set_up_url_forwarder_callback_.Run(SetUpUrlForwarderResponse::FAILED);
        // Cleanup callback due to terminating state.
        set_up_url_forwarder_callback_.Reset();
        break;
      case mojom::UrlForwarderState::kSetupPendingUserIntervention:
        set_up_url_forwarder_callback_.Run(
            SetUpUrlForwarderResponse::USER_INTERVENTION_REQUIRED);
        break;
      default:
        LOG(ERROR) << "Received unexpected state: " << state;
    }
  }
}

DesktopSessionProxy::~DesktopSessionProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (desktop_session_connector_.get() && is_desktop_session_connected_) {
    desktop_session_connector_->DisconnectTerminal(this);
  }
}

void DesktopSessionProxy::OnAudioPacket(
    std::unique_ptr<AudioPacket> audio_packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Pass the captured audio packet to |audio_capturer_|.
  audio_capture_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&IpcAudioCapturer::OnAudioPacket,
                                audio_capturer_, std::move(audio_packet)));
}

void DesktopSessionProxy::OnDesktopDisplayChanged(
    const protocol::VideoLayout& displays) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "DSP::OnDesktopDisplayChanged";
  for (int display_id = 0; display_id < displays.video_track_size();
       display_id++) {
    protocol::VideoTrackLayout track = displays.video_track(display_id);
    LOG(INFO) << "   #" << display_id << " : "
              << " [" << track.x_dpi() << "," << track.y_dpi() << "]";
  }

  if (client_session_control_) {
    auto layout = std::make_unique<protocol::VideoLayout>();
    layout->CopyFrom(displays);
    client_session_control_->OnDesktopDisplayChanged(std::move(layout));
  }
}

void DesktopSessionProxy::OnBeginFileReadResult(
    IpcFileOperations::BeginFileReadCallback callback,
    base::CallbackListSubscription disconnect_handler_subscription,
    mojom::BeginFileReadResultPtr result) {
  // This handler is only needed until the pending_remote is returned from the
  // DesktopSessionAgent as the IpcFileReader will then use the new channel and
  // hook into its disconnect handler.
  disconnect_handler_subscription = {};
  std::move(callback).Run(std::move(result));
}

void DesktopSessionProxy::OnBeginFileWriteResult(
    IpcFileOperations::BeginFileWriteCallback callback,
    base::CallbackListSubscription disconnect_handler_subscription,
    mojom::BeginFileWriteResultPtr result) {
  // This handler is only needed until the pending_remote is returned from the
  // DesktopSessionAgent as the IpcFileWriter will then use the new channel and
  // hook into its disconnect handler.
  disconnect_handler_subscription = {};
  std::move(callback).Run(std::move(result));
}

void DesktopSessionProxy::OnMouseCursorChanged(
    const webrtc::MouseCursor& mouse_cursor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (mouse_cursor_monitor_) {
    mouse_cursor_monitor_->OnMouseCursor(
        base::WrapUnique(webrtc::MouseCursor::CopyOf(mouse_cursor)));
  }
}

void DesktopSessionProxy::OnKeyboardLayoutChanged(
    const protocol::KeyboardLayout& layout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  keyboard_layout_ = layout;
  if (keyboard_layout_monitor_) {
    keyboard_layout_monitor_->OnKeyboardChanged(layout);
  }
}

void DesktopSessionProxy::OnClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (client_clipboard_) {
    client_clipboard_->InjectClipboardEvent(event);
  }
}

void DesktopSessionProxy::SignalWebAuthnExtension() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (desktop_session_control_) {
    desktop_session_control_->SignalWebAuthnExtension();
  }
}

void DesktopSessionProxy::RequestMojoVideoCapturer(
    webrtc::ScreenId id,
    base::WeakPtr<IpcVideoFrameCapturer> capturer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(desktop_session_control_);
  DCHECK(capturer);
  desktop_session_control_->CreateVideoCapturer(
      id, base::BindOnce(&IpcVideoFrameCapturer::OnCreateVideoCapturerResult,
                         capturer));
}

}  // namespace remoting
