// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/gpu_channel_host.h"

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
<<<<<<< HEAD
#include "base/metrics/histogram_macros.h"
#include "base/record_replay.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
||||||| 80c960997e61f
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
=======
#include "base/task/single_thread_task_runner.h"
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_watchdog_timeout.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "url/gurl.h"

using base::AutoLock;

namespace gpu {

GpuChannelHost::GpuChannelHost(
    int channel_id,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    mojo::ScopedMessagePipeHandle handle,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_thread_(io_task_runner ? io_task_runner
                                : base::ThreadTaskRunnerHandle::Get()),
      channel_id_(channel_id),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      listener_(new Listener(), base::OnTaskRunnerDeleter(io_thread_)),
      connection_tracker_(base::MakeRefCounted<ConnectionTracker>()),
      shared_image_interface_(
          this,
          static_cast<int32_t>(
              GpuChannelReservedRoutes::kSharedImageInterface)),
      image_decode_accelerator_proxy_(
          this,
          static_cast<int32_t>(
<<<<<<< HEAD
              GpuChannelReservedRoutes::kImageDecodeAccelerator)),
      context_lock_("GpuChannelHost.context_lock_") {
||||||| 80c960997e61f
              GpuChannelReservedRoutes::kImageDecodeAccelerator)) {
=======
              GpuChannelReservedRoutes::kImageDecodeAccelerator)) {
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

>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
  next_image_id_.GetNext();
  for (int32_t i = 0;
       i <= static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue); ++i)
    next_route_id_.GetNext();

#if BUILDFLAG(IS_MAC)
  gpu::SetMacOSSpecificTextureTarget(gpu_info.macos_specific_texture_target);
#endif  // BUILDFLAG(IS_MAC)
}

<<<<<<< HEAD
bool GpuChannelHost::Send(IPC::Message* msg) {
  // The sync wait below currently can prevent recordings from being finished
  // on the main thread.
  if (recordreplay::IsRecordingOrReplaying()) {
    fprintf(stderr, "Warning: Sending GPU message while recording.\n");
  }

  TRACE_IPC_MESSAGE_SEND("ipc", "GpuChannelHost::Send", msg);

  auto message = base::WrapUnique(msg);

  DCHECK(!io_thread_->BelongsToCurrentThread());

  // The GPU process never sends synchronous IPCs so clear the unblock flag to
  // preserve order.
  message->set_unblock(false);

  if (!message->is_sync()) {
    io_thread_->PostTask(FROM_HERE,
                         base::BindOnce(&Listener::SendMessage,
                                        base::Unretained(listener_.get()),
                                        std::move(message), nullptr));
    return true;
  }

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto deserializer = base::WrapUnique(
      static_cast<IPC::SyncMessage*>(message.get())->GetReplyDeserializer());

  IPC::PendingSyncMsg pending_sync(IPC::SyncMessage::GetMessageId(*message),
                                   deserializer.get(), &done_event);
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&Listener::SendMessage, base::Unretained(listener_.get()),
                     std::move(message), &pending_sync));
  base::TimeTicks start_time = base::TimeTicks::Now();

  // http://crbug.com/125264
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;

  pending_sync.done_event->Wait();

  // Histogram to measure how long the browser UI thread spends blocked.
  // Recorded only for users with high-resolution clocks.
  base::TimeDelta wait_duration = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("GPU.GPUChannelHostWaitTime3",
                                          wait_duration,
                                          base::TimeDelta::FromMicroseconds(5),
                                          base::TimeDelta::FromSeconds(1), 50);

  return pending_sync.send_result;
||||||| 80c960997e61f
bool GpuChannelHost::Send(IPC::Message* msg) {
  TRACE_IPC_MESSAGE_SEND("ipc", "GpuChannelHost::Send", msg);

  auto message = base::WrapUnique(msg);

  DCHECK(!io_thread_->BelongsToCurrentThread());

  // The GPU process never sends synchronous IPCs so clear the unblock flag to
  // preserve order.
  message->set_unblock(false);

  if (!message->is_sync()) {
    io_thread_->PostTask(FROM_HERE,
                         base::BindOnce(&Listener::SendMessage,
                                        base::Unretained(listener_.get()),
                                        std::move(message), nullptr));
    return true;
  }

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto deserializer = base::WrapUnique(
      static_cast<IPC::SyncMessage*>(message.get())->GetReplyDeserializer());

  IPC::PendingSyncMsg pending_sync(IPC::SyncMessage::GetMessageId(*message),
                                   deserializer.get(), &done_event);
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&Listener::SendMessage, base::Unretained(listener_.get()),
                     std::move(message), &pending_sync));
  base::TimeTicks start_time = base::TimeTicks::Now();

  // http://crbug.com/125264
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;

  pending_sync.done_event->Wait();

  // Histogram to measure how long the browser UI thread spends blocked.
  // Recorded only for users with high-resolution clocks.
  base::TimeDelta wait_duration = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("GPU.GPUChannelHostWaitTime3",
                                          wait_duration,
                                          base::TimeDelta::FromMicroseconds(5),
                                          base::TimeDelta::FromSeconds(1), 50);

  return pending_sync.send_result;
=======
mojom::GpuChannel& GpuChannelHost::GetGpuChannel() {
  return *gpu_channel_.get();
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
}

uint32_t GpuChannelHost::OrderingBarrier(
    int32_t route_id,
    int32_t put_offset,
    std::vector<SyncToken> sync_token_fences) {
  AutoLock lock(context_lock_);

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
  return pending_ordering_barrier_->deferred_message_id;
}

uint32_t GpuChannelHost::EnqueueDeferredMessage(
    mojom::DeferredRequestParamsPtr params,
    std::vector<SyncToken> sync_token_fences) {
  AutoLock lock(context_lock_);

  EnqueuePendingOrderingBarrier();
  enqueued_deferred_message_id_ = next_deferred_message_id_++;
  deferred_messages_.push_back(mojom::DeferredRequest::New(
      std::move(params), std::move(sync_token_fences)));
  return enqueued_deferred_message_id_;
}

void GpuChannelHost::EnsureFlush(uint32_t deferred_message_id) {
  AutoLock lock(context_lock_);
  InternalFlush(deferred_message_id);
}

void GpuChannelHost::VerifyFlush(uint32_t deferred_message_id) {
  AutoLock lock(context_lock_);

  InternalFlush(deferred_message_id);

  if (deferred_message_id > verified_deferred_message_id_) {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
    GetGpuChannel().Flush();
    verified_deferred_message_id_ = flushed_deferred_message_id_;
  }
}

void GpuChannelHost::EnqueuePendingOrderingBarrier() {
  context_lock_.AssertAcquired();
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
      std::move(pending_ordering_barrier_->sync_token_fences)));
  pending_ordering_barrier_.reset();
}

void GpuChannelHost::InternalFlush(uint32_t deferred_message_id) {
  context_lock_.AssertAcquired();

  EnqueuePendingOrderingBarrier();
  if (!deferred_messages_.empty() &&
      deferred_message_id > flushed_deferred_message_id_) {
    DCHECK_EQ(enqueued_deferred_message_id_, next_deferred_message_id_ - 1);

    GetGpuChannel().FlushDeferredRequests(std::move(deferred_messages_));
    deferred_messages_.clear();
    flushed_deferred_message_id_ = next_deferred_message_id_ - 1;
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

void GpuChannelHost::CrashGpuProcessForTesting() {
  GetGpuChannel().CrashForTesting();
}

void GpuChannelHost::TerminateGpuProcessForTesting() {
  GetGpuChannel().TerminateForTesting();
}

std::unique_ptr<ClientSharedImageInterface>
GpuChannelHost::CreateClientSharedImageInterface() {
  return std::make_unique<ClientSharedImageInterface>(&shared_image_interface_);
}

GpuChannelHost::~GpuChannelHost() = default;

GpuChannelHost::ConnectionTracker::ConnectionTracker() = default;

GpuChannelHost::ConnectionTracker::~ConnectionTracker() = default;

void GpuChannelHost::ConnectionTracker::OnDisconnectedFromGpuProcess() {
  is_connected_.store(false);
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
  channel_ = IPC::ChannelMojo::Create(
      std::move(handle), IPC::Channel::MODE_CLIENT, this, io_task_runner,
      io_task_runner, mojo::internal::MessageQuotaChecker::MaybeCreate());
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

}  // namespace gpu
