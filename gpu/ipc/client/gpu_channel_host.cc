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
#include "base/task/common/task_annotator.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/command_buffer_trace_utils.h"
#include "gpu/ipc/common/gpu_watchdog_timeout.h"
#include "ipc/ipc_channel.h"
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
      listener_(nullptr, base::OnTaskRunnerDeleter(io_thread_)),
      connection_tracker_(base::MakeRefCounted<ConnectionTracker>()),
      shared_image_interface_(
          this,
          static_cast<int32_t>(GpuChannelReservedRoutes::kSharedImageInterface),
          shared_image_capabilities),
      sync_point_graph_validation_enabled_(
          features::IsSyncPointGraphValidationEnabled()) {
  if (features::IsLegacyIpcDisabled()) {
    gpu_channel_.emplace<SharedRemote>(
        mojo::PendingRemote<mojom::GpuChannel>(std::move(handle), 0),
        io_thread_);
  } else {
    listener_ = std::unique_ptr<Listener, base::OnTaskRunnerDeleter>(
        new Listener(), base::OnTaskRunnerDeleter(io_thread_));
    mojo::PendingAssociatedRemote<mojom::GpuChannel> channel;
    listener_->Initialize(std::move(handle),
                          channel.InitWithNewEndpointAndPassReceiver(),
                          io_thread_);
    gpu_channel_.emplace<SharedAssociatedRemote>(
        mojo::SharedAssociatedRemote<mojom::GpuChannel>(std::move(channel),
                                                        io_thread_));
  }

  std::visit(
      [&](auto& gpu_channel_remote) {
        // Test callers may pass an invalid handle, leaving `gpu_channel_remote`
        // unbound.
        if (gpu_channel_remote) {
          gpu_channel_remote.set_disconnect_handler(
              base::BindOnce(&ConnectionTracker::OnDisconnectedFromGpuProcess,
                             connection_tracker_),
              io_thread_);
        }
      },
      gpu_channel_);

  next_image_id_.GetNext();
  for (int32_t i = 0;
       i <= static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue); ++i)
    next_route_id_.GetNext();
}

mojom::GpuChannel& GpuChannelHost::GetGpuChannel() {
  return *std::visit(
      [](auto& gpu_channel_remote) { return gpu_channel_remote.get(); },
      gpu_channel_);
}

uint32_t GpuChannelHost::OrderingBarrier(
    int32_t route_id,
    int32_t put_offset,
    std::vector<SyncToken> sync_token_fences,
    uint64_t release_count) {
  AutoLock lock(deferred_message_lock_);

  if (pending_ordering_barrier_ &&
      pending_ordering_barrier_->route_id != route_id) {
    EnqueuePendingOrderingBarrier();
  }

  unsigned int trace_event_flags = TRACE_EVENT_FLAG_FLOW_OUT;
  if (!pending_ordering_barrier_) {
    pending_ordering_barrier_.emplace();
    pending_ordering_barrier_->deferred_message_id =
        next_deferred_message_id_++;
  } else {
    trace_event_flags |= TRACE_EVENT_FLAG_FLOW_IN;
  }

  const uint64_t global_flush_id = GlobalFlushTracingId(
      channel_id_, pending_ordering_barrier_->deferred_message_id);
  TRACE_EVENT_WITH_FLOW0(
      "gpu,toplevel.flow", "CommandBuffer::OrderingBarrier",
      TRACE_ID_WITH_SCOPE("CommandBuffer::Flush", global_flush_id),
      trace_event_flags);

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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
void GpuChannelHost::CopyNativeGmbToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  // Some callers block on callback execution to map synchronously.
  // However, the callback will be executed on the IO thread.
  // So no Mapping call should be made from IO thread because it may
  // lead to a deadlock: the thread will wait for the callback to execute,
  // but the callback will be scheduled on the very same thread.
  CHECK(!io_thread_->BelongsToCurrentThread());
  GetGpuChannel().CopyNativeGmbToSharedMemoryAsync(
      std::move(buffer_handle), std::move(memory_region), std::move(callback));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

void GpuChannelHost::DelayedEnsureFlush(uint32_t deferred_message_id) {
  AutoLock lock(deferred_message_lock_);
  if (delayed_flush_deferred_message_id_) {
    delayed_flush_deferred_message_id_ = deferred_message_id;
  } else {
    delayed_flush_deferred_message_id_ = deferred_message_id;
    base::ThreadPool::PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](GpuChannelHost* host) {
              AutoLock lock(host->deferred_message_lock_);
              host->InternalFlush(
                  host->delayed_flush_deferred_message_id_.value());
              host->delayed_flush_deferred_message_id_ = std::nullopt;
            },
            base::RetainedRef(this)),
        kDelayForEnsuringFlush);
  }
}

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
    TRACE_EVENT0("gpu", "GpuChannelHost::VerifyFlush");
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
    GetGpuChannel().Flush();
  }
}

void GpuChannelHost::EnqueuePendingOrderingBarrier() {
  deferred_message_lock_.AssertAcquired();
  if (!pending_ordering_barrier_)
    return;

  const uint64_t global_flush_id = GlobalFlushTracingId(
      channel_id_, pending_ordering_barrier_->deferred_message_id);
  TRACE_EVENT_WITH_FLOW0(
      "gpu,toplevel.flow", "CommandBuffer::OrderingBarrier",
      TRACE_ID_WITH_SCOPE("CommandBuffer::Flush", global_flush_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

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
    if (TRACE_EVENT_CATEGORY_ENABLED("gpu,toplevel.flow")) {
      for (auto& message : deferred_messages_) {
        if (message->params->is_command_buffer_request()) {
          auto& command_buffer_request =
              message->params->get_command_buffer_request();
          if (command_buffer_request->params->is_async_flush()) {
            auto& flush = command_buffer_request->params->get_async_flush();
            const uint64_t global_flush_id =
                GlobalFlushTracingId(channel_id_, flush->flush_id);
            TRACE_EVENT_WITH_FLOW0(
                "gpu,toplevel.flow", "GpuChannel::Flush",
                TRACE_ID_WITH_SCOPE("CommandBuffer::Flush", global_flush_id),
                TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
          }
        }
      }
    }

    DCHECK_EQ(enqueued_deferred_message_id_, next_deferred_message_id_ - 1);
    flushed_deferred_message_id_ = enqueued_deferred_message_id_;

    GetGpuChannel().FlushDeferredRequests(std::move(deferred_messages_),
                                          flushed_deferred_message_id_);
  }
}

void GpuChannelHost::DestroyChannel() {
  std::visit([](auto& gpu_channel_remote) { gpu_channel_remote.Disconnect(); },
             gpu_channel_);
  connection_tracker_->OnDisconnectedFromGpuProcess();
  if (!features::IsLegacyIpcDisabled()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&Listener::Close, base::Unretained(listener_.get())));
  }
}

void GpuChannelHost::ResetChannelRemoteForTesting() {
  std::visit([](auto& gpu_channel_remote) { gpu_channel_remote.reset(); },
             gpu_channel_);
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

void GpuChannelHost::CrashGpuProcessForTesting() {
  GetGpuChannel().CrashForTesting();
}

void GpuChannelHost::TerminateGpuProcessForTesting() {
  GetGpuChannel().TerminateForTesting();
}

scoped_refptr<SharedImageInterface>
GpuChannelHost::CreateClientSharedImageInterface() {
  return base::MakeRefCounted<ClientSharedImageInterface>(
      &shared_image_interface_, this);
}

GpuChannelHost::~GpuChannelHost() = default;

GpuChannelHost::ConnectionTracker::ConnectionTracker() = default;

GpuChannelHost::ConnectionTracker::~ConnectionTracker() {
  CHECK(observer_list_.empty());
}

void GpuChannelHost::ConnectionTracker::OnDisconnectedFromGpuProcess() {
  is_connected_.store(false);
  NotifyGpuChannelLost();
}

bool GpuChannelHost::ConnectionTracker::AddObserverIfNotAlreadyLost(
    GpuChannelLostObserver* obs) {
  AutoLock lock(channel_obs_lock_);
  if (!is_connected()) {
    return false;
  }
  CHECK(!base::Contains(observer_list_, obs));
  observer_list_.push_back(obs);
  return true;
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
  channel_ = IPC::Channel::Create(std::move(handle), IPC::Channel::MODE_CLIENT,
                                  this, io_task_runner, io_task_runner);
  DCHECK(channel_);
  bool result = channel_->Connect();
  DCHECK(result);
  channel_->GetRemoteAssociatedInterface(std::move(receiver));
}

GpuChannelHost::Listener::~Listener() = default;

void GpuChannelHost::Listener::Close() {
  OnChannelError();
}

void GpuChannelHost::Listener::OnChannelError() {
  AutoLock lock(lock_);
  channel_ = nullptr;
}

bool GpuChannelHost::AddObserverIfNotAlreadyLost(GpuChannelLostObserver* obs) {
  return connection_tracker_->AddObserverIfNotAlreadyLost(obs);
}

void GpuChannelHost::RemoveObserver(GpuChannelLostObserver* obs) {
  connection_tracker_->RemoveObserver(obs);
}

}  // namespace gpu
