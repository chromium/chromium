// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/gpu_channel_host.h"

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_watchdog_timeout.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "url/gurl.h"

using base::AutoLock;

namespace gpu {

GpuChannelHost::GpuChannelHost(
    int channel_id,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::SharedImageCapabilities& shared_image_capabilities,
    mojo::ScopedMessagePipeHandle handle,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_thread_(io_task_runner
                     ? io_task_runner
                     : base::SingleThreadTaskRunner::GetCurrentDefault()),
      channel_id_(channel_id),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      listener_(new Listener(), base::OnTaskRunnerDeleter(io_thread_)),
      connection_tracker_(base::MakeRefCounted<ConnectionTracker>()),
      shared_image_interface_(
          this,
          static_cast<int32_t>(GpuChannelReservedRoutes::kSharedImageInterface),
          shared_image_capabilities),
      image_decode_accelerator_proxy_(
          this,
          static_cast<int32_t>(
              GpuChannelReservedRoutes::kImageDecodeAccelerator)),
      sync_point_graph_validation_enabled_(
          features::IsSyncPointGraphValidationEnabled()) {
  mojo::PendingAssociatedRemote<mojom::GpuChannel> channel;
  listener_->Initialize(std::move(handle),
                        channel.InitWithNewEndpointAndPassReceiver(),
                        io_thread_);
  gpu_channel_ = mojo::SharedAssociatedRemote<mojom::GpuChannel>(
      std::move(channel), io_thread_);
  gpu_channel_.set_disconnect_handler(
      base::BindOnce(&ConnectionTracker::OnDisconnectedFromGpuProcess,
                     connection_tracker_),
      io_thread_);

  next_image_id_.GetNext();
  for (int32_t i = 0;
       i <= static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue); ++i)
    next_route_id_.GetNext();
}

mojom::GpuChannel& GpuChannelHost::GetGpuChannel() {
  return *gpu_channel_.get();
}

uint32_t GpuChannelHost::OrderingBarrier(
    int32_t route_id,
    int32_t put_offset,
    std::vector<SyncToken> sync_token_fences,
    uint64_t release_count) {
  AutoLock lock(deferred_message_lock_);

  if (pending_ordering_barrier_ &&
      pending_ordering_barrier_->route_id != route_id)
    EnqueuePendingOrderingBarrier();
  if (!pending_ordering_barrier_)
    pending_ordering_barrier_.emplace();

  pending_ordering_barrier_->deferred_message_id = next_deferred_message_id_++;
  pending_ordering_barrier_->route_id = route_id;
  pending_ordering_barrier_->put_offset = put_offset;
  pending_ordering_barrier_->sync_token_fences.insert(
      pending_ordering_barrier_->sync_token_fences.end(),
      std::make_move_iterator(sync_token_fences.begin()),
      std::make_move_iterator(sync_token_fences.end()));
  pending_ordering_barrier_->release_count = release_count;
  return pending_ordering_barrier_->deferred_message_id;
}

uint32_t GpuChannelHost::EnqueueDeferredMessage(
    mojom::DeferredRequestParamsPtr params,
    std::vector<SyncToken> sync_token_fences,
    uint64_t release_count) {
  AutoLock lock(deferred_message_lock_);

  EnqueuePendingOrderingBarrier();
  enqueued_deferred_message_id_ = next_deferred_message_id_++;
  deferred_messages_.push_back(mojom::DeferredRequest::New(
      std::move(params), std::move(sync_token_fences), release_count));
  return enqueued_deferred_message_id_;
}

#if BUILDFLAG(IS_WIN)
void GpuChannelHost::CopyToGpuMemoryBufferAsync(
    const Mailbox& mailbox,
    std::vector<SyncToken> sync_token_dependencies,
    uint64_t release_count,
    base::OnceCallback<void(bool)> callback) {
  AutoLock lock(deferred_message_lock_);
  InternalFlush(UINT32_MAX);
  GetGpuChannel().CopyToGpuMemoryBufferAsync(
      mailbox, std::move(sync_token_dependencies), release_count,
      std::move(callback));
}

void GpuChannelHost::CopyNativeGmbToSharedMemorySync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    bool* status) {
  GetGpuChannel().CopyNativeGmbToSharedMemorySync(
      std::move(buffer_handle), std::move(memory_region), status);
}

void GpuChannelHost::CopyNativeGmbToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  GetGpuChannel().CopyNativeGmbToSharedMemoryAsync(
      std::move(buffer_handle), std::move(memory_region), std::move(callback));
}
#endif

void GpuChannelHost::EnsureFlush(uint32_t deferred_message_id) {
  AutoLock lock(deferred_message_lock_);
  InternalFlush(deferred_message_id);
}

void GpuChannelHost::VerifyFlush(uint32_t deferred_message_id) {
  uint32_t cached_flushed_deferred_message_id;
  {
    AutoLock lock(deferred_message_lock_);
    InternalFlush(deferred_message_id);
    cached_flushed_deferred_message_id = flushed_deferred_message_id_;
  }

  if (sync_point_graph_validation_enabled_) {
    // No need to do synchronous flush when graph validation of sync points is
    // enabled.
    return;
  }

  bool ipc_needed = false;
  const bool skip_flush_if_possible =
      base::FeatureList::IsEnabled(features::kConditionallySkipGpuChannelFlush);

  // A few different scenarios can happen here.
  //
  // 1) There is no attempt to skip the flush.
  // 2) A sync call is issued to establish the shared memory communication.
  //    This in itself syncs so no further IPCs are needed to flush.
  // 3) The communication is already established and confirms the need for an
  //    IPC.
  // 4) The communication is already established and confirms no need for
  //    an IPC. In that case the default value of `ipc_needed` which is false
  //    is used.
  //
  if (skip_flush_if_possible) {
    base::AutoLock lock(shared_memory_version_lock_);

    // If shared memory communication is not established, do so.
    if (!shared_memory_version_client_.has_value()) {
      mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
      EstablishSharedMemoryForFlushVerification();
      // A sync IPC was just completed which serves the same purpose as Flush()
      // which is a noop sync IPC. No need to continue.
      ipc_needed = false;
    }
    // GPUChannel has not processed ids up to the ones that were flushed. IPC
    // needed.
    else if (shared_memory_version_client_->SharedVersionIsLessThan(
                 cached_flushed_deferred_message_id)) {
      ipc_needed = true;
    }
  } else {
    ipc_needed = true;
  }

  // Flush is needed.
  if (ipc_needed) {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
    GetGpuChannel().Flush();
  }
}

void GpuChannelHost::EnqueuePendingOrderingBarrier() {
  deferred_message_lock_.AssertAcquired();
  if (!pending_ordering_barrier_)
    return;
  DCHECK_LT(enqueued_deferred_message_id_,
            pending_ordering_barrier_->deferred_message_id);
  enqueued_deferred_message_id_ =
      pending_ordering_barrier_->deferred_message_id;
  auto params = mojom::AsyncFlushParams::New(
      pending_ordering_barrier_->put_offset,
      pending_ordering_barrier_->deferred_message_id,
      pending_ordering_barrier_->sync_token_fences);
  deferred_messages_.push_back(mojom::DeferredRequest::New(
      mojom::DeferredRequestParams::NewCommandBufferRequest(
          mojom::DeferredCommandBufferRequest::New(
              pending_ordering_barrier_->route_id,
              mojom::DeferredCommandBufferRequestParams::NewAsyncFlush(
                  std::move(params)))),
      std::move(pending_ordering_barrier_->sync_token_fences),
      pending_ordering_barrier_->release_count));
  pending_ordering_barrier_.reset();
}

void GpuChannelHost::EstablishSharedMemoryForFlushVerification() {
  base::ReadOnlySharedMemoryRegion mapped_region;
  GetGpuChannel().GetSharedMemoryForFlushId(&mapped_region);
  if (mapped_region.IsValid()) {
    shared_memory_version_client_.emplace(std::move(mapped_region));
  }
}

void GpuChannelHost::InternalFlush(uint32_t deferred_message_id) {
  deferred_message_lock_.AssertAcquired();

  EnqueuePendingOrderingBarrier();
  if (!deferred_messages_.empty() &&
      deferred_message_id > flushed_deferred_message_id_) {
    DCHECK_EQ(enqueued_deferred_message_id_, next_deferred_message_id_ - 1);
    flushed_deferred_message_id_ = enqueued_deferred_message_id_;

    GetGpuChannel().FlushDeferredRequests(std::move(deferred_messages_),
                                          flushed_deferred_message_id_);
  }
}

void GpuChannelHost::DestroyChannel() {
  gpu_channel_.Disconnect();
  connection_tracker_->OnDisconnectedFromGpuProcess();
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&Listener::Close, base::Unretained(listener_.get())));
}

int32_t GpuChannelHost::ReserveImageId() {
  return next_image_id_.GetNext();
}

int32_t GpuChannelHost::GenerateRouteID() {
  return next_route_id_.GetNext();
}

void GpuChannelHost::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    const viz::SharedImageFormat& format,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle* buffer_handle) {
  GetGpuChannel().CreateGpuMemoryBuffer(size, format, buffer_usage,
                                        buffer_handle);
}

void GpuChannelHost::GetGpuMemoryBufferHandleInfo(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle* handle,
    viz::SharedImageFormat* format,
    gfx::Size* size,
    gfx::BufferUsage* buffer_usage) {
  GetGpuChannel().GetGpuMemoryBufferHandleInfo(mailbox, handle, format, size,
                                               buffer_usage);
}

void GpuChannelHost::CrashGpuProcessForTesting() {
  GetGpuChannel().CrashForTesting();
}

void GpuChannelHost::TerminateGpuProcessForTesting() {
  GetGpuChannel().TerminateForTesting();
}

scoped_refptr<ClientSharedImageInterface>
GpuChannelHost::CreateClientSharedImageInterface() {
  return base::MakeRefCounted<ClientSharedImageInterface>(
      &shared_image_interface_, this);
}

GpuChannelHost::~GpuChannelHost() = default;

GpuChannelHost::ConnectionTracker::ConnectionTracker() = default;

GpuChannelHost::ConnectionTracker::~ConnectionTracker() {
  CHECK(observer_list_.empty(), base::NotFatalUntil::M126);
}

void GpuChannelHost::ConnectionTracker::OnDisconnectedFromGpuProcess() {
  is_connected_.store(false);
  NotifyGpuChannelLost();
}

void GpuChannelHost::ConnectionTracker::AddObserver(
    GpuChannelLostObserver* obs) {
  AutoLock lock(channel_obs_lock_);
  CHECK(!base::Contains(observer_list_, obs), base::NotFatalUntil::M126);
  observer_list_.push_back(obs);
}

void GpuChannelHost::ConnectionTracker::RemoveObserver(
    GpuChannelLostObserver* obs) {
  AutoLock lock(channel_obs_lock_);
  std::erase(observer_list_, obs);
}

void GpuChannelHost::ConnectionTracker::NotifyGpuChannelLost() {
  AutoLock lock(channel_obs_lock_);
  for (GpuChannelLostObserver* observer : observer_list_) {
    observer->OnGpuChannelLost();
  }
  observer_list_.clear();
}

GpuChannelHost::OrderingBarrierInfo::OrderingBarrierInfo() = default;

GpuChannelHost::OrderingBarrierInfo::~OrderingBarrierInfo() = default;

GpuChannelHost::OrderingBarrierInfo::OrderingBarrierInfo(
    OrderingBarrierInfo&&) = default;

GpuChannelHost::OrderingBarrierInfo& GpuChannelHost::OrderingBarrierInfo::
operator=(OrderingBarrierInfo&&) = default;

GpuChannelHost::Listener::Listener() = default;

void GpuChannelHost::Listener::Initialize(
    mojo::ScopedMessagePipeHandle handle,
    mojo::PendingAssociatedReceiver<mojom::GpuChannel> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  base::AutoLock lock(lock_);
  channel_ =
      IPC::ChannelMojo::Create(std::move(handle), IPC::Channel::MODE_CLIENT,
                               this, io_task_runner, io_task_runner);
  DCHECK(channel_);
  bool result = channel_->Connect();
  DCHECK(result);
  channel_->GetAssociatedInterfaceSupport()->GetRemoteAssociatedInterface(
      std::move(receiver));
}

GpuChannelHost::Listener::~Listener() = default;

void GpuChannelHost::Listener::Close() {
  OnChannelError();
}

bool GpuChannelHost::Listener::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void GpuChannelHost::Listener::OnChannelError() {
  AutoLock lock(lock_);
  channel_ = nullptr;
}

void GpuChannelHost::AddObserver(GpuChannelLostObserver* obs) {
  connection_tracker_->AddObserver(obs);
}

void GpuChannelHost::RemoveObserver(GpuChannelLostObserver* obs) {
  connection_tracker_->RemoveObserver(obs);
}

}  // namespace gpu
