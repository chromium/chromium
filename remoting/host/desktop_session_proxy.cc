// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_proxy.h"

#include <stddef.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/capabilities.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/ipc_action_executor.h"
#include "remoting/host/ipc_audio_capturer.h"
#include "remoting/host/ipc_input_injector.h"
#include "remoting/host/ipc_mouse_cursor_monitor.h"
#include "remoting/host/ipc_screen_controls.h"
#include "remoting/host/ipc_video_frame_capturer.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/capability_names.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

const bool kReadOnly = true;

namespace remoting {

class DesktopSessionProxy::IpcSharedBufferCore
    : public base::RefCountedThreadSafe<IpcSharedBufferCore> {
 public:
  IpcSharedBufferCore(int id,
                      base::SharedMemoryHandle handle,
                      size_t size)
      : id_(id),
        shared_memory_(handle, kReadOnly),
        size_(size) {
    if (!shared_memory_.Map(size)) {
      LOG(ERROR) << "Failed to map a shared buffer: id=" << id
                 << ", size=" << size;
    }
  }

  int id() { return id_; }
  size_t size() { return size_; }
  void* memory() { return shared_memory_.memory(); }

 private:
  virtual ~IpcSharedBufferCore() = default;
  friend class base::RefCountedThreadSafe<IpcSharedBufferCore>;

  int id_;
  base::SharedMemory shared_memory_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(IpcSharedBufferCore);
};

class DesktopSessionProxy::IpcSharedBuffer : public webrtc::SharedMemory {
 public:
  IpcSharedBuffer(scoped_refptr<IpcSharedBufferCore> core)
      : SharedMemory(core->memory(), core->size(), 0, core->id()),
        core_(core) {}

 private:
  scoped_refptr<IpcSharedBufferCore> core_;

  DISALLOW_COPY_AND_ASSIGN(IpcSharedBuffer);
};

DesktopSessionProxy::DesktopSessionProxy(
    scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
    const DesktopEnvironmentOptions& options)
    : audio_capture_task_runner_(audio_capture_task_runner),
      caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      client_session_control_(client_session_control),
      desktop_session_connector_(desktop_session_connector),
      pending_capture_frame_requests_(0),
      is_desktop_session_connected_(false),
      options_(options) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

std::unique_ptr<ActionExecutor> DesktopSessionProxy::CreateActionExecutor() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return std::make_unique<IpcActionExecutor>(this);
}

std::unique_ptr<AudioCapturer> DesktopSessionProxy::CreateAudioCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return std::make_unique<IpcAudioCapturer>(this);
}

std::unique_ptr<InputInjector> DesktopSessionProxy::CreateInputInjector() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return std::make_unique<IpcInputInjector>(this);
}

std::unique_ptr<ScreenControls> DesktopSessionProxy::CreateScreenControls() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return std::make_unique<IpcScreenControls>(this);
}

std::unique_ptr<webrtc::DesktopCapturer>
DesktopSessionProxy::CreateVideoCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return std::make_unique<IpcVideoFrameCapturer>(this);
}

std::unique_ptr<webrtc::MouseCursorMonitor>
DesktopSessionProxy::CreateMouseCursorMonitor() {
  return std::make_unique<IpcMouseCursorMonitor>(this);
}

std::string DesktopSessionProxy::GetCapabilities() const {
  std::string result = protocol::kRateLimitResizeRequests;
  // Ask the client to send its resolution unconditionally.
  if (options_.enable_curtaining()) {
    result += " ";
    result += protocol::kSendInitialResolution;
  }

  if (InputInjector::SupportsTouchEvents()) {
    result += " ";
    result += protocol::kTouchEventsCapability;
  }

  return result;
}

void DesktopSessionProxy::SetCapabilities(const std::string& capabilities) {
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
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DesktopSessionProxy, message)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_AudioPacket,
                        OnAudioPacket)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_CaptureResult,
                        OnCaptureResult)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_MouseCursor,
                        OnMouseCursor)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_CreateSharedBuffer,
                        OnCreateSharedBuffer)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_ReleaseSharedBuffer,
                        OnReleaseSharedBuffer)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_InjectClipboardEvent,
                        OnInjectClipboardEvent)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_DisconnectSession,
                        DisconnectSession);
  IPC_END_MESSAGE_MAP()

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
  return handled;
}

void DesktopSessionProxy::OnChannelConnected(int32_t peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  VLOG(1) << "IPC: network <- desktop (" << peer_pid << ")";
}

void DesktopSessionProxy::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DetachFromDesktop();
}

bool DesktopSessionProxy::AttachToDesktop(
    const IPC::ChannelHandle& desktop_pipe,
    int session_id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!desktop_channel_);

  // Ignore the attach notification if the client session has been disconnected
  // already.
  if (!client_session_control_.get())
    return false;

  // Connect to the desktop process.
  desktop_channel_ = IPC::ChannelProxy::Create(
      desktop_pipe, IPC::Channel::MODE_CLIENT, this, io_task_runner_.get(),
      base::ThreadTaskRunnerHandle::Get());

  // Pass ID of the client (which is authenticated at this point) to the desktop
  // session agent and start the agent.
  SendToDesktop(new ChromotingNetworkDesktopMsg_StartSessionAgent(
      client_session_control_->client_jid(), screen_resolution_, options_));

  desktop_session_id_ = session_id;

  return true;
}

void DesktopSessionProxy::DetachFromDesktop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  desktop_channel_.reset();
  desktop_session_id_ = UINT32_MAX;

  shared_buffers_.clear();

  // Generate fake responses to keep the video capturer in sync.
  while (pending_capture_frame_requests_) {
    --pending_capture_frame_requests_;
    video_capturer_->OnCaptureResult(
        webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
  }
}

void DesktopSessionProxy::SetAudioCapturer(
    const base::WeakPtr<IpcAudioCapturer>& audio_capturer) {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  audio_capturer_ = audio_capturer;
}

void DesktopSessionProxy::CaptureFrame() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (desktop_channel_) {
    ++pending_capture_frame_requests_;
    SendToDesktop(new ChromotingNetworkDesktopMsg_CaptureFrame());
  } else {
    video_capturer_->OnCaptureResult(
        webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
  }
}

void DesktopSessionProxy::SetVideoCapturer(
    const base::WeakPtr<IpcVideoFrameCapturer> video_capturer) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  video_capturer_ = video_capturer;
}

void DesktopSessionProxy::SetMouseCursorMonitor(
    const base::WeakPtr<IpcMouseCursorMonitor>& mouse_cursor_monitor) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  mouse_cursor_monitor_ = mouse_cursor_monitor;
}

void DesktopSessionProxy::DisconnectSession(protocol::ErrorCode error) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Disconnect the client session if it hasn't been disconnected yet.
  if (client_session_control_.get())
    client_session_control_->DisconnectSession(error);
}

void DesktopSessionProxy::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::ClipboardEvent.";
    return;
  }

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InjectClipboardEvent(serialized_event));
}

void DesktopSessionProxy::InjectKeyEvent(const protocol::KeyEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::KeyEvent.";
    return;
  }

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InjectKeyEvent(serialized_event));
}

void DesktopSessionProxy::InjectTextEvent(const protocol::TextEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::TextEvent.";
    return;
  }

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InjectTextEvent(serialized_event));
}

void DesktopSessionProxy::InjectMouseEvent(const protocol::MouseEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::MouseEvent.";
    return;
  }

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InjectMouseEvent(serialized_event));
}

void DesktopSessionProxy::InjectTouchEvent(const protocol::TouchEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::TouchEvent.";
    return;
  }

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InjectTouchEvent(serialized_event));
}

void DesktopSessionProxy::StartInputInjector(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  client_clipboard_ = std::move(client_clipboard);
}

void DesktopSessionProxy::SetScreenResolution(
    const ScreenResolution& resolution) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

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
  if (desktop_session_connector_.get() && !screen_resolution_.IsEmpty())
    desktop_session_connector_->SetScreenResolution(this, screen_resolution_);

  // Passing an empty |screen_resolution_| value to the desktop process
  // indicates that the original resolution, if one exists, should be restored.
  SendToDesktop(
      new ChromotingNetworkDesktopMsg_SetScreenResolution(screen_resolution_));
}

void DesktopSessionProxy::ExecuteAction(
    const protocol::ActionRequest& request) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  SendToDesktop(new ChromotingNetworkDesktopMsg_ExecuteActionRequest(request));
}

DesktopSessionProxy::~DesktopSessionProxy() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (desktop_session_connector_.get() && is_desktop_session_connected_)
    desktop_session_connector_->DisconnectTerminal(this);
}

scoped_refptr<DesktopSessionProxy::IpcSharedBufferCore>
DesktopSessionProxy::GetSharedBufferCore(int id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  SharedBuffers::const_iterator i = shared_buffers_.find(id);
  if (i != shared_buffers_.end()) {
    return i->second;
  } else {
    LOG(ERROR) << "Failed to find the shared buffer " << id;
    return nullptr;
  }
}

void DesktopSessionProxy::OnAudioPacket(const std::string& serialized_packet) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Parse a serialized audio packet. No further validation is done since
  // the message was sent by more privileged process.
  std::unique_ptr<AudioPacket> packet(new AudioPacket());
  if (!packet->ParseFromString(serialized_packet)) {
    LOG(ERROR) << "Failed to parse AudioPacket.";
    return;
  }

  // Pass a captured audio packet to |audio_capturer_|.
  audio_capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&IpcAudioCapturer::OnAudioPacket, audio_capturer_,
                            base::Passed(&packet)));
}

void DesktopSessionProxy::OnCreateSharedBuffer(
    int id,
    base::SharedMemoryHandle handle,
    uint32_t size) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  scoped_refptr<IpcSharedBufferCore> shared_buffer =
      new IpcSharedBufferCore(id, handle, size);

  if (shared_buffer->memory() != nullptr &&
      !shared_buffers_.insert(std::make_pair(id, shared_buffer)).second) {
    LOG(ERROR) << "Duplicate shared buffer id " << id << " encountered";
  }
}

void DesktopSessionProxy::OnReleaseSharedBuffer(int id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Drop the cached reference to the buffer.
  shared_buffers_.erase(id);
}

void DesktopSessionProxy::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    const SerializedDesktopFrame& serialized_frame) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  --pending_capture_frame_requests_;

  if (!video_capturer_) {
    return;
  }

  if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
    video_capturer_->OnCaptureResult(result, nullptr);
    return;
  }

  // Assume that |serialized_frame| is well-formed because it was received from
  // a more privileged process.
  scoped_refptr<IpcSharedBufferCore> shared_buffer_core =
      GetSharedBufferCore(serialized_frame.shared_buffer_id);
  CHECK(shared_buffer_core.get());

  std::unique_ptr<webrtc::DesktopFrame> frame(
      new webrtc::SharedMemoryDesktopFrame(
          serialized_frame.dimensions, serialized_frame.bytes_per_row,
          new IpcSharedBuffer(shared_buffer_core)));
  frame->set_capture_time_ms(serialized_frame.capture_time_ms);
  frame->set_dpi(serialized_frame.dpi);
  frame->set_capturer_id(serialized_frame.capturer_id);

  for (const auto& rect : serialized_frame.dirty_region) {
    frame->mutable_updated_region()->AddRect(rect);
  }

  video_capturer_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                                   std::move(frame));
}

void DesktopSessionProxy::OnMouseCursor(
    const webrtc::MouseCursor& mouse_cursor) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (mouse_cursor_monitor_) {
    mouse_cursor_monitor_->OnMouseCursor(
        base::WrapUnique(webrtc::MouseCursor::CopyOf(mouse_cursor)));
  }
}

void DesktopSessionProxy::OnInjectClipboardEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (client_clipboard_) {
    protocol::ClipboardEvent event;
    if (!event.ParseFromString(serialized_event)) {
      LOG(ERROR) << "Failed to parse protocol::ClipboardEvent.";
      return;
    }

    client_clipboard_->InjectClipboardEvent(event);
  }
}

void DesktopSessionProxy::SendToDesktop(IPC::Message* message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (desktop_channel_) {
    desktop_channel_->Send(message);
  } else {
    delete message;
  }
}

// static
void DesktopSessionProxyTraits::Destruct(
    const DesktopSessionProxy* desktop_session_proxy) {
  desktop_session_proxy->caller_task_runner_->DeleteSoon(FROM_HERE,
                                                         desktop_session_proxy);
}

}  // namespace remoting
