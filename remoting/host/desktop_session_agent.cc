// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/process_stats_sender.h"
#include "remoting/host/remote_input_filter.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/proto/action.pb.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/input_event_tracker.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

#if defined(OS_WIN)
#include "base/memory/writable_shared_memory_region.h"
#endif

namespace remoting {

// webrtc::SharedMemory implementation that creates a
// base::ReadOnlySharedMemoryRegion along with a writable mapping.
//
// This is declared outside the anonymous namespace so that it can be friended
// with base::WritableSharedMemoryRegion. It is not exported in the header.
class SharedMemoryImpl : public webrtc::SharedMemory {
 public:
  static std::unique_ptr<SharedMemoryImpl>
  Create(size_t size, int id, const base::Closure& on_deleted_callback) {
    webrtc::SharedMemory::Handle handle = webrtc::SharedMemory::kInvalidHandle;
#if defined(OS_WIN)
    // webrtc::ScreenCapturer uses webrtc::SharedMemory::handle() only on
    // windows. This handle must be writable. A WritableSharedMemoryRegion is
    // created, and then it is converted to read-only.  On the windows platform,
    // it happens to be the case that converting a region to read-only does not
    // change the status of existing handles. This is not true on all other
    // platforms, so please don't emulate this behavior!
    base::WritableSharedMemoryRegion region =
        base::WritableSharedMemoryRegion::Create(size);
    base::WritableSharedMemoryMapping mapping = region.Map();
    // Converting |region| to read-only will close its associated handle, so we
    // must duplicate it into the handle used for |webrtc::ScreenCapturer|.
    HANDLE process = ::GetCurrentProcess();
    BOOL success =
        ::DuplicateHandle(process, region.UnsafeGetPlatformHandle(), process,
                          &handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
    if (!success)
      return nullptr;
    base::ReadOnlySharedMemoryRegion read_only_region =
        base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
#else
    base::MappedReadOnlyRegion region_mapping =
        base::ReadOnlySharedMemoryRegion::Create(size);
    base::ReadOnlySharedMemoryRegion read_only_region =
        std::move(region_mapping.region);
    base::WritableSharedMemoryMapping mapping =
        std::move(region_mapping.mapping);
#endif
    if (!mapping.IsValid())
      return nullptr;
    // The SharedMemoryImpl ctor is private, so std::make_unique can't be
    // used.
    return base::WrapUnique(new SharedMemoryImpl(std::move(read_only_region),
                                                 std::move(mapping), handle, id,
                                                 on_deleted_callback));
  }

  ~SharedMemoryImpl() override { on_deleted_callback_.Run(); }

  const base::ReadOnlySharedMemoryRegion& region() const { return region_; }

 private:
  SharedMemoryImpl(base::ReadOnlySharedMemoryRegion region,
                   base::WritableSharedMemoryMapping mapping,
                   webrtc::SharedMemory::Handle handle,
                   int id,
                   const base::Closure& on_deleted_callback)
      : SharedMemory(mapping.memory(), mapping.size(), handle, id),
        on_deleted_callback_(on_deleted_callback)
#if defined(OS_WIN)
        ,
        writable_handle_(handle)
#endif
  {
    region_ = std::move(region);
    mapping_ = std::move(mapping);
  }

  base::Closure on_deleted_callback_;
  base::ReadOnlySharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
#if defined(OS_WIN)
  // Owns the handle passed to the base class which is used by
  // webrtc::ScreenCapturer.
  base::win::ScopedHandle writable_handle_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryImpl);
};

namespace {

// Routes local clipboard events though the IPC channel to the network process.
class DesktopSessionClipboardStub : public protocol::ClipboardStub {
 public:
  explicit DesktopSessionClipboardStub(
      scoped_refptr<DesktopSessionAgent> desktop_session_agent);
  ~DesktopSessionClipboardStub() override;

  // protocol::ClipboardStub implementation.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

 private:
  scoped_refptr<DesktopSessionAgent> desktop_session_agent_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionClipboardStub);
};

DesktopSessionClipboardStub::DesktopSessionClipboardStub(
    scoped_refptr<DesktopSessionAgent> desktop_session_agent)
    : desktop_session_agent_(desktop_session_agent) {}

DesktopSessionClipboardStub::~DesktopSessionClipboardStub() = default;

void DesktopSessionClipboardStub::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  desktop_session_agent_->InjectClipboardEvent(event);
}

class SharedMemoryFactoryImpl : public webrtc::SharedMemoryFactory {
 public:
  typedef base::Callback<void(std::unique_ptr<IPC::Message> message)>
      SendMessageCallback;

  SharedMemoryFactoryImpl(const SendMessageCallback& send_message_callback)
      : send_message_callback_(send_message_callback) {}

  std::unique_ptr<webrtc::SharedMemory> CreateSharedMemory(
      size_t size) override {
    base::Closure release_buffer_callback = base::Bind(
        send_message_callback_,
        base::Passed(
            std::make_unique<ChromotingDesktopNetworkMsg_ReleaseSharedBuffer>(
                next_shared_buffer_id_)));
    std::unique_ptr<SharedMemoryImpl> buffer = SharedMemoryImpl::Create(
        size, next_shared_buffer_id_, release_buffer_callback);
    if (buffer) {
      // |next_shared_buffer_id_| starts from 1 and incrementing it by 2 makes
      // sure it is always odd and therefore zero is never used as a valid
      // buffer ID.
      //
      // It is very unlikely (though theoretically possible) to allocate the
      // same ID for two different buffers due to integer overflow. It should
      // take about a year of allocating 100 new buffers every second.
      // Practically speaking it never happens.
      next_shared_buffer_id_ += 2;

      send_message_callback_.Run(
          std::make_unique<ChromotingDesktopNetworkMsg_CreateSharedBuffer>(
              buffer->id(), buffer->region().Duplicate(), buffer->size()));
    }

    return std::move(buffer);
  }

 private:
  int next_shared_buffer_id_ = 1;
  SendMessageCallback send_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryFactoryImpl);
};

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
      io_task_runner_(io_task_runner),
      current_process_stats_("DesktopSessionAgent") {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

bool DesktopSessionAgent::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  if (started_) {
    IPC_BEGIN_MESSAGE_MAP(DesktopSessionAgent, message)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_CaptureFrame,
                          OnCaptureFrame)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_SelectSource,
                          OnSelectSource)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectClipboardEvent,
                          OnInjectClipboardEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectKeyEvent,
                          OnInjectKeyEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectTextEvent,
                          OnInjectTextEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectMouseEvent,
                          OnInjectMouseEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectTouchEvent,
                          OnInjectTouchEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_ExecuteActionRequest,
                          OnExecuteActionRequestEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_SetScreenResolution,
                          SetScreenResolution)
      IPC_MESSAGE_FORWARD(ChromotingNetworkDesktopMsg_ReadFile,
                          &*session_file_operations_handler_,
                          SessionFileOperationsHandler::ReadFile)
      IPC_MESSAGE_FORWARD(ChromotingNetworkDesktopMsg_ReadFileChunk,
                          &*session_file_operations_handler_,
                          SessionFileOperationsHandler::ReadChunk)
      IPC_MESSAGE_FORWARD(ChromotingNetworkDesktopMsg_WriteFile,
                          &*session_file_operations_handler_,
                          SessionFileOperationsHandler::WriteFile)
      IPC_MESSAGE_FORWARD(ChromotingNetworkDesktopMsg_WriteFileChunk,
                          &*session_file_operations_handler_,
                          SessionFileOperationsHandler::WriteChunk)
      IPC_MESSAGE_FORWARD(ChromotingNetworkDesktopMsg_CloseFile,
                          &*session_file_operations_handler_,
                          SessionFileOperationsHandler::Close)
      IPC_MESSAGE_FORWARD(ChromotingNetworkDesktopMsg_CancelFile,
                          &*session_file_operations_handler_,
                          SessionFileOperationsHandler::Cancel)
      IPC_MESSAGE_HANDLER(ChromotingNetworkToAnyMsg_StartProcessStatsReport,
                          StartProcessStatsReport)
      IPC_MESSAGE_HANDLER(ChromotingNetworkToAnyMsg_StopProcessStatsReport,
                          StopProcessStatsReport)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  } else {
    IPC_BEGIN_MESSAGE_MAP(DesktopSessionAgent, message)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_StartSessionAgent,
                          OnStartSessionAgent)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  }

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
  return handled;
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
  if (delegate_.get())
    delegate_->OnNetworkProcessDisconnected();
}

DesktopSessionAgent::~DesktopSessionAgent() {
  DCHECK(!audio_capturer_);
  DCHECK(!desktop_environment_);
  DCHECK(!network_channel_);
  DCHECK(!screen_controls_);
  DCHECK(!video_capturer_);
  DCHECK(!stats_sender_);
  DCHECK(!session_file_operations_handler_);
}

const std::string& DesktopSessionAgent::client_jid() const {
  return client_jid_;
}

void DesktopSessionAgent::DisconnectSession(protocol::ErrorCode error) {
  SendToNetwork(
      std::make_unique<ChromotingDesktopNetworkMsg_DisconnectSession>(error));
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
  SendToNetwork(std::make_unique<ChromotingDesktopNetworkMsg_DisplayChanged>(
      *layout.get()));
}

void DesktopSessionAgent::OnProcessStats(
    const protocol::AggregatedProcessResourceUsage& usage) {
  SendToNetwork(
      std::make_unique<ChromotingAnyToNetworkMsg_ReportProcessStats>(usage));
}

void DesktopSessionAgent::OnStartSessionAgent(
    const std::string& authenticated_jid,
    const ScreenResolution& resolution,
    const remoting::DesktopEnvironmentOptions& options) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!started_);
  DCHECK(!audio_capturer_);
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(!video_capturer_);
  DCHECK(!session_file_operations_handler_);

  started_ = true;
  client_jid_ = authenticated_jid;

  // Create a desktop environment for the new session.
  desktop_environment_ = delegate_->desktop_environment_factory().Create(
      weak_factory_.GetWeakPtr(), options);

  // Create the session controller and set the initial screen resolution.
  screen_controls_ = desktop_environment_->CreateScreenControls();
  SetScreenResolution(resolution);

  // Create the input injector.
  input_injector_ = desktop_environment_->CreateInputInjector();

  action_executor_ = desktop_environment_->CreateActionExecutor();

  // Hook up the input filter.
  input_tracker_.reset(new protocol::InputEventTracker(input_injector_.get()));
  remote_input_filter_.reset(new RemoteInputFilter(input_tracker_.get()));

#if defined(OS_WIN)
  // LocalInputMonitorWin filters out an echo of the injected input before it
  // reaches |remote_input_filter_|.
  remote_input_filter_->SetExpectLocalEcho(false);
#endif  // defined(OS_WIN)

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

  // Start the video capturer and mouse cursor monitor.
  video_capturer_ = desktop_environment_->CreateVideoCapturer();
  video_capturer_->Start(this);
  video_capturer_->SetSharedMemoryFactory(
      std::unique_ptr<webrtc::SharedMemoryFactory>(new SharedMemoryFactoryImpl(
          base::Bind(&DesktopSessionAgent::SendToNetwork, this))));
  mouse_cursor_monitor_ = desktop_environment_->CreateMouseCursorMonitor();
  mouse_cursor_monitor_->Init(this, webrtc::MouseCursorMonitor::SHAPE_ONLY);

  // Set up the message handler for file transfers.
  session_file_operations_handler_.emplace(
      this, desktop_environment_->CreateFileOperations());
}

void DesktopSessionAgent::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Serialize webrtc::DesktopFrame.
  SerializedDesktopFrame serialized_frame;
  if (frame) {
    serialized_frame.shared_buffer_id = frame->shared_memory()->id();
    serialized_frame.bytes_per_row = frame->stride();
    serialized_frame.dimensions = frame->size();
    serialized_frame.capture_time_ms = frame->capture_time_ms();
    serialized_frame.dpi = frame->dpi();
    serialized_frame.capturer_id = frame->capturer_id();
    for (webrtc::DesktopRegion::Iterator i(frame->updated_region());
         !i.IsAtEnd(); i.Advance()) {
      serialized_frame.dirty_region.push_back(i.rect());
    }
  }

  last_frame_ = std::move(frame);

  SendToNetwork(std::make_unique<ChromotingDesktopNetworkMsg_CaptureResult>(
      result, serialized_frame));
}

void DesktopSessionAgent::OnMouseCursor(webrtc::MouseCursor* cursor) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<webrtc::MouseCursor> owned_cursor(cursor);

  SendToNetwork(
      std::make_unique<ChromotingDesktopNetworkMsg_MouseCursor>(*owned_cursor));
}

void DesktopSessionAgent::OnMouseCursorPosition(
    webrtc::MouseCursorMonitor::CursorState state,
    const webrtc::DesktopVector& position) {
  // We're not subscribing to mouse position changes.
  NOTREACHED();
}

void DesktopSessionAgent::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::ClipboardEvent.";
    return;
  }

  SendToNetwork(
      std::make_unique<ChromotingDesktopNetworkMsg_InjectClipboardEvent>(
          serialized_event));
}

void DesktopSessionAgent::ProcessAudioPacket(
    std::unique_ptr<AudioPacket> packet) {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  std::string serialized_packet;
  if (!packet->SerializeToString(&serialized_packet)) {
    LOG(ERROR) << "Failed to serialize AudioPacket.";
    return;
  }

  SendToNetwork(std::make_unique<ChromotingDesktopNetworkMsg_AudioPacket>(
      serialized_packet));
}

void DesktopSessionAgent::OnResult(uint64_t file_id,
                                   ResultHandler::Result result) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  SendToNetwork(std::make_unique<ChromotingDesktopNetworkMsg_FileResult>(
      file_id, std::move(result)));
}

void DesktopSessionAgent::OnInfoResult(std::uint64_t file_id,
                                       ResultHandler::InfoResult result) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  SendToNetwork(std::make_unique<ChromotingDesktopNetworkMsg_FileInfoResult>(
      file_id, std::move(result)));
}

void DesktopSessionAgent::OnDataResult(std::uint64_t file_id,
                                       ResultHandler::DataResult result) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  SendToNetwork(std::make_unique<ChromotingDesktopNetworkMsg_FileDataResult>(
      file_id, std::move(result)));
}

mojo::ScopedMessagePipeHandle DesktopSessionAgent::Start(
    const base::WeakPtr<Delegate>& delegate) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate);
  DCHECK(!delegate_);

  delegate_ = delegate;

  mojo::MessagePipe pipe;
  network_channel_ = IPC::ChannelProxy::Create(
      pipe.handle0.release(), IPC::Channel::MODE_SERVER, this, io_task_runner_,
      base::ThreadTaskRunnerHandle::Get());
  return std::move(pipe.handle1);
}

void DesktopSessionAgent::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  delegate_.reset();

  stats_sender_.reset();

  // Make sure the channel is closed.
  network_channel_.reset();

  if (started_) {
    started_ = false;

    // Ignore any further callbacks.
    weak_factory_.InvalidateWeakPtrs();
    client_jid_.clear();

    remote_input_filter_.reset();

    session_file_operations_handler_.reset();

    // Ensure that any pressed keys or buttons are released.
    input_tracker_->ReleaseAll();
    input_tracker_.reset();

    desktop_environment_.reset();
    action_executor_.reset();
    input_injector_.reset();
    screen_controls_.reset();

    // Stop the audio capturer.
    audio_capture_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DesktopSessionAgent::StopAudioCapturer, this));

    // Stop the video capturer.
    video_capturer_.reset();
    last_frame_.reset();
    mouse_cursor_monitor_.reset();
  }
}

void DesktopSessionAgent::OnCaptureFrame() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  mouse_cursor_monitor_->Capture();

  // webrtc::DesktopCapturer supports a very few (currently 2) outstanding
  // capture requests. The requests are serialized on
  // |video_capture_task_runner()| task runner. If the client issues more
  // requests, pixel data in captured frames will likely be corrupted but
  // stability of webrtc::DesktopCapturer will not be affected.
  video_capturer_->CaptureFrame();
}

void DesktopSessionAgent::OnSelectSource(int id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  video_capturer_->SelectSource(id);
}

void DesktopSessionAgent::OnInjectClipboardEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::ClipboardEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::ClipboardEvent.";
    return;
  }

  // InputStub implementations must verify events themselves, so we don't need
  // verification here. This matches HostEventDispatcher.
  input_injector_->InjectClipboardEvent(event);
}

void DesktopSessionAgent::OnInjectKeyEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::KeyEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::KeyEvent.";
    return;
  }

  // InputStub implementations must verify events themselves, so we need only
  // basic verification here. This matches HostEventDispatcher.
  if (!event.has_usb_keycode() || !event.has_pressed()) {
    LOG(ERROR) << "Received invalid key event.";
    return;
  }

  remote_input_filter_->InjectKeyEvent(event);
}

void DesktopSessionAgent::OnInjectTextEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::TextEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::TextEvent.";
    return;
  }

  // InputStub implementations must verify events themselves, so we need only
  // basic verification here. This matches HostEventDispatcher.
  if (!event.has_text()) {
    LOG(ERROR) << "Received invalid TextEvent.";
    return;
  }

  remote_input_filter_->InjectTextEvent(event);
}

void DesktopSessionAgent::OnInjectMouseEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::MouseEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::MouseEvent.";
    return;
  }

  // InputStub implementations must verify events themselves, so we don't need
  // verification here. This matches HostEventDispatcher.
  remote_input_filter_->InjectMouseEvent(event);
}

void DesktopSessionAgent::OnInjectTouchEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::TouchEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::TouchEvent.";
    return;
  }

  remote_input_filter_->InjectTouchEvent(event);
}

void DesktopSessionAgent::OnExecuteActionRequestEvent(
    const protocol::ActionRequest& request) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  action_executor_->ExecuteAction(request);
}

void DesktopSessionAgent::SetScreenResolution(
    const ScreenResolution& resolution) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (screen_controls_ && resolution.IsEmpty())
    screen_controls_->SetScreenResolution(resolution);
}

void DesktopSessionAgent::SendToNetwork(std::unique_ptr<IPC::Message> message) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DesktopSessionAgent::SendToNetwork, this,
                                  std::move(message)));
    return;
  }

  if (network_channel_) {
    network_channel_->Send(message.release());
  }
}

void DesktopSessionAgent::StartAudioCapturer() {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  if (audio_capturer_) {
    audio_capturer_->Start(base::Bind(&DesktopSessionAgent::ProcessAudioPacket,
                                      this));
  }
}

void DesktopSessionAgent::StopAudioCapturer() {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  audio_capturer_.reset();
}

void DesktopSessionAgent::StartProcessStatsReport(base::TimeDelta interval) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!stats_sender_);

  if (interval <= base::TimeDelta::FromSeconds(0)) {
    interval = kDefaultProcessStatsInterval;
  }

  stats_sender_.reset(new ProcessStatsSender(
      this,
      interval,
      { &current_process_stats_ }));
}

void DesktopSessionAgent::StopProcessStatsReport() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(stats_sender_);
  stats_sender_.reset();
}

}  // namespace remoting
