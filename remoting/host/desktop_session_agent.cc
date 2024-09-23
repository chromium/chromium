// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/crash_process.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/mojom/desktop_session.mojom-shared.h"
#include "remoting/host/remote_input_filter.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/host/video_memory_utils.h"
#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"
#include "remoting/proto/action.pb.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/input_event_tracker.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

namespace {

using SetUpUrlForwarderResponse =
    protocol::UrlForwarderControl::SetUpUrlForwarderResponse;

// Routes local clipboard events though the IPC channel to the network process.
class DesktopSessionClipboardStub : public protocol::ClipboardStub {
 public:
  explicit DesktopSessionClipboardStub(
      scoped_refptr<DesktopSessionAgent> desktop_session_agent);

  DesktopSessionClipboardStub(const DesktopSessionClipboardStub&) = delete;
  DesktopSessionClipboardStub& operator=(const DesktopSessionClipboardStub&) =
      delete;

  ~DesktopSessionClipboardStub() override;

  // protocol::ClipboardStub implementation.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

 private:
  scoped_refptr<DesktopSessionAgent> desktop_session_agent_;
};

DesktopSessionClipboardStub::DesktopSessionClipboardStub(
    scoped_refptr<DesktopSessionAgent> desktop_session_agent)
    : desktop_session_agent_(desktop_session_agent) {}

DesktopSessionClipboardStub::~DesktopSessionClipboardStub() = default;

void DesktopSessionClipboardStub::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  desktop_session_agent_->OnClipboardEvent(event);
}

}  // namespace

DesktopSessionAgent::Delegate::~Delegate() = default;

DesktopSessionAgent::DesktopSessionAgent(
    scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner)
    : audio_capture_task_runner_(audio_capture_task_runner),
      caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      io_task_runner_(io_task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

bool DesktopSessionAgent::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  NOTREACHED() << "Received unexpected IPC type: " << message.type();
}

void DesktopSessionAgent::OnChannelConnected(int32_t peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  VLOG(1) << "IPC: desktop <- network (" << peer_pid << ")";
}

void DesktopSessionAgent::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Make sure the channel is closed.
  network_channel_.reset();

  // Notify the caller that the channel has been disconnected.
  if (delegate_.get()) {
    delegate_->OnNetworkProcessDisconnected();
  }
}

void DesktopSessionAgent::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (interface_name == mojom::DesktopSessionAgent::Name_) {
    if (desktop_session_agent_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::DesktopSessionAgent::Name_;
      delegate_->CrashNetworkProcess(base::Location::Current());
    }

    mojo::PendingAssociatedReceiver<mojom::DesktopSessionAgent>
        pending_receiver(std::move(handle));
    desktop_session_agent_.Bind(std::move(pending_receiver));
  } else {
    LOG(ERROR) << "Unknown associated interface requested: " << interface_name
               << ", crashing the network process";
    delegate_->CrashNetworkProcess(base::Location::Current());
  }
}

DesktopSessionAgent::~DesktopSessionAgent() {
  DCHECK(!audio_capturer_);
  DCHECK(!desktop_environment_);
  DCHECK(!network_channel_);
  DCHECK(!screen_controls_);
  DCHECK(video_capturers_.IsEmpty());
  DCHECK(!session_file_operations_handler_);
}

const std::string& DesktopSessionAgent::client_jid() const {
  return client_jid_;
}

void DesktopSessionAgent::DisconnectSession(protocol::ErrorCode error) {
  if (desktop_session_state_handler_) {
    desktop_session_state_handler_->DisconnectSession(error);
  }
}

void DesktopSessionAgent::OnLocalKeyPressed(uint32_t usb_keycode) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  remote_input_filter_->LocalKeyPressed(usb_keycode);
}

void DesktopSessionAgent::OnLocalPointerMoved(
    const webrtc::DesktopVector& new_pos,
    ui::EventType type) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  remote_input_filter_->LocalPointerMoved(new_pos, type);
}

void DesktopSessionAgent::SetDisableInputs(bool disable_inputs) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Do not expect this method to be called because it is only used by It2Me.
  NOTREACHED();
}

void DesktopSessionAgent::OnDesktopDisplayChanged(
    std::unique_ptr<protocol::VideoLayout> layout) {
  LOG(INFO) << "DSA::OnDesktopDisplayChanged";
  for (int display_id = 0; display_id < layout->video_track_size();
       display_id++) {
    protocol::VideoTrackLayout track = layout->video_track(display_id);
    LOG(INFO) << "   #" << display_id << " : "
              << " [" << track.x_dpi() << "," << track.y_dpi() << "]";
  }
  if (desktop_session_event_handler_) {
    desktop_session_event_handler_->OnDesktopDisplayChanged(*layout);
  }
}

void DesktopSessionAgent::Start(
    const std::string& authenticated_jid,
    const ScreenResolution& resolution,
    const remoting::DesktopEnvironmentOptions& options,
    StartCallback callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!audio_capturer_);
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(video_capturers_.IsEmpty());
  DCHECK(!session_file_operations_handler_);

  if (started_) {
    LOG(ERROR) << __func__ << " called more than once for the current process.";
    delegate_->CrashNetworkProcess(base::Location::Current());
    // No need to run the callback since it just calls into the process we are
    // asking the daemon process to crash.
    callback.Reset();
    return;
  }

  started_ = true;
  client_jid_ = authenticated_jid;

  // Hook up the associated interfaces.
  network_channel_->GetRemoteAssociatedInterface(
      &desktop_session_event_handler_);
  network_channel_->GetRemoteAssociatedInterface(
      &desktop_session_state_handler_);

  // Create a desktop environment for the new session.
  desktop_environment_ = delegate_->desktop_environment_factory().Create(
      weak_factory_.GetWeakPtr(), /* client_session_events= */ nullptr,
      options);

  // Create the session controller and set the initial screen resolution.
  screen_controls_ = desktop_environment_->CreateScreenControls();
  SetScreenResolution(resolution);

  // Create the input injector.
  input_injector_ = desktop_environment_->CreateInputInjector();

  action_executor_ = desktop_environment_->CreateActionExecutor();

  // Hook up the input filter.
  input_tracker_ =
      std::make_unique<protocol::InputEventTracker>(input_injector_.get());
  remote_input_filter_ =
      std::make_unique<RemoteInputFilter>(input_tracker_.get());

#if BUILDFLAG(IS_WIN)
  // LocalInputMonitorWin filters out an echo of the injected input before it
  // reaches |remote_input_filter_|.
  remote_input_filter_->SetExpectLocalEcho(false);
#endif  // BUILDFLAG(IS_WIN)

  // Start the input injector.
  std::unique_ptr<protocol::ClipboardStub> clipboard_stub(
      new DesktopSessionClipboardStub(this));
  input_injector_->Start(std::move(clipboard_stub));

  // Start the audio capturer.
  if (delegate_->desktop_environment_factory().SupportsAudioCapture()) {
    audio_capturer_ = desktop_environment_->CreateAudioCapturer();
    audio_capture_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DesktopSessionAgent::StartAudioCapturer, this));
  }

  // Start the mouse cursor monitor.
  mouse_shape_pump_ = std::make_unique<MouseShapePump>(
      desktop_environment_->CreateMouseCursorMonitor(),
      /*CursorShapeStub*/ nullptr);
  mouse_shape_pump_->SetMouseCursorMonitorCallback(this);

  // Unretained is sound because callback will never be invoked after
  // |keyboard_layout_monitor_| is destroyed.
  keyboard_layout_monitor_ = desktop_environment_->CreateKeyboardLayoutMonitor(
      base::BindRepeating(&DesktopSessionAgent::OnKeyboardLayoutChange,
                          base::Unretained(this)));
  keyboard_layout_monitor_->Start();

  // Begin observing the desktop display(s) for changes. Note that some
  // desktop environments may not provide a display info monitor.
  auto* display_info_monitor = desktop_environment_->GetDisplayInfoMonitor();
  if (display_info_monitor) {
    display_info_monitor->Start();
  }

  // Set up the message handler for file transfers.
  session_file_operations_handler_.emplace(
      desktop_environment_->CreateFileOperations());

  url_forwarder_configurator_ =
      desktop_environment_->CreateUrlForwarderConfigurator();

  // Check and report the initial URL forwarder setup state.
  url_forwarder_configurator_->IsUrlForwarderSetUp(base::BindOnce(
      &DesktopSessionAgent::OnCheckUrlForwarderSetUpResult, this));

  webauthn_state_change_notifier_ =
      desktop_environment_->CreateRemoteWebAuthnStateChangeNotifier();

  std::move(callback).Run(
      desktop_session_control_.BindNewEndpointAndPassRemote());
}

void DesktopSessionAgent::OnMouseCursor(webrtc::MouseCursor* cursor) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<webrtc::MouseCursor> owned_cursor(cursor);

  if (desktop_session_event_handler_) {
    desktop_session_event_handler_->OnMouseCursorChanged(*owned_cursor);
  }

  video_capturers_.SetMouseCursor(*owned_cursor);
}

void DesktopSessionAgent::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  video_capturers_.SetMouseCursorPosition(position);
}

void DesktopSessionAgent::OnClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_) << "OnClipboardEvent called before agent was started.";

  if (desktop_session_event_handler_) {
    desktop_session_event_handler_->OnClipboardEvent(event);
  }
}

void DesktopSessionAgent::ProcessAudioPacket(
    std::unique_ptr<AudioPacket> packet) {
  // AudioPackets are received on the audio_capture task runner but must be sent
  // over IPC on the same task_runner the mojo remote was bound on.
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DesktopSessionAgent::ProcessAudioPacket,
                                  this, std::move(packet)));
    return;
  }

  if (desktop_session_event_handler_) {
    desktop_session_event_handler_->OnAudioPacket(std::move(packet));
  }
}

mojo::ScopedMessagePipeHandle DesktopSessionAgent::Initialize(
    const base::WeakPtr<Delegate>& delegate) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate);
  DCHECK(!delegate_);

  delegate_ = delegate;

  mojo::MessagePipe pipe;
  network_channel_ = IPC::ChannelProxy::Create(
      pipe.handle0.release(), IPC::Channel::MODE_SERVER, this, io_task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  return std::move(pipe.handle1);
}

void DesktopSessionAgent::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  delegate_.reset();

  // Make sure the channel is closed.
  network_channel_.reset();

  if (started_) {
    started_ = false;

    // Ignore any further callbacks.
    weak_factory_.InvalidateWeakPtrs();
    client_jid_.clear();

    desktop_session_event_handler_.reset();
    desktop_session_state_handler_.reset();
    desktop_session_control_.reset();
    desktop_session_agent_.reset();

    url_forwarder_configurator_.reset();
    webauthn_state_change_notifier_.reset();

    remote_input_filter_.reset();

    session_file_operations_handler_.reset();

    // Ensure that any pressed keys or buttons are released.
    input_tracker_->ReleaseAll();
    input_tracker_.reset();

    desktop_environment_.reset();
    action_executor_.reset();
    input_injector_.reset();
    screen_controls_.reset();
    keyboard_layout_monitor_.reset();

    // Stop the audio capturer.
    audio_capture_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DesktopSessionAgent::StopAudioCapturer, this));

    // Stop the video capturers.
    video_capturers_.Clear();
    mouse_shape_pump_.reset();
  }
}

void DesktopSessionAgent::CreateVideoCapturer(
    int64_t desktop_display_id,
    CreateVideoCapturerCallback callback) {
  std::move(callback).Run(video_capturers_.CreateVideoCapturer(
      desktop_display_id, desktop_environment_.get(), caller_task_runner_));
}

void DesktopSessionAgent::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  // InputStub implementations must verify events themselves, so we don't need
  // verification here. This matches HostEventDispatcher.
  input_injector_->InjectClipboardEvent(event);
}

void DesktopSessionAgent::InjectKeyEvent(const protocol::KeyEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  // InputStub implementations must verify events themselves, so we need only
  // basic verification here. This matches HostEventDispatcher.
  if (!event.has_usb_keycode() || !event.has_pressed()) {
    LOG(ERROR) << "Received invalid key event.";
    return;
  }

  remote_input_filter_->InjectKeyEvent(event);
}

void DesktopSessionAgent::InjectTextEvent(const protocol::TextEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  // InputStub implementations must verify events themselves, so we need only
  // basic verification here. This matches HostEventDispatcher.
  if (!event.has_text()) {
    LOG(ERROR) << "Received invalid TextEvent.";
    return;
  }

  remote_input_filter_->InjectTextEvent(event);
}

void DesktopSessionAgent::InjectMouseEvent(const protocol::MouseEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  video_capturers_.SetComposeEnabled(event.has_delta_x() ||
                                     event.has_delta_y());

  // InputStub implementations must verify events themselves, so we don't need
  // verification here. This matches HostEventDispatcher.
  remote_input_filter_->InjectMouseEvent(event);
}

void DesktopSessionAgent::InjectTouchEvent(const protocol::TouchEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  remote_input_filter_->InjectTouchEvent(event);
}

void DesktopSessionAgent::InjectSendAttentionSequence() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  protocol::ActionRequest request;
  request.set_action(protocol::ActionRequest::SEND_ATTENTION_SEQUENCE);
  action_executor_->ExecuteAction(request);
}

void DesktopSessionAgent::LockWorkstation() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  protocol::ActionRequest request;
  request.set_action(protocol::ActionRequest::LOCK_WORKSTATION);
  action_executor_->ExecuteAction(request);
}

void DesktopSessionAgent::OnKeyboardLayoutChange(
    const protocol::KeyboardLayout& layout) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  if (desktop_session_event_handler_) {
    desktop_session_event_handler_->OnKeyboardLayoutChanged(layout);
  }
}

void DesktopSessionAgent::SetScreenResolution(
    const ScreenResolution& resolution) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  if (screen_controls_) {
    screen_controls_->SetScreenResolution(resolution, std::nullopt);
  }
}

void DesktopSessionAgent::StartAudioCapturer() {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  if (audio_capturer_) {
    audio_capturer_->Start(
        base::BindRepeating(&DesktopSessionAgent::ProcessAudioPacket, this));
  }
}

void DesktopSessionAgent::StopAudioCapturer() {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  audio_capturer_.reset();
}

void DesktopSessionAgent::SetUpUrlForwarder() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  url_forwarder_configurator_->SetUpUrlForwarder(base::BindRepeating(
      &DesktopSessionAgent::OnUrlForwarderSetUpStateChanged, this));
}

void DesktopSessionAgent::SignalWebAuthnExtension() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  webauthn_state_change_notifier_->NotifyStateChange();
}

void DesktopSessionAgent::BeginFileRead(BeginFileReadCallback callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  session_file_operations_handler_->BeginFileRead(std::move(callback));
}

void DesktopSessionAgent::BeginFileWrite(const base::FilePath& file_path,
                                         BeginFileWriteCallback callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  CHECK(started_);

  session_file_operations_handler_->BeginFileWrite(file_path,
                                                   std::move(callback));
}

void DesktopSessionAgent::OnCheckUrlForwarderSetUpResult(bool is_set_up) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  if (!desktop_session_event_handler_) {
    // Callback passed to UrlForwarderConfiguratorWin::IsUrlForwarderSetUp()
    // may be called after the configurator is deleted and the agent is
    // stopped, so we need to null-check |desktop_session_event_handler_|.
    //
    // TODO(yuweih): Scope callback of IsUrlForwarderSetUp() to the lifetime of
    //     the UrlForwarderConfiguratorWin instance.
    return;
  }
  desktop_session_event_handler_->OnUrlForwarderStateChange(
      is_set_up ? mojom::UrlForwarderState::kSetUp
                : mojom::UrlForwarderState::kNotSetUp);
}

void DesktopSessionAgent::OnUrlForwarderSetUpStateChanged(
    SetUpUrlForwarderResponse::State state) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  mojom::UrlForwarderState mojo_state;
  switch (state) {
    case SetUpUrlForwarderResponse::COMPLETE:
      mojo_state = mojom::UrlForwarderState::kSetUp;
      break;
    case SetUpUrlForwarderResponse::FAILED:
      mojo_state = mojom::UrlForwarderState::kFailed;
      break;
    case SetUpUrlForwarderResponse::USER_INTERVENTION_REQUIRED:
      mojo_state = mojom::UrlForwarderState::kSetupPendingUserIntervention;
      break;
    default:
      NOTREACHED() << "Unknown state: " << state;
  }
  desktop_session_event_handler_->OnUrlForwarderStateChange(mojo_state);
}

}  // namespace remoting
