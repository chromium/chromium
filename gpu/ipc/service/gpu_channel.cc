// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel.h"

#include <utility>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <algorithm>
#include <set>
#include <vector>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/abstract_texture_impl_shared_context_state.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gles2_command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/image_decode_accelerator_stub.h"
#include "gpu/ipc/service/raster_command_buffer_stub.h"
#include "gpu/ipc/service/webgpu_command_buffer_stub.h"
#include "ipc/ipc_channel.h"
#include "ipc/message_filter.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"

#if defined(OS_ANDROID)
#include "gpu/ipc/service/stream_texture_android.h"
#endif  // defined(OS_ANDROID)

namespace gpu {

struct GpuChannelMessage {
  IPC::Message message;
  uint32_t order_number;
  base::TimeTicks time_received;

  GpuChannelMessage(const IPC::Message& msg,
                    uint32_t order_num,
                    base::TimeTicks ts)
      : message(msg), order_number(order_num), time_received(ts) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuChannelMessage);
};

// This filter does the following:
// - handles the Nop message used for verifying sync tokens on the IO thread
// - forwards messages to child message filters
// - posts control and out of order messages to the main thread
// - forwards other messages to the scheduler
class GPU_IPC_SERVICE_EXPORT GpuChannelMessageFilter
    : public IPC::MessageFilter {
 public:
  GpuChannelMessageFilter(
      GpuChannel* gpu_channel,
      Scheduler* scheduler,
      ImageDecodeAcceleratorWorker* image_decode_accelerator_worker,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  // Methods called on main thread.
  void Destroy();

  // Called when scheduler is enabled.
  void AddRoute(int32_t route_id, SequenceId sequence_id);
  void RemoveRoute(int32_t route_id);

  // Methods called on IO thread.
  // IPC::MessageFilter implementation.
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void AddChannelFilter(scoped_refptr<IPC::MessageFilter> filter);
  void RemoveChannelFilter(scoped_refptr<IPC::MessageFilter> filter);

  ImageDecodeAcceleratorStub* image_decode_accelerator_stub() const {
    return image_decode_accelerator_stub_.get();
  }

 private:
  ~GpuChannelMessageFilter() override;

  SequenceId GetSequenceId(int32_t route_id) const;

  bool HandleFlushMessage(const IPC::Message& message);

  bool MessageErrorHandler(const IPC::Message& message, const char* error_msg);

  IPC::Channel* ipc_channel_ = nullptr;
  base::ProcessId peer_pid_ = base::kNullProcessId;
  std::vector<scoped_refptr<IPC::MessageFilter>> channel_filters_;

  GpuChannel* gpu_channel_ = nullptr;
  // Map of route id to scheduler sequence id.
  base::flat_map<int32_t, SequenceId> route_sequences_;
  mutable base::Lock gpu_channel_lock_;

  Scheduler* scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  scoped_refptr<ImageDecodeAcceleratorStub> image_decode_accelerator_stub_;
  base::ThreadChecker io_thread_checker_;

  bool allow_crash_for_testing_ = false;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelMessageFilter);
};

GpuChannelMessageFilter::GpuChannelMessageFilter(
    GpuChannel* gpu_channel,
    Scheduler* scheduler,
    ImageDecodeAcceleratorWorker* image_decode_accelerator_worker,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : gpu_channel_(gpu_channel),
      scheduler_(scheduler),
      main_task_runner_(std::move(main_task_runner)),
      image_decode_accelerator_stub_(
          base::MakeRefCounted<ImageDecodeAcceleratorStub>(
              image_decode_accelerator_worker,
              gpu_channel,
              static_cast<int32_t>(
                  GpuChannelReservedRoutes::kImageDecodeAccelerator))) {
  io_thread_checker_.DetachFromThread();
  allow_crash_for_testing_ = gpu_channel->gpu_channel_manager()
                                 ->gpu_preferences()
                                 .enable_gpu_benchmarking_extension;
}

GpuChannelMessageFilter::~GpuChannelMessageFilter() {
  DCHECK(!gpu_channel_);
}

void GpuChannelMessageFilter::Destroy() {
  base::AutoLock auto_lock(gpu_channel_lock_);
  image_decode_accelerator_stub_->Shutdown();
  gpu_channel_ = nullptr;
}

void GpuChannelMessageFilter::AddRoute(int32_t route_id,
                                       SequenceId sequence_id) {
  base::AutoLock auto_lock(gpu_channel_lock_);
  DCHECK(gpu_channel_);
  DCHECK(scheduler_);
  route_sequences_[route_id] = sequence_id;
}

void GpuChannelMessageFilter::RemoveRoute(int32_t route_id) {
  base::AutoLock auto_lock(gpu_channel_lock_);
  DCHECK(gpu_channel_);
  DCHECK(scheduler_);
  route_sequences_.erase(route_id);
}

void GpuChannelMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(!ipc_channel_);
  ipc_channel_ = channel;
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_)
    filter->OnFilterAdded(ipc_channel_);
}

void GpuChannelMessageFilter::OnFilterRemoved() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_)
    filter->OnFilterRemoved();
  ipc_channel_ = nullptr;
  peer_pid_ = base::kNullProcessId;
}

void GpuChannelMessageFilter::OnChannelConnected(int32_t peer_pid) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(peer_pid_ == base::kNullProcessId);
  peer_pid_ = peer_pid;
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_)
    filter->OnChannelConnected(peer_pid);
}

void GpuChannelMessageFilter::OnChannelError() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_)
    filter->OnChannelError();
}

void GpuChannelMessageFilter::OnChannelClosing() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_)
    filter->OnChannelClosing();
}

void GpuChannelMessageFilter::AddChannelFilter(
    scoped_refptr<IPC::MessageFilter> filter) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  channel_filters_.push_back(filter);
  if (ipc_channel_)
    filter->OnFilterAdded(ipc_channel_);
  if (peer_pid_ != base::kNullProcessId)
    filter->OnChannelConnected(peer_pid_);
}

void GpuChannelMessageFilter::RemoveChannelFilter(
    scoped_refptr<IPC::MessageFilter> filter) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (ipc_channel_)
    filter->OnFilterRemoved();
  base::Erase(channel_filters_, filter);
}

bool GpuChannelMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(ipc_channel_);

  if (message.should_unblock() || message.is_reply())
    return MessageErrorHandler(message, "Unexpected message type");

  switch (message.type()) {
    case GpuCommandBufferMsg_AsyncFlush::ID:
    case GpuCommandBufferMsg_DestroyTransferBuffer::ID:
    case GpuCommandBufferMsg_ReturnFrontBuffer::ID:
    case GpuCommandBufferMsg_TakeFrontBuffer::ID:
    case GpuChannelMsg_CreateSharedImage::ID:
    case GpuChannelMsg_DestroySharedImage::ID:
      return MessageErrorHandler(message, "Invalid message");
    case GpuChannelMsg_CrashForTesting::ID:
      // Handle this message early, on the IO thread, in case the main
      // thread is hung. This is the purpose of this message: generating
      // minidumps on the bots, which are symbolized later by the test
      // harness. Only pay attention to this message if Telemetry's GPU
      // benchmarking extension was enabled via the command line, which
      // exposes privileged APIs to JavaScript.
      if (allow_crash_for_testing_) {
        gl::Crash();
      }
      // Won't be reached if the extension is enabled.
      return MessageErrorHandler(message, "Crashes for testing are disabled");
    default:
      break;
  }

  if (message.type() == GpuChannelMsg_Nop::ID) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    ipc_channel_->Send(reply);
    return true;
  }

  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    if (filter->OnMessageReceived(message))
      return true;
  }

  base::AutoLock auto_lock(gpu_channel_lock_);
  if (!gpu_channel_)
    return MessageErrorHandler(message, "Channel destroyed");

  // Handle flush first so that it doesn't get handled out of order.
  if (message.type() == GpuChannelMsg_FlushDeferredMessages::ID)
    return HandleFlushMessage(message);

  if (message.routing_id() ==
      static_cast<int32_t>(GpuChannelReservedRoutes::kImageDecodeAccelerator)) {
    if (!image_decode_accelerator_stub_->OnMessageReceived(message))
      return MessageErrorHandler(message, "Invalid image decode request");
    return true;
  }

  bool handle_out_of_order =
      message.routing_id() == MSG_ROUTING_CONTROL ||
      message.type() == GpuCommandBufferMsg_WaitForTokenInRange::ID ||
      message.type() == GpuCommandBufferMsg_WaitForGetOffsetInRange::ID;

  if (handle_out_of_order) {
    // It's OK to post task that may never run even for sync messages, because
    // if the channel is destroyed, the client Send will fail.
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuChannel::HandleOutOfOrderMessage,
                                  gpu_channel_->AsWeakPtr(), message));
    return true;
  }

  // Messages which do not have sync token dependencies.
  SequenceId sequence_id = GetSequenceId(message.routing_id());
  if (sequence_id.is_null())
    return MessageErrorHandler(message, "Invalid route id");

  scheduler_->ScheduleTask(
      Scheduler::Task(sequence_id,
                      base::BindOnce(&GpuChannel::HandleMessage,
                                     gpu_channel_->AsWeakPtr(), message),
                      std::vector<SyncToken>()));
  return true;
}

SequenceId GpuChannelMessageFilter::GetSequenceId(int32_t route_id) const {
  gpu_channel_lock_.AssertAcquired();
  auto it = route_sequences_.find(route_id);
  if (it == route_sequences_.end())
    return SequenceId();
  return it->second;
}

bool GpuChannelMessageFilter::HandleFlushMessage(const IPC::Message& message) {
  DCHECK_EQ(message.type(), GpuChannelMsg_FlushDeferredMessages::ID);
  gpu_channel_lock_.AssertAcquired();

  GpuChannelMsg_FlushDeferredMessages::Param params;
  if (!GpuChannelMsg_FlushDeferredMessages::Read(&message, &params))
    return MessageErrorHandler(message, "Invalid flush message");

  std::vector<GpuDeferredMessage> deferred_messages =
      std::get<0>(std::move(params));

  std::vector<Scheduler::Task> tasks;
  tasks.reserve(deferred_messages.size());
  for (auto& deferred_message : deferred_messages) {
    auto it = route_sequences_.find(deferred_message.message.routing_id());
    if (it == route_sequences_.end()) {
      DLOG(ERROR) << "Invalid route id in flush list";
      continue;
    }
    tasks.emplace_back(
        it->second /* sequence_id */,
        base::BindOnce(&GpuChannel::HandleMessage, gpu_channel_->AsWeakPtr(),
                       std::move(deferred_message.message)),
        std::move(deferred_message.sync_token_fences));
  }
  scheduler_->ScheduleTasks(std::move(tasks));
  return true;
}

bool GpuChannelMessageFilter::MessageErrorHandler(const IPC::Message& message,
                                                  const char* error_msg) {
  DLOG(ERROR) << error_msg;
  if (message.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    reply->set_reply_error();
    ipc_channel_->Send(reply);
  }
  return true;
}

GpuChannel::GpuChannel(
    GpuChannelManager* gpu_channel_manager,
    Scheduler* scheduler,
    SyncPointManager* sync_point_manager,
    scoped_refptr<gl::GLShareGroup> share_group,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    int32_t client_id,
    uint64_t client_tracing_id,
    bool is_gpu_host,
    ImageDecodeAcceleratorWorker* image_decode_accelerator_worker)
    : gpu_channel_manager_(gpu_channel_manager),
      scheduler_(scheduler),
      sync_point_manager_(sync_point_manager),
      client_id_(client_id),
      client_tracing_id_(client_tracing_id),
      task_runner_(task_runner),
      io_task_runner_(io_task_runner),
      share_group_(share_group),
      image_manager_(new gles2::ImageManager()),
      is_gpu_host_(is_gpu_host) {
  DCHECK(gpu_channel_manager_);
  DCHECK(client_id_);
  filter_ = new GpuChannelMessageFilter(
      this, scheduler, image_decode_accelerator_worker, task_runner);
}

GpuChannel::~GpuChannel() {
  // Clear stubs first because of dependencies.
  stubs_.clear();

#if defined(OS_ANDROID)
  // Release any references to this channel held by StreamTexture.
  for (auto& stream_texture : stream_textures_) {
    stream_texture.second->ReleaseChannel();
  }
  stream_textures_.clear();
#endif  // OS_ANDROID

  // Destroy filter first to stop posting tasks to scheduler.
  filter_->Destroy();

  for (const auto& kv : stream_sequences_)
    scheduler_->DestroySequence(kv.second);
}

std::unique_ptr<GpuChannel> GpuChannel::Create(
    GpuChannelManager* gpu_channel_manager,
    Scheduler* scheduler,
    SyncPointManager* sync_point_manager,
    scoped_refptr<gl::GLShareGroup> share_group,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    int32_t client_id,
    uint64_t client_tracing_id,
    bool is_gpu_host,
    ImageDecodeAcceleratorWorker* image_decode_accelerator_worker) {
  auto gpu_channel = base::WrapUnique(
      new GpuChannel(gpu_channel_manager, scheduler, sync_point_manager,
                     std::move(share_group), std::move(task_runner),
                     std::move(io_task_runner), client_id, client_tracing_id,
                     is_gpu_host, image_decode_accelerator_worker));

  if (!gpu_channel->CreateSharedImageStub()) {
    LOG(ERROR) << "GpuChannel: Failed to create SharedImageStub";
    return nullptr;
  }
  return gpu_channel;
}

void GpuChannel::Init(IPC::ChannelHandle channel_handle,
                      base::WaitableEvent* shutdown_event) {
  sync_channel_ = IPC::SyncChannel::Create(
      channel_handle, IPC::Channel::MODE_SERVER, this, io_task_runner_.get(),
      task_runner_.get(), false, shutdown_event);
  sync_channel_->AddFilter(filter_.get());
  channel_ = sync_channel_.get();
}

void GpuChannel::InitForTesting(IPC::Channel* channel) {
  channel_ = channel;
  // |channel| is an IPC::TestSink in tests, so don't add the filter to it
  // because it will forward sent messages back to the filter.
  // Call OnFilterAdded() to prevent DCHECK failures.
  filter_->OnFilterAdded(channel);
}

void GpuChannel::SetUnhandledMessageListener(IPC::Listener* listener) {
  unhandled_message_listener_ = listener;
}

base::WeakPtr<GpuChannel> GpuChannel::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::ProcessId GpuChannel::GetClientPID() const {
  DCHECK(IsConnected());
  return peer_pid_;
}

bool GpuChannel::IsConnected() const {
  return peer_pid_ != base::kNullProcessId;
}

bool GpuChannel::OnMessageReceived(const IPC::Message& msg) {
  // All messages should be pushed to channel_messages_ and handled separately.
  NOTREACHED();
  return false;
}

void GpuChannel::OnChannelConnected(int32_t peer_pid) {
  peer_pid_ = peer_pid;
}

void GpuChannel::OnChannelError() {
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::Send(IPC::Message* message) {
  // The GPU process must never send a synchronous IPC message to the renderer
  // process. This could result in deadlock.
  DCHECK(!message->is_sync());

  DVLOG(1) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type();

  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void GpuChannel::OnCommandBufferScheduled(CommandBufferStub* stub) {
  scheduler_->EnableSequence(stub->sequence_id());
}

void GpuChannel::OnCommandBufferDescheduled(CommandBufferStub* stub) {
  scheduler_->DisableSequence(stub->sequence_id());
}

CommandBufferStub* GpuChannel::LookupCommandBuffer(int32_t route_id) {
  auto it = stubs_.find(route_id);
  if (it == stubs_.end())
    return nullptr;

  return it->second.get();
}

bool GpuChannel::HasActiveWebGLContext() const {
  for (auto& kv : stubs_) {
    ContextType context_type = kv.second->context_type();
    if (context_type == CONTEXT_TYPE_WEBGL1 ||
        context_type == CONTEXT_TYPE_WEBGL2) {
      return true;
    }
  }
  return false;
}

void GpuChannel::MarkAllContextsLost() {
  for (auto& kv : stubs_)
    kv.second->MarkContextLost();
}

bool GpuChannel::AddRoute(int32_t route_id,
                          SequenceId sequence_id,
                          IPC::Listener* listener) {
  if (scheduler_)
    filter_->AddRoute(route_id, sequence_id);
  return router_.AddRoute(route_id, listener);
}

void GpuChannel::RemoveRoute(int32_t route_id) {
  if (scheduler_)
    filter_->RemoveRoute(route_id);
  router_.RemoveRoute(route_id);
}

bool GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateCommandBuffer,
                        OnCreateCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyCommandBuffer,
                        OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateStreamTexture,
                        OnCreateStreamTexture)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuChannel::HandleMessage(const IPC::Message& msg) {
  int32_t routing_id = msg.routing_id();
  CommandBufferStub* stub = LookupCommandBuffer(routing_id);

  DCHECK(!stub || stub->IsScheduled());

  DVLOG(1) << "received message @" << &msg << " on channel @" << this
           << " with type " << msg.type();

  HandleMessageHelper(msg);

  // If we get descheduled or yield while processing a message.
  if (stub && (stub->HasUnprocessedCommands() || !stub->IsScheduled())) {
    DCHECK_EQ(GpuCommandBufferMsg_AsyncFlush::ID, msg.type());
    scheduler_->ContinueTask(
        stub->sequence_id(),
        base::BindOnce(&GpuChannel::HandleMessage, AsWeakPtr(), msg));
  }
}

void GpuChannel::HandleMessageForTesting(const IPC::Message& msg) {
  // Message filter gets message first on IO thread.
  filter_->OnMessageReceived(msg);
}

ImageDecodeAcceleratorStub* GpuChannel::GetImageDecodeAcceleratorStub() const {
  DCHECK(filter_);
  return filter_->image_decode_accelerator_stub();
}

bool GpuChannel::CreateSharedImageStub() {
  // SharedImageInterfaceProxy/Stub is a singleton per channel, using a reserved
  // route.
  const int32_t shared_image_route_id =
      static_cast<int32_t>(GpuChannelReservedRoutes::kSharedImageInterface);
  shared_image_stub_ = SharedImageStub::Create(this, shared_image_route_id);
  if (!shared_image_stub_)
    return false;

  filter_->AddRoute(shared_image_route_id, shared_image_stub_->sequence());
  router_.AddRoute(shared_image_route_id, shared_image_stub_.get());
  return true;
}

void GpuChannel::HandleMessageHelper(const IPC::Message& msg) {
  int32_t routing_id = msg.routing_id();

  bool handled = false;
  if (routing_id == MSG_ROUTING_CONTROL) {
    handled = OnControlMessageReceived(msg);
  } else {
    handled = router_.RouteMessage(msg);
  }

  if (!handled && unhandled_message_listener_)
    handled = unhandled_message_listener_->OnMessageReceived(msg);

  // Respond to sync messages even if router failed to route.
  if (!handled && msg.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
    reply->set_reply_error();
    Send(reply);
  }
}

void GpuChannel::HandleOutOfOrderMessage(const IPC::Message& msg) {
  HandleMessageHelper(msg);
}

#if defined(OS_ANDROID)
const CommandBufferStub* GpuChannel::GetOneStub() const {
  for (const auto& kv : stubs_) {
    const CommandBufferStub* stub = kv.second.get();
    if (stub->decoder_context() && !stub->decoder_context()->WasContextLost())
      return stub;
  }
  return nullptr;
}

void GpuChannel::DestroyStreamTexture(int32_t stream_id) {
  auto found = stream_textures_.find(stream_id);
  if (found == stream_textures_.end()) {
    LOG(ERROR) << "Trying to destroy a non-existent stream texture.";
    return;
  }
  found->second->ReleaseChannel();
  stream_textures_.erase(stream_id);
}
#endif

void GpuChannel::OnCreateCommandBuffer(
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id,
    base::UnsafeSharedMemoryRegion shared_state_shm,
    ContextResult* result,
    gpu::Capabilities* capabilities) {
  TRACE_EVENT2("gpu", "GpuChannel::OnCreateCommandBuffer", "route_id", route_id,
               "offscreen", (init_params.surface_handle == kNullSurfaceHandle));
  // Default result on failure. Override with a more accurate failure if needed,
  // or with success.
  *result = ContextResult::kFatalFailure;
  *capabilities = gpu::Capabilities();

  if (init_params.surface_handle != kNullSurfaceHandle && !is_gpu_host_) {
    LOG(ERROR)
        << "ContextResult::kFatalFailure: "
           "attempt to create a view context on a non-privileged channel";
    return;
  }

  if (gpu_channel_manager_->delegate()->IsExiting()) {
    LOG(ERROR) << "ContextResult::kTransientFailure: trying to create command "
                  "buffer during process shutdown.";
    *result = gpu::ContextResult::kTransientFailure;
    return;
  }

  int32_t stream_id = init_params.stream_id;
  int32_t share_group_id = init_params.share_group_id;
  CommandBufferStub* share_group = LookupCommandBuffer(share_group_id);

  if (!share_group && share_group_id != MSG_ROUTING_NONE) {
    LOG(ERROR) << "ContextResult::kFatalFailure: invalid share group id";
    return;
  }

  if (share_group && stream_id != share_group->stream_id()) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "stream id does not match share group stream id";
    return;
  }

  if (share_group && !share_group->decoder_context()) {
    // This should catch test errors where we did not Initialize the
    // share_group's CommandBuffer.
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "shared context was not initialized";
    return;
  }

  if (share_group && share_group->decoder_context()->WasContextLost()) {
    // The caller should retry to get a context.
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "shared context was already lost";
    *result = gpu::ContextResult::kTransientFailure;
    return;
  }

  CommandBufferId command_buffer_id =
      CommandBufferIdFromChannelAndRoute(client_id_, route_id);

  SequenceId sequence_id = stream_sequences_[stream_id];
  if (sequence_id.is_null()) {
    sequence_id = scheduler_->CreateSequence(init_params.stream_priority);
    stream_sequences_[stream_id] = sequence_id;
  }

  std::unique_ptr<CommandBufferStub> stub;
  if (init_params.attribs.context_type == CONTEXT_TYPE_WEBGPU) {
    if (!gpu_channel_manager_->gpu_preferences().enable_webgpu) {
      DLOG(ERROR) << "ContextResult::kFatalFailure: WebGPU not enabled";
      return;
    }

    stub = std::make_unique<WebGPUCommandBufferStub>(
        this, init_params, command_buffer_id, sequence_id, stream_id, route_id);
  } else if (init_params.attribs.enable_raster_interface &&
             !init_params.attribs.enable_gles2_interface) {
    stub = std::make_unique<RasterCommandBufferStub>(
        this, init_params, command_buffer_id, sequence_id, stream_id, route_id);
  } else {
    stub = std::make_unique<GLES2CommandBufferStub>(
        this, init_params, command_buffer_id, sequence_id, stream_id, route_id);
  }

  auto stub_result =
      stub->Initialize(share_group, init_params, std::move(shared_state_shm));
  if (stub_result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "GpuChannel::CreateCommandBuffer(): failed to initialize "
                   "CommandBufferStub";
    *result = stub_result;
    return;
  }

  if (!AddRoute(route_id, sequence_id, stub.get())) {
    LOG(ERROR) << "ContextResult::kFatalFailure: failed to add route";
    return;
  }

  *result = ContextResult::kSuccess;
  *capabilities = stub->decoder_context()->GetCapabilities();
  stubs_[route_id] = std::move(stub);
}

void GpuChannel::OnDestroyCommandBuffer(int32_t route_id) {
  TRACE_EVENT1("gpu", "GpuChannel::OnDestroyCommandBuffer", "route_id",
               route_id);

  std::unique_ptr<CommandBufferStub> stub;
  auto it = stubs_.find(route_id);
  if (it != stubs_.end()) {
    stub = std::move(it->second);
    stubs_.erase(it);
  }
  // In case the renderer is currently blocked waiting for a sync reply from the
  // stub, we need to make sure to reschedule the correct stream here.
  if (stub && !stub->IsScheduled()) {
    // This stub won't get a chance to be scheduled so do that now.
    OnCommandBufferScheduled(stub.get());
  }

  RemoveRoute(route_id);
}

void GpuChannel::OnCreateStreamTexture(int32_t stream_id, bool* succeeded) {
#if defined(OS_ANDROID)
  auto found = stream_textures_.find(stream_id);
  if (found != stream_textures_.end()) {
    LOG(ERROR)
        << "Trying to create a StreamTexture with an existing stream_id.";
    *succeeded = false;
    return;
  }
  scoped_refptr<StreamTexture> stream_texture =
      StreamTexture::Create(this, stream_id);
  if (!stream_texture) {
    *succeeded = false;
    return;
  }
  stream_textures_.emplace(stream_id, std::move(stream_texture));
  *succeeded = true;
#else
  *succeeded = false;
#endif
}

void GpuChannel::CacheShader(const std::string& key,
                             const std::string& shader) {
  gpu_channel_manager_->delegate()->StoreShaderToDisk(client_id_, key, shader);
}

void GpuChannel::AddFilter(IPC::MessageFilter* filter) {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuChannelMessageFilter::AddChannelFilter,
                                filter_, base::RetainedRef(filter)));
}

void GpuChannel::RemoveFilter(IPC::MessageFilter* filter) {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuChannelMessageFilter::RemoveChannelFilter,
                                filter_, base::RetainedRef(filter)));
}

uint64_t GpuChannel::GetMemoryUsage() const {
  // Collect the unique memory trackers in use by the |stubs_|.
  base::flat_set<MemoryTracker*> unique_memory_trackers;
  unique_memory_trackers.reserve(stubs_.size());
  uint64_t size = 0;
  for (const auto& kv : stubs_) {
    MemoryTracker* tracker = kv.second->GetMemoryTracker();
    if (!unique_memory_trackers.insert(tracker).second) {
      // We already counted that tracker.
      continue;
    }
    size += tracker->GetSize();
  }
  size += shared_image_stub_->GetSize();

  return size;
}

scoped_refptr<gl::GLImage> GpuChannel::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    SurfaceHandle surface_handle) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      if (!base::IsValueInRangeForNumericType<size_t>(handle.stride))
        return nullptr;
      auto image = base::MakeRefCounted<gl::GLImageSharedMemory>(size);
      if (!image->Initialize(handle.region, handle.id, format, handle.offset,
                             handle.stride)) {
        return nullptr;
      }

      return image;
    }
    default: {
      GpuChannelManager* manager = gpu_channel_manager();
      if (!manager->gpu_memory_buffer_factory())
        return nullptr;

      return manager->gpu_memory_buffer_factory()
          ->AsImageFactory()
          ->CreateImageForGpuMemoryBuffer(std::move(handle), size, format,
                                          client_id_, surface_handle);
    }
  }
}

}  // namespace gpu
