// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/command_buffer_proxy_impl.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/cpu_reduction_experiment.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/presentation_feedback_utils.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
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
    int32_t stream_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::SharedMemoryMapper* transfer_buffer_mapper)
    : channel_(std::move(channel)),
      channel_id_(channel_->channel_id()),
      route_id_(channel_->GenerateRouteID()),
      stream_id_(stream_id),
      command_buffer_id_(
          CommandBufferIdFromChannelAndRoute(channel_id_, route_id_)),
      callback_thread_(std::move(task_runner)),
      transfer_buffer_mapper_(transfer_buffer_mapper) {
  DCHECK(route_id_);
}

CommandBufferProxyImpl::~CommandBufferProxyImpl() {
  for (auto& observer : deletion_observers_)
    observer.OnWillDeleteImpl();
  DisconnectChannel();
  CancelAllQueries();
}

ContextResult CommandBufferProxyImpl::Initialize(
    gpu::SurfaceHandle surface_handle,
    CommandBufferProxyImpl* share_group,
    gpu::SchedulingPriority stream_priority,
    const gpu::ContextCreationAttribs& attribs,
    const GURL& active_url) {
  DCHECK(!share_group || (stream_id_ == share_group->stream_id_));
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateViewCommandBuffer");

  // Drop the |channel_| if this method does not succeed and early-outs, to
  // prevent cleanup on destruction.
  auto channel = std::move(channel_);

  auto params = mojom::CreateCommandBufferParams::New();
#if BUILDFLAG(IS_ANDROID)
  params->surface_handle = surface_handle;
#else
  CHECK(surface_handle == gpu::kNullSurfaceHandle);
#endif
  params->share_group_id =
      share_group ? share_group->route_id_ : MSG_ROUTING_NONE;
  params->stream_id = stream_id_;
  params->stream_priority = stream_priority;
  params->attribs = attribs;
  params->active_url = active_url;

  TRACE_EVENT0("gpu", "CommandBufferProxyImpl::Initialize");
  std::tie(shared_state_shm_, shared_state_mapping_) =
      AllocateAndMapSharedMemory(sizeof(*shared_state()));
  if (!shared_state_shm_.IsValid()) {
    DLOG(ERROR) << "ContextResult::kFatalFailure: "
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

  // We're blocking the UI thread, which is generally undesirable.
  // In this case we need to wait for this before we can show any UI /anyway/,
  // so it won't cause additional jank.
  // TODO(piman): Make this asynchronous (http://crbug.com/125248).
  ContextResult result = ContextResult::kSuccess;
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
  IPC::ScopedAllowOffSequenceChannelAssociatedBindings allow_binding;
  bool sent = channel->GetGpuChannel().CreateCommandBuffer(
      std::move(params), route_id_, std::move(region),
      command_buffer_.BindNewEndpointAndPassReceiver(channel->io_task_runner()),
      client_receiver_.BindNewEndpointAndPassRemote(callback_thread_), &result,
      &capabilities_, &gl_capabilities_);
  if (!sent) {
    command_buffer_.reset();
    client_receiver_.reset();
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "Failed to send GpuControl.CreateCommandBuffer.";
    return ContextResult::kTransientFailure;
  }
  if (result != ContextResult::kSuccess) {
    command_buffer_.reset();
    client_receiver_.reset();
    DLOG(ERROR) << "Failure processing GpuControl.CreateCommandBuffer.";
    return result;
  }

  client_receiver_.set_disconnect_handler(base::BindOnce(
      &CommandBufferProxyImpl::OnDisconnect, base::Unretained(this)));

  channel_ = std::move(channel);
  return result;
}

void CommandBufferProxyImpl::OnDisconnect() {
  base::AutoLockMaybe lock(lock_.get());
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
  base::AutoLockMaybe lock(lock_.get());
  base::AutoLock last_state_lock(last_state_lock_);
  OnGpuAsyncMessageError(reason, error);
}

void CommandBufferProxyImpl::OnConsoleMessage(const std::string& message) {
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlErrorMessage(message.c_str(), /*id=*/0);
}

void CommandBufferProxyImpl::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
  if (gpu_control_client_)
    gpu_control_client_->OnGpuSwitched(active_gpu_heuristic);
}

void CommandBufferProxyImpl::AddDeletionObserver(DeletionObserver* observer) {
  base::AutoLockMaybe lock(lock_.get());
  deletion_observers_.AddObserver(observer);
}

void CommandBufferProxyImpl::RemoveDeletionObserver(
    DeletionObserver* observer) {
  base::AutoLockMaybe lock(lock_.get());
  deletion_observers_.RemoveObserver(observer);
}

void CommandBufferProxyImpl::UpdateLastFenceSyncRelease(
    uint64_t release_count) {
  CheckLock();
  if (last_fence_sync_release_ < release_count) {
    last_fence_sync_release_ = release_count;
  }
}
void CommandBufferProxyImpl::OnSignalAck(uint32_t id,
                                         const CommandBuffer::State& state) {
  base::AutoLockMaybe lock(lock_.get());
  {
    base::AutoLock last_state_lock(last_state_lock_);
    SetStateFromMessageReply(state);
    if (last_state_.error != gpu::error::kNoError)
      return;
  }
  SignalTaskMap::iterator it = signal_tasks_.find(id);
  if (it == signal_tasks_.end()) {
    LOG(ERROR) << "Gpu process sent invalid SignalAck.";
    base::AutoLock last_state_lock(last_state_lock_);
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
      route_id_, put_offset, std::move(pending_sync_token_fences_),
      last_fence_sync_release_);
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
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
    gpu::CommandBuffer::State state;
    if (channel_->GetGpuChannel().WaitForTokenInRange(route_id_, start, end,
                                                      &state)) {
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
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
    gpu::CommandBuffer::State state;
    if (channel_->GetGpuChannel().WaitForGetOffsetInRange(
            route_id_, set_get_buffer_count, start, end, &state)) {
      SetStateFromMessageReply(state);
    }
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

  command_buffer_->SetGetBuffer(shm_id);
  last_put_offset_ = -1;
  has_buffer_ = (shm_id > 0);
}

scoped_refptr<gpu::Buffer> CommandBufferProxyImpl::CreateTransferBuffer(
    uint32_t size,
    int32_t* id,
    uint32_t alignment,
    TransferBufferAllocationOption option) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  *id = -1;

  int32_t new_id = GetNextBufferId();

  base::UnsafeSharedMemoryRegion shared_memory_region;
  base::WritableSharedMemoryMapping shared_memory_mapping;
  std::tie(shared_memory_region, shared_memory_mapping) =
      AllocateAndMapSharedMemory(size, transfer_buffer_mapper_);
  if (!shared_memory_mapping.IsValid()) {
    if (last_state_.error == gpu::error::kNoError &&
        option != TransferBufferAllocationOption::kReturnNullOnOOM)
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
    command_buffer_->RegisterTransferBuffer(new_id, std::move(region));
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
      mojom::DeferredRequestParams::NewCommandBufferRequest(
          mojom::DeferredCommandBufferRequest::New(
              route_id_, mojom::DeferredCommandBufferRequestParams::
                             NewDestroyTransferBuffer(id))),
      /*sync_token_fences=*/{}, /*release_count=*/0);
}

void CommandBufferProxyImpl::ForceLostContext(error::ContextLostReason reason) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error == gpu::error::kLostContext) {
    // Per specification, do nothing if the context is already lost.
    return;
  }
  last_state_.error = gpu::error::kLostContext;
  // The caller determines the context lost reason.
  last_state_.context_lost_reason = reason;
  // Calling code may be in an indeterminate state (possibly including
  // being in a GpuControlClient callback), so avoid re-entering the
  // GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::SetGpuControlClient(GpuControlClient* client) {
  CheckLock();
  gpu_control_client_ = client;
}

const gpu::Capabilities& CommandBufferProxyImpl::GetCapabilities() const {
  return capabilities_;
}

const gpu::GLCapabilities& CommandBufferProxyImpl::GetGLCapabilities() const {
  return gl_capabilities_;
}

void CommandBufferProxyImpl::SetLock(base::Lock* lock) {
  lock_ = lock;
}

void CommandBufferProxyImpl::EnsureWorkVisible() {
  // Don't send messages once disconnected.
  if (disconnected_)
    return;

  constexpr char kEnsureWorkVisible[] = "EnsureWorkVisible";

  const base::ElapsedTimer elapsed_timer;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("gpu,login", kEnsureWorkVisible,
                                    TRACE_ID_LOCAL(kEnsureWorkVisible));

  channel_->VerifyFlush(UINT32_MAX);

  TRACE_EVENT_NESTABLE_ASYNC_END0("gpu,login", kEnsureWorkVisible,
                                  TRACE_ID_LOCAL(kEnsureWorkVisible));

  if (base::ShouldLogHistogramForCpuReductionExperiment()) {
    GetUMAHistogramEnsureWorkVisibleDuration()->Add(
        elapsed_timer.Elapsed().InMicroseconds());

    UMA_HISTOGRAM_CUSTOM_TIMES("GPU.EnsureWorkVisibleDurationLowRes",
                               elapsed_timer.Elapsed(), base::Milliseconds(1),
                               base::Seconds(5), 100);
  }
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
  return ++last_fence_sync_release_;
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
  command_buffer_->SignalSyncToken(sync_token, signal_id);
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
  command_buffer_->SignalQuery(query, signal_id);
  signal_tasks_.insert(std::make_pair(signal_id, std::move(callback)));
}

void CommandBufferProxyImpl::CancelAllQueries() {
  // Clear all of the signal query callbacks.
  signal_tasks_.clear();
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
  command_buffer_->CreateGpuFenceFromHandle(
      gpu_fence_id, gpu_fence->GetGpuFenceHandle().Clone());
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

  command_buffer_->GetGpuFenceHandle(
      gpu_fence_id,
      base::BindOnce(&CommandBufferProxyImpl::OnGetGpuFenceHandleComplete,
                     // TODO(crbug.com/40061562): Remove
                     // `UnsafeDanglingUntriaged`
                     base::UnsafeDanglingUntriaged(this), gpu_fence_id,
                     std::move(callback)));
}

void CommandBufferProxyImpl::OnGetGpuFenceHandleComplete(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback,
    gfx::GpuFenceHandle handle) {
  std::move(callback).Run(std::make_unique<gfx::GpuFence>(std::move(handle)));
}

void CommandBufferProxyImpl::OnReturnData(const std::vector<uint8_t>& data) {
  if (gpu_control_client_) {
    gpu_control_client_->OnGpuControlReturnData(data);
  }
}

void CommandBufferProxyImpl::SetDefaultFramebufferSharedImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    int samples_count,
    bool preserve,
    bool needs_depth,
    bool needs_stencil) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  last_flush_id_ = channel_->EnqueueDeferredMessage(
      mojom::DeferredRequestParams::NewCommandBufferRequest(
          mojom::DeferredCommandBufferRequest::New(
              route_id_,
              mojom::DeferredCommandBufferRequestParams::
                  NewSetDefaultFramebufferSharedImage(
                      mojom::SetDefaultFramebufferSharedImageParams::New(
                          mailbox, samples_count, preserve, needs_depth,
                          needs_stencil)))),
      {sync_token}, /*release_count=*/0);
}

std::pair<base::UnsafeSharedMemoryRegion, base::WritableSharedMemoryMapping>
CommandBufferProxyImpl::AllocateAndMapSharedMemory(
    size_t size,
    base::SharedMemoryMapper* mapper) {
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size);
  if (!region.IsValid()) {
    DLOG(ERROR) << "AllocateAndMapSharedMemory: Allocation failed";
    return {};
  }

  base::WritableSharedMemoryMapping mapping = region.Map(mapper);
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "AllocateAndMapSharedMemory: Map failed";
    return {};
  }

  return {std::move(region), std::move(mapping)};
}

void CommandBufferProxyImpl::SetStateFromMessageReply(
    const gpu::CommandBuffer::State& state) {
  CheckLock();
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
  if (last_state_.error == gpu::error::kNoError) {
    shared_state()->Read(&last_state_);
    if (last_state_.error != gpu::error::kNoError)
      OnGpuStateError();
  }
}

void CommandBufferProxyImpl::TryUpdateStateThreadSafe() {
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
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

gpu::CommandBufferSharedState* CommandBufferProxyImpl::shared_state() const {
  return reinterpret_cast<gpu::CommandBufferSharedState*>(
      shared_state_mapping_.memory());
}

base::HistogramBase*
CommandBufferProxyImpl::GetUMAHistogramEnsureWorkVisibleDuration() {
  if (!uma_histogram_ensure_work_visible_duration_) {
    // Combine two linear histograms:
    // 1/4 ms buckets in 0..15ms = 60 buckets
    // 1/4 s buckets in 25ms .. 30s = 120 buckets
    // This gives good cardinality for both VSYNC intervals and 30s watchdog:
    // 0.00 ms
    // 0.25 ms
    // 0.50 ms
    // 0.75 ms
    // 1.00 ms
    // 1.25 ms
    // 1.50 ms
    // ...
    // 14.00 ms
    // 14.25 ms
    // 14.50 ms
    // 14.75 ms
    // 15.00 ms
    // 0.25 s
    // 0.50 s
    // 0.75 s
    // 1.00 s
    // 1.25 s
    // 1.50 s
    // ...
    // 29.00 s
    // 29.25 s
    // 29.50 s
    // 29.75 s
    //
    // Histogram values are in microseconds.

    std::vector<base::HistogramBase::Sample> intervals;
    constexpr base::HistogramBase::Sample k15Milliseconds = 15 * 1000;
    constexpr base::HistogramBase::Sample k30Seconds = 30 * 1000 * 1000;
    constexpr int kFirstPartCount = 60;
    constexpr int kSecondPartCount = 120;
    intervals.reserve(kFirstPartCount + kSecondPartCount);
    for (int i = 0; i <= kFirstPartCount; ++i) {
      intervals.push_back(k15Milliseconds /
                          static_cast<float>(kFirstPartCount) * i);
    }
    // 0 index was already populated by the first part.
    for (int i = 1; i < kSecondPartCount; ++i) {
      intervals.push_back(k30Seconds / static_cast<float>(kSecondPartCount) *
                          i);
    }
    uma_histogram_ensure_work_visible_duration_ =
        base::CustomHistogram::FactoryGet(
            "GPU.EnsureWorkVisibleDuration", intervals,
            base::HistogramBase::kUmaTargetedHistogramFlag);
  }
  return uma_histogram_ensure_work_visible_duration_;
}

void CommandBufferProxyImpl::OnGpuSyncReplyError() {
  CheckLock();
  last_state_.error = gpu::error::kLostContext;
  // This error typically happens while waiting for a synchronous
  // reply from the GPU process because the GPU process crashed.
  // Report this as a lost GPU channel rather than invalid GPU message.
  last_state_.context_lost_reason = gpu::error::kGpuChannelLost;
  // This method may be inside a callstack from the GpuControlClient (we got a
  // bad reply to something we are sending to the GPU process). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::OnGpuAsyncMessageError(
    gpu::error::ContextLostReason reason,
    gpu::error::Error error) {
  CheckLock();
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
  DCHECK_NE(gpu::error::kNoError, last_state_.error);
  // This method may be inside a callstack from the GpuControlClient (we
  // encountered an error while trying to perform some action). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::OnClientError(gpu::error::Error error) {
  CheckLock();
  last_state_.error = error;
  last_state_.context_lost_reason = gpu::error::kUnknown;
  // This method may be inside a callstack from the GpuControlClient (we
  // encountered an error while trying to perform some action). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::DisconnectChannelInFreshCallStack() {
  CheckLock();
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
  base::AutoLockMaybe lock(lock_.get());
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

  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
  channel_->GetGpuChannel().DestroyCommandBuffer(route_id_);

  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContext();
}

}  // namespace gpu
