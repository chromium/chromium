// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/command_buffer_proxy_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/presentation_feedback_utils.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/gpu_param_traits.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gpu_preference.h"

namespace gpu {

CommandBufferProxyImpl::CommandBufferProxyImpl(
    scoped_refptr<GpuChannelHost> channel,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    int32_t stream_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : channel_(std::move(channel)),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      channel_id_(channel_->channel_id()),
      route_id_(channel_->GenerateRouteID()),
      stream_id_(stream_id),
      command_buffer_id_(
          CommandBufferIdFromChannelAndRoute(channel_id_, route_id_)),
      callback_thread_(std::move(task_runner)) {
  DCHECK(route_id_);
}

CommandBufferProxyImpl::~CommandBufferProxyImpl() {
  for (auto& observer : deletion_observers_)
    observer.OnWillDeleteImpl();
  DisconnectChannel();
}

ContextResult CommandBufferProxyImpl::Initialize(
    gpu::SurfaceHandle surface_handle,
    CommandBufferProxyImpl* share_group,
    gpu::SchedulingPriority stream_priority,
    const gpu::ContextCreationAttribs& attribs,
    const GURL& active_url) {
  DCHECK(!share_group || (stream_id_ == share_group->stream_id_));
  TRACE_EVENT1("gpu", "GpuChannelHost::CreateViewCommandBuffer",
               "surface_handle", surface_handle);

  // Drop the |channel_| if this method does not succeed and early-outs, to
  // prevent cleanup on destruction.
  auto channel = std::move(channel_);

  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = surface_handle;
  init_params.share_group_id =
      share_group ? share_group->route_id_ : MSG_ROUTING_NONE;
  init_params.stream_id = stream_id_;
  init_params.stream_priority = stream_priority;
  init_params.attribs = attribs;
  init_params.active_url = active_url;

  TRACE_EVENT0("gpu", "CommandBufferProxyImpl::Initialize");
  std::tie(shared_state_shm_, shared_state_mapping_) =
      AllocateAndMapSharedMemory(sizeof(*shared_state()));
  if (!shared_state_shm_.IsValid()) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "AllocateAndMapSharedMemory failed";
    return ContextResult::kFatalFailure;
  }

  shared_state()->Initialize();

  base::UnsafeSharedMemoryRegion region = shared_state_shm_.Duplicate();
  if (!region.IsValid()) {
    // TODO(piman): ShareToGpuProcess should alert if it is failing due to
    // being out of file descriptors, in which case this is a fatal error
    // that won't be recovered from.
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "Shared memory region is not valid";
    return ContextResult::kTransientFailure;
  }

  // Route must be added before sending the message, otherwise messages sent
  // from the GPU process could race against adding ourselves to the filter.
  channel->AddRouteWithTaskRunner(route_id_, weak_ptr_factory_.GetWeakPtr(),
                                  callback_thread_);

  // We're blocking the UI thread, which is generally undesirable.
  // In this case we need to wait for this before we can show any UI /anyway/,
  // so it won't cause additional jank.
  // TODO(piman): Make this asynchronous (http://crbug.com/125248).
  ContextResult result = ContextResult::kSuccess;
  bool sent = channel->Send(new GpuChannelMsg_CreateCommandBuffer(
      init_params, route_id_, std::move(region), &result, &capabilities_));
  if (!sent) {
    channel->RemoveRoute(route_id_);
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "Failed to send GpuChannelMsg_CreateCommandBuffer.";
    return ContextResult::kTransientFailure;
  }
  if (result != ContextResult::kSuccess) {
    DLOG(ERROR) << "Failure processing GpuChannelMsg_CreateCommandBuffer.";
    channel->RemoveRoute(route_id_);
    return result;
  }

  channel_ = std::move(channel);
  return result;
}

bool CommandBufferProxyImpl::OnMessageReceived(const IPC::Message& message) {
  base::AutoLockMaybe lock(lock_);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CommandBufferProxyImpl, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Destroyed, OnDestroyed);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ConsoleMsg, OnConsoleMessage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_GpuSwitched, OnGpuSwitched);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalAck, OnSignalAck);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SwapBuffersCompleted,
                        OnSwapBuffersCompleted);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_BufferPresented, OnBufferPresented);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_GetGpuFenceHandleComplete,
                        OnGetGpuFenceHandleComplete);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ReturnData, OnReturnData);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled) {
    LOG(ERROR) << "Gpu process sent invalid message.";
    base::AutoLock last_state_lock(last_state_lock_);
    OnGpuAsyncMessageError(gpu::error::kInvalidGpuMessage,
                           gpu::error::kLostContext);
  }
  return handled;
}

void CommandBufferProxyImpl::OnChannelError() {
  base::AutoLockMaybe lock(lock_);
  base::AutoLock last_state_lock(last_state_lock_);

  gpu::error::ContextLostReason context_lost_reason =
      gpu::error::kGpuChannelLost;
  if (shared_state_mapping_.IsValid()) {
    // The GPU process might have intentionally been crashed
    // (exit_on_context_lost), so try to find out the original reason.
    TryUpdateStateDontReportError();
    if (last_state_.error == gpu::error::kLostContext)
      context_lost_reason = last_state_.context_lost_reason;
  }
  OnGpuAsyncMessageError(context_lost_reason, gpu::error::kLostContext);
}

void CommandBufferProxyImpl::OnDestroyed(gpu::error::ContextLostReason reason,
                                         gpu::error::Error error) {
  base::AutoLock lock(last_state_lock_);
  OnGpuAsyncMessageError(reason, error);
}

void CommandBufferProxyImpl::OnConsoleMessage(
    const GPUCommandBufferConsoleMessage& message) {
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlErrorMessage(message.message.c_str(),
                                                  message.id);
}

void CommandBufferProxyImpl::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
  if (gpu_control_client_)
    gpu_control_client_->OnGpuSwitched(active_gpu_heuristic);
}

void CommandBufferProxyImpl::AddDeletionObserver(DeletionObserver* observer) {
  std::unique_ptr<base::AutoLock> lock;
  if (lock_)
    lock.reset(new base::AutoLock(*lock_));
  deletion_observers_.AddObserver(observer);
}

void CommandBufferProxyImpl::RemoveDeletionObserver(
    DeletionObserver* observer) {
  std::unique_ptr<base::AutoLock> lock;
  if (lock_)
    lock.reset(new base::AutoLock(*lock_));
  deletion_observers_.RemoveObserver(observer);
}

void CommandBufferProxyImpl::OnSignalAck(uint32_t id,
                                         const CommandBuffer::State& state) {
  {
    base::AutoLock lock(last_state_lock_);
    SetStateFromMessageReply(state);
    if (last_state_.error != gpu::error::kNoError)
      return;
  }
  SignalTaskMap::iterator it = signal_tasks_.find(id);
  if (it == signal_tasks_.end()) {
    LOG(ERROR) << "Gpu process sent invalid SignalAck.";
    base::AutoLock lock(last_state_lock_);
    OnGpuAsyncMessageError(gpu::error::kInvalidGpuMessage,
                           gpu::error::kLostContext);
    return;
  }
  base::OnceClosure callback = std::move(it->second);
  signal_tasks_.erase(it);
  std::move(callback).Run();
}

CommandBuffer::State CommandBufferProxyImpl::GetLastState() {
  base::AutoLock lock(last_state_lock_);
  TryUpdateState();
  return last_state_;
}

void CommandBufferProxyImpl::Flush(int32_t put_offset) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  TRACE_EVENT1("gpu", "CommandBufferProxyImpl::Flush", "put_offset",
               put_offset);

  OrderingBarrierHelper(put_offset);

  // Don't send messages once disconnected.
  if (!disconnected_)
    channel_->EnsureFlush(last_flush_id_);
}

void CommandBufferProxyImpl::OrderingBarrier(int32_t put_offset) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  TRACE_EVENT1("gpu", "CommandBufferProxyImpl::OrderingBarrier", "put_offset",
               put_offset);

  OrderingBarrierHelper(put_offset);
}

void CommandBufferProxyImpl::OrderingBarrierHelper(int32_t put_offset) {
  DCHECK(has_buffer_);

  if (last_put_offset_ == put_offset)
    return;
  last_put_offset_ = put_offset;
  last_flush_id_ = channel_->OrderingBarrier(
      route_id_, put_offset, std::move(pending_sync_token_fences_));
}

void CommandBufferProxyImpl::SetUpdateVSyncParametersCallback(
    const UpdateVSyncParametersCallback& callback) {
  CheckLock();
  update_vsync_parameters_completion_callback_ = callback;
}

gpu::CommandBuffer::State CommandBufferProxyImpl::WaitForTokenInRange(
    int32_t start,
    int32_t end) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  TRACE_EVENT2("gpu", "CommandBufferProxyImpl::WaitForToken", "start", start,
               "end", end);
  // Error needs to be checked in case the state was updated on another thread.
  // We need to make sure that the reentrant context loss callback is called so
  // that the share group is also lost before we return any error up the stack.
  if (last_state_.error != gpu::error::kNoError) {
    if (gpu_control_client_)
      gpu_control_client_->OnGpuControlLostContextMaybeReentrant();
    return last_state_;
  }
  TryUpdateState();
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_WaitForTokenInRange(route_id_, start, end,
                                                         &state))) {
      SetStateFromMessageReply(state);
    }
  }
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    LOG(ERROR) << "GPU state invalid after WaitForTokenInRange.";
    OnGpuSyncReplyError();
  }
  return last_state_;
}

gpu::CommandBuffer::State CommandBufferProxyImpl::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  TRACE_EVENT2("gpu", "CommandBufferProxyImpl::WaitForGetOffset", "start",
               start, "end", end);
  // Error needs to be checked in case the state was updated on another thread.
  // We need to make sure that the reentrant context loss callback is called so
  // that the share group is also lost before we return any error up the stack.
  if (last_state_.error != gpu::error::kNoError) {
    if (gpu_control_client_)
      gpu_control_client_->OnGpuControlLostContextMaybeReentrant();
    return last_state_;
  }
  TryUpdateState();
  if (((set_get_buffer_count != last_state_.set_get_buffer_count) ||
       !InRange(start, end, last_state_.get_offset)) &&
      last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_WaitForGetOffsetInRange(
            route_id_, set_get_buffer_count, start, end, &state)))
      SetStateFromMessageReply(state);
  }
  if (((set_get_buffer_count != last_state_.set_get_buffer_count) ||
       !InRange(start, end, last_state_.get_offset)) &&
      last_state_.error == gpu::error::kNoError) {
    LOG(ERROR) << "GPU state invalid after WaitForGetOffsetInRange.";
    OnGpuSyncReplyError();
  }
  return last_state_;
}

void CommandBufferProxyImpl::SetGetBuffer(int32_t shm_id) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_SetGetBuffer(route_id_, shm_id));
  last_put_offset_ = -1;
  has_buffer_ = (shm_id > 0);
}

scoped_refptr<gpu::Buffer> CommandBufferProxyImpl::CreateTransferBuffer(
    uint32_t size,
    int32_t* id) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  *id = -1;

  int32_t new_id = GetNextBufferId();

  base::UnsafeSharedMemoryRegion shared_memory_region;
  base::WritableSharedMemoryMapping shared_memory_mapping;
  std::tie(shared_memory_region, shared_memory_mapping) =
      AllocateAndMapSharedMemory(size);
  if (!shared_memory_mapping.IsValid()) {
    if (last_state_.error == gpu::error::kNoError)
      OnClientError(gpu::error::kOutOfBounds);
    return nullptr;
  }
  DCHECK_LE(shared_memory_mapping.size(), static_cast<size_t>(UINT32_MAX));

  if (last_state_.error == gpu::error::kNoError) {
    base::UnsafeSharedMemoryRegion region = shared_memory_region.Duplicate();
    if (!region.IsValid()) {
      if (last_state_.error == gpu::error::kNoError)
        OnClientError(gpu::error::kLostContext);
      return nullptr;
    }
    Send(new GpuCommandBufferMsg_RegisterTransferBuffer(route_id_, new_id,
                                                        std::move(region)));
  }

  *id = new_id;
  scoped_refptr<gpu::Buffer> buffer(gpu::MakeBufferFromSharedMemory(
      std::move(shared_memory_region), std::move(shared_memory_mapping)));
  return buffer;
}

void CommandBufferProxyImpl::DestroyTransferBuffer(int32_t id) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  last_flush_id_ = channel_->EnqueueDeferredMessage(
      GpuCommandBufferMsg_DestroyTransferBuffer(route_id_, id));
}

void CommandBufferProxyImpl::SetGpuControlClient(GpuControlClient* client) {
  CheckLock();
  gpu_control_client_ = client;
}

const gpu::Capabilities& CommandBufferProxyImpl::GetCapabilities() const {
  return capabilities_;
}

int32_t CommandBufferProxyImpl::CreateImage(ClientBuffer buffer,
                                            size_t width,
                                            size_t height) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return -1;

  int32_t new_id = channel_->ReserveImageId();

  gfx::GpuMemoryBuffer* gpu_memory_buffer =
      reinterpret_cast<gfx::GpuMemoryBuffer*>(buffer);
  DCHECK(gpu_memory_buffer);

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the CreateImage IPC below.
  gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->CloneHandle();
  bool requires_sync_token = handle.type == gfx::IO_SURFACE_BUFFER;

  uint64_t image_fence_sync = 0;
  if (requires_sync_token)
    image_fence_sync = GenerateFenceSyncRelease();

  DCHECK(gpu::IsImageFromGpuMemoryBufferFormatSupported(
      gpu_memory_buffer->GetFormat(), capabilities_))
      << gfx::BufferFormatToString(gpu_memory_buffer->GetFormat());
  DCHECK(gpu::IsImageSizeValidForGpuMemoryBufferFormat(
      gfx::Size(width, height), gpu_memory_buffer->GetFormat()))
      << gfx::BufferFormatToString(gpu_memory_buffer->GetFormat());

  GpuCommandBufferMsg_CreateImage_Params params;
  params.id = new_id;
  params.gpu_memory_buffer = std::move(handle);
  params.size = gfx::Size(width, height);
  params.format = gpu_memory_buffer->GetFormat();
  params.image_release_count = image_fence_sync;

  Send(new GpuCommandBufferMsg_CreateImage(route_id_, params));

  if (image_fence_sync) {
    gpu::SyncToken sync_token(GetNamespaceID(), GetCommandBufferID(),
                              image_fence_sync);

    // Force a synchronous IPC to validate sync token.
    EnsureWorkVisible();
    sync_token.SetVerifyFlush();

    gpu_memory_buffer_manager_->SetDestructionSyncToken(gpu_memory_buffer,
                                                        sync_token);
  }

  return new_id;
}

void CommandBufferProxyImpl::DestroyImage(int32_t id) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_DestroyImage(route_id_, id));
}

void CommandBufferProxyImpl::SetLock(base::Lock* lock) {
  lock_ = lock;
}

void CommandBufferProxyImpl::EnsureWorkVisible() {
  // Don't send messages once disconnected.
  if (!disconnected_)
    channel_->VerifyFlush(UINT32_MAX);
}

gpu::CommandBufferNamespace CommandBufferProxyImpl::GetNamespaceID() const {
  return gpu::CommandBufferNamespace::GPU_IO;
}

gpu::CommandBufferId CommandBufferProxyImpl::GetCommandBufferID() const {
  return command_buffer_id_;
}

void CommandBufferProxyImpl::FlushPendingWork() {
  // Don't send messages once disconnected.
  if (!disconnected_)
    channel_->EnsureFlush(UINT32_MAX);
}

uint64_t CommandBufferProxyImpl::GenerateFenceSyncRelease() {
  CheckLock();
  return next_fence_sync_release_++;
}

// This can be called from any thread without holding |lock_|. Use a thread-safe
// non-error throwing variant of TryUpdateState for this.
bool CommandBufferProxyImpl::IsFenceSyncReleased(uint64_t release) {
  base::AutoLock lock(last_state_lock_);
  TryUpdateStateThreadSafe();
  return release <= last_state_.release_count;
}

void CommandBufferProxyImpl::SignalSyncToken(const gpu::SyncToken& sync_token,
                                             base::OnceClosure callback) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  uint32_t signal_id = next_signal_id_++;
  Send(new GpuCommandBufferMsg_SignalSyncToken(route_id_, sync_token,
                                               signal_id));
  signal_tasks_.insert(std::make_pair(signal_id, std::move(callback)));
}

void CommandBufferProxyImpl::WaitSyncToken(const gpu::SyncToken& sync_token) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  pending_sync_token_fences_.push_back(sync_token);
}

bool CommandBufferProxyImpl::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken& sync_token) {
  // Can only wait on an unverified sync token if it is from the same channel.
  int sync_token_channel_id =
      ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
  if (sync_token.namespace_id() != gpu::CommandBufferNamespace::GPU_IO ||
      sync_token_channel_id != channel_id_) {
    return false;
  }
  return true;
}

void CommandBufferProxyImpl::SignalQuery(uint32_t query,
                                         base::OnceClosure callback) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  // Signal identifiers are hidden, so nobody outside of this class will see
  // them. (And thus, they cannot save them.) The IDs themselves only last
  // until the callback is invoked, which will happen as soon as the GPU
  // catches upwith the command buffer.
  // A malicious caller trying to create a collision by making next_signal_id
  // would have to make calls at an astounding rate (300B/s) and even if they
  // could do that, all they would do is to prevent some callbacks from getting
  // called, leading to stalled threads and/or memory leaks.
  uint32_t signal_id = next_signal_id_++;
  Send(new GpuCommandBufferMsg_SignalQuery(route_id_, query, signal_id));
  signal_tasks_.insert(std::make_pair(signal_id, std::move(callback)));
}

void CommandBufferProxyImpl::CreateGpuFence(uint32_t gpu_fence_id,
                                            ClientGpuFence source) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError) {
    DLOG(ERROR) << "got error=" << last_state_.error;
    return;
  }

  gfx::GpuFence* gpu_fence = gfx::GpuFence::FromClientGpuFence(source);
  gfx::GpuFenceHandle handle =
      gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle());
  Send(new GpuCommandBufferMsg_CreateGpuFenceFromHandle(route_id_, gpu_fence_id,
                                                        handle));
}

void CommandBufferProxyImpl::SetDisplayTransform(
    gfx::OverlayTransform transform) {
  NOTREACHED();
}

void CommandBufferProxyImpl::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError) {
    DLOG(ERROR) << "got error=" << last_state_.error;
    return;
  }

  Send(new GpuCommandBufferMsg_GetGpuFenceHandle(route_id_, gpu_fence_id));
  get_gpu_fence_tasks_.emplace(gpu_fence_id, std::move(callback));
}

void CommandBufferProxyImpl::OnGetGpuFenceHandleComplete(
    uint32_t gpu_fence_id,
    const gfx::GpuFenceHandle& handle) {
  // Always consume the provided handle to avoid leaks on error.
  auto gpu_fence = std::make_unique<gfx::GpuFence>(handle);

  GetGpuFenceTaskMap::iterator it = get_gpu_fence_tasks_.find(gpu_fence_id);
  if (it == get_gpu_fence_tasks_.end()) {
    DLOG(ERROR) << "GPU process sent invalid GetGpuFenceHandle response.";
    base::AutoLock lock(last_state_lock_);
    OnGpuAsyncMessageError(gpu::error::kInvalidGpuMessage,
                           gpu::error::kLostContext);
    return;
  }
  auto callback = std::move(it->second);
  get_gpu_fence_tasks_.erase(it);
  std::move(callback).Run(std::move(gpu_fence));
}

void CommandBufferProxyImpl::OnReturnData(const std::vector<uint8_t>& data) {
  if (gpu_control_client_) {
    gpu_control_client_->OnGpuControlReturnData(data);
  }
}

void CommandBufferProxyImpl::TakeFrontBuffer(const gpu::Mailbox& mailbox) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  // TakeFrontBuffer should be a deferred message so that it's sequenced
  // correctly with respect to preceding ReturnFrontBuffer messages.
  last_flush_id_ = channel_->EnqueueDeferredMessage(
      GpuCommandBufferMsg_TakeFrontBuffer(route_id_, mailbox));
}

void CommandBufferProxyImpl::ReturnFrontBuffer(const gpu::Mailbox& mailbox,
                                               const gpu::SyncToken& sync_token,
                                               bool is_lost) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  last_flush_id_ = channel_->EnqueueDeferredMessage(
      GpuCommandBufferMsg_ReturnFrontBuffer(route_id_, mailbox, is_lost),
      {sync_token});
}

bool CommandBufferProxyImpl::Send(IPC::Message* msg) {
  DCHECK(channel_);
  last_state_lock_.AssertAcquired();
  DCHECK_EQ(gpu::error::kNoError, last_state_.error);

  last_state_lock_.Release();

  // Call is_sync() before sending message.
  bool is_sync = msg->is_sync();
  bool result = channel_->Send(msg);
  // Send() should always return true for async messages.
  DCHECK(is_sync || result);

  last_state_lock_.Acquire();

  if (last_state_.error != gpu::error::kNoError) {
    // Error needs to be checked in case the state was updated on another thread
    // while we were waiting on Send. We need to make sure that the reentrant
    // context loss callback is called so that the share group is also lost
    // before we return any error up the stack.
    if (gpu_control_client_)
      gpu_control_client_->OnGpuControlLostContextMaybeReentrant();
    return false;
  }

  if (!result) {
    // Flag the command buffer as lost. Defer deleting the channel until
    // OnChannelError is called after returning to the message loop in case it
    // is referenced elsewhere.
    DVLOG(1) << "CommandBufferProxyImpl::Send failed. Losing context.";
    OnClientError(gpu::error::kLostContext);
    return false;
  }

  return true;
}

std::pair<base::UnsafeSharedMemoryRegion, base::WritableSharedMemoryMapping>
CommandBufferProxyImpl::AllocateAndMapSharedMemory(size_t size) {
  base::UnsafeSharedMemoryRegion region =
      mojo::CreateUnsafeSharedMemoryRegion(size);
  if (!region.IsValid()) {
    DLOG(ERROR) << "AllocateAndMapSharedMemory: Allocation failed";
    return {};
  }

  base::WritableSharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "AllocateAndMapSharedMemory: Map failed";
    return {};
  }

  return {std::move(region), std::move(mapping)};
}

void CommandBufferProxyImpl::SetStateFromMessageReply(
    const gpu::CommandBuffer::State& state) {
  CheckLock();
  last_state_lock_.AssertAcquired();
  if (last_state_.error != gpu::error::kNoError)
    return;
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
  if (last_state_.error != gpu::error::kNoError)
    OnGpuStateError();
}

void CommandBufferProxyImpl::TryUpdateState() {
  CheckLock();
  last_state_lock_.AssertAcquired();
  if (last_state_.error == gpu::error::kNoError) {
    shared_state()->Read(&last_state_);
    if (last_state_.error != gpu::error::kNoError)
      OnGpuStateError();
  }
}

void CommandBufferProxyImpl::TryUpdateStateThreadSafe() {
  last_state_lock_.AssertAcquired();
  if (last_state_.error == gpu::error::kNoError) {
    shared_state()->Read(&last_state_);
    if (last_state_.error != gpu::error::kNoError) {
      callback_thread_->PostTask(
          FROM_HERE,
          base::BindOnce(&CommandBufferProxyImpl::LockAndDisconnectChannel,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void CommandBufferProxyImpl::TryUpdateStateDontReportError() {
  last_state_lock_.AssertAcquired();
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

gpu::CommandBufferSharedState* CommandBufferProxyImpl::shared_state() const {
  return reinterpret_cast<gpu::CommandBufferSharedState*>(
      shared_state_mapping_.memory());
}

void CommandBufferProxyImpl::OnSwapBuffersCompleted(
    const SwapBuffersCompleteParams& params) {
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlSwapBuffersCompleted(params);
}

void CommandBufferProxyImpl::OnBufferPresented(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  if (gpu_control_client_)
    gpu_control_client_->OnSwapBufferPresented(swap_id, feedback);
  if (update_vsync_parameters_completion_callback_ &&
      ShouldUpdateVsyncParams(feedback)) {
    update_vsync_parameters_completion_callback_.Run(feedback.timestamp,
                                                     feedback.interval);
  }
}

void CommandBufferProxyImpl::OnGpuSyncReplyError() {
  CheckLock();
  last_state_lock_.AssertAcquired();
  last_state_.error = gpu::error::kLostContext;
  last_state_.context_lost_reason = gpu::error::kInvalidGpuMessage;
  // This method may be inside a callstack from the GpuControlClient (we got a
  // bad reply to something we are sending to the GPU process). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::OnGpuAsyncMessageError(
    gpu::error::ContextLostReason reason,
    gpu::error::Error error) {
  CheckLock();
  last_state_lock_.AssertAcquired();
  last_state_.error = error;
  last_state_.context_lost_reason = reason;
  // This method only occurs when receiving IPC messages, so we know it's not in
  // a callstack from the GpuControlClient. Unlock the state lock to prevent
  // a deadlock when calling the context loss callback.
  base::AutoUnlock unlock(last_state_lock_);
  DisconnectChannel();
}

void CommandBufferProxyImpl::OnGpuStateError() {
  CheckLock();
  last_state_lock_.AssertAcquired();
  DCHECK_NE(gpu::error::kNoError, last_state_.error);
  // This method may be inside a callstack from the GpuControlClient (we
  // encountered an error while trying to perform some action). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::OnClientError(gpu::error::Error error) {
  CheckLock();
  last_state_lock_.AssertAcquired();
  last_state_.error = error;
  last_state_.context_lost_reason = gpu::error::kUnknown;
  // This method may be inside a callstack from the GpuControlClient (we
  // encountered an error while trying to perform some action). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::DisconnectChannelInFreshCallStack() {
  CheckLock();
  last_state_lock_.AssertAcquired();
  // Inform the GpuControlClient of the lost state immediately, though this may
  // be a re-entrant call to the client so we use the MaybeReentrant variant.
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContextMaybeReentrant();
  // Create a fresh call stack to keep the |channel_| alive while we unwind the
  // stack in case things will use it, and give the GpuChannelClient a chance to
  // act fully on the lost context.
  callback_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&CommandBufferProxyImpl::LockAndDisconnectChannel,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CommandBufferProxyImpl::LockAndDisconnectChannel() {
  base::AutoLockMaybe lock(lock_);
  DisconnectChannel();
}

void CommandBufferProxyImpl::DisconnectChannel() {
  CheckLock();
  // Prevent any further messages from being sent, and ensure we only call
  // the client for lost context a single time.
  if (!channel_ || disconnected_)
    return;
  disconnected_ = true;
  channel_->VerifyFlush(UINT32_MAX);
  channel_->Send(new GpuChannelMsg_DestroyCommandBuffer(route_id_));
  channel_->RemoveRoute(route_id_);
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContext();
}

}  // namespace gpu
