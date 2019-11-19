// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_command_buffer_proxy.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

PpapiCommandBufferProxy::PpapiCommandBufferProxy(
    const ppapi::HostResource& resource,
    InstanceData::FlushInfo* flush_info,
    LockedSender* sender,
    const gpu::Capabilities& capabilities,
    SerializedHandle shared_state,
    gpu::CommandBufferId command_buffer_id)
    : command_buffer_id_(command_buffer_id),
      capabilities_(capabilities),
      resource_(resource),
      flush_info_(flush_info),
      sender_(sender),
      next_fence_sync_release_(1),
      pending_fence_sync_release_(0),
      flushed_fence_sync_release_(0),
      validated_fence_sync_release_(0) {
  base::UnsafeSharedMemoryRegion shmem_region =
      base::UnsafeSharedMemoryRegion::Deserialize(
          shared_state.TakeSharedMemoryRegion());
  shared_state_mapping_ = shmem_region.Map();
}

PpapiCommandBufferProxy::~PpapiCommandBufferProxy() {
  // gpu::Buffers are no longer referenced, allowing shared memory objects to be
  // deleted, closing the handle in this process.
}

gpu::CommandBuffer::State PpapiCommandBufferProxy::GetLastState() {
  ppapi::ProxyLock::AssertAcquiredDebugOnly();
  TryUpdateState();
  return last_state_;
}

void PpapiCommandBufferProxy::Flush(int32_t put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  OrderingBarrier(put_offset);
  FlushInternal();
}

void PpapiCommandBufferProxy::OrderingBarrier(int32_t put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  if (flush_info_->flush_pending && flush_info_->resource != resource_) {
    FlushInternal();
  }

  flush_info_->flush_pending = true;
  flush_info_->resource = resource_;
  flush_info_->put_offset = put_offset;
  pending_fence_sync_release_ = next_fence_sync_release_ - 1;
}

gpu::CommandBuffer::State PpapiCommandBufferProxy::WaitForTokenInRange(
    int32_t start,
    int32_t end) {
  TryUpdateState();
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    bool success = false;
    gpu::CommandBuffer::State state;
    if (Send(new PpapiHostMsg_PPBGraphics3D_WaitForTokenInRange(
            ppapi::API_ID_PPB_GRAPHICS_3D, resource_, start, end, &state,
            &success)))
      UpdateState(state, success);
  }
  DCHECK(InRange(start, end, last_state_.token) ||
         last_state_.error != gpu::error::kNoError);
  return last_state_;
}

gpu::CommandBuffer::State PpapiCommandBufferProxy::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  TryUpdateState();
  if (((set_get_buffer_count != last_state_.set_get_buffer_count) ||
       !InRange(start, end, last_state_.get_offset)) &&
      last_state_.error == gpu::error::kNoError) {
    bool success = false;
    gpu::CommandBuffer::State state;
    if (Send(new PpapiHostMsg_PPBGraphics3D_WaitForGetOffsetInRange(
            ppapi::API_ID_PPB_GRAPHICS_3D, resource_, set_get_buffer_count,
            start, end, &state, &success)))
      UpdateState(state, success);
  }
  DCHECK(((set_get_buffer_count == last_state_.set_get_buffer_count) &&
          InRange(start, end, last_state_.get_offset)) ||
         last_state_.error != gpu::error::kNoError);
  return last_state_;
}

void PpapiCommandBufferProxy::SetGetBuffer(int32_t transfer_buffer_id) {
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBGraphics3D_SetGetBuffer(
        ppapi::API_ID_PPB_GRAPHICS_3D, resource_, transfer_buffer_id));
  }
}

scoped_refptr<gpu::Buffer> PpapiCommandBufferProxy::CreateTransferBuffer(
    uint32_t size,
    int32_t* id) {
  *id = -1;

  if (last_state_.error != gpu::error::kNoError)
    return nullptr;

  // Assuming we are in the renderer process, the service is responsible for
  // duplicating the handle. This might not be true for NaCl.
  ppapi::proxy::SerializedHandle handle(
      ppapi::proxy::SerializedHandle::SHARED_MEMORY_REGION);
  if (!Send(new PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer(
          ppapi::API_ID_PPB_GRAPHICS_3D, resource_, size, id, &handle))) {
    if (last_state_.error == gpu::error::kNoError)
      last_state_.error = gpu::error::kLostContext;
    return nullptr;
  }

  if (*id <= 0 || !handle.is_shmem_region()) {
    if (last_state_.error == gpu::error::kNoError)
      last_state_.error = gpu::error::kOutOfBounds;
    return nullptr;
  }

  base::UnsafeSharedMemoryRegion shared_memory_region =
      base::UnsafeSharedMemoryRegion::Deserialize(
          handle.TakeSharedMemoryRegion());

  base::WritableSharedMemoryMapping shared_memory_mapping =
      shared_memory_region.Map();
  if (!shared_memory_mapping.IsValid() ||
      (shared_memory_mapping.size() > UINT32_MAX)) {
    if (last_state_.error == gpu::error::kNoError)
      last_state_.error = gpu::error::kOutOfBounds;
    *id = -1;
    return nullptr;
  }

  return gpu::MakeBufferFromSharedMemory(std::move(shared_memory_region),
                                         std::move(shared_memory_mapping));
}

void PpapiCommandBufferProxy::DestroyTransferBuffer(int32_t id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  if (flush_info_->flush_pending)
    FlushInternal();

  Send(new PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_, id));
}

void PpapiCommandBufferProxy::SetLock(base::Lock*) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::EnsureWorkVisible() {
  if (last_state_.error != gpu::error::kNoError)
    return;

  if (flush_info_->flush_pending)
    FlushInternal();
  DCHECK_GE(flushed_fence_sync_release_, validated_fence_sync_release_);
  Send(new PpapiHostMsg_PPBGraphics3D_EnsureWorkVisible(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_));
  validated_fence_sync_release_ = flushed_fence_sync_release_;
}

gpu::CommandBufferNamespace PpapiCommandBufferProxy::GetNamespaceID() const {
  return gpu::CommandBufferNamespace::GPU_IO;
}

gpu::CommandBufferId PpapiCommandBufferProxy::GetCommandBufferID() const {
  return command_buffer_id_;
}

void PpapiCommandBufferProxy::FlushPendingWork() {
  if (last_state_.error != gpu::error::kNoError)
    return;
  if (flush_info_->flush_pending)
    FlushInternal();
}

uint64_t PpapiCommandBufferProxy::GenerateFenceSyncRelease() {
  return next_fence_sync_release_++;
}

bool PpapiCommandBufferProxy::IsFenceSyncReleased(uint64_t release) {
  NOTREACHED();
  return false;
}

void PpapiCommandBufferProxy::SignalSyncToken(const gpu::SyncToken& sync_token,
                                              base::OnceClosure callback) {
  NOTREACHED();
}

// Pepper plugin does not expose or call WaitSyncTokenCHROMIUM.
void PpapiCommandBufferProxy::WaitSyncToken(const gpu::SyncToken& sync_token) {
  NOTREACHED();
}

bool PpapiCommandBufferProxy::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken& sync_token) {
  NOTREACHED();
  return false;
}

void PpapiCommandBufferProxy::SetDisplayTransform(
    gfx::OverlayTransform transform) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::SignalQuery(uint32_t query,
                                          base::OnceClosure callback) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::CreateGpuFence(uint32_t gpu_fence_id,
                                             ClientGpuFence source) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::SetGpuControlClient(gpu::GpuControlClient*) {
  // TODO(piman): The lost context callback skips past here and goes directly
  // to the plugin instance. Make it more uniform and use the GpuControlClient.
}

const gpu::Capabilities& PpapiCommandBufferProxy::GetCapabilities() const {
  return capabilities_;
}

int32_t PpapiCommandBufferProxy::CreateImage(ClientBuffer buffer,
                                             size_t width,
                                             size_t height) {
  NOTREACHED();
  return -1;
}

void PpapiCommandBufferProxy::DestroyImage(int32_t id) {
  NOTREACHED();
}

bool PpapiCommandBufferProxy::Send(IPC::Message* msg) {
  DCHECK(last_state_.error == gpu::error::kNoError);

  // We need to hold the Pepper proxy lock for sync IPC, because the GPU command
  // buffer may use a sync IPC with another lock held which could lead to lock
  // and deadlock if we dropped the proxy lock here.
  // http://crbug.com/418651
  if (sender_->SendAndStayLocked(msg))
    return true;

  last_state_.error = gpu::error::kLostContext;
  return false;
}

void PpapiCommandBufferProxy::UpdateState(
    const gpu::CommandBuffer::State& state,
    bool success) {
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (success) {
    if (state.generation - last_state_.generation < 0x80000000U) {
      last_state_ = state;
    }
  } else {
    last_state_.error = gpu::error::kLostContext;
    ++last_state_.generation;
  }
}

void PpapiCommandBufferProxy::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

gpu::CommandBufferSharedState* PpapiCommandBufferProxy::shared_state() const {
  return reinterpret_cast<gpu::CommandBufferSharedState*>(
      shared_state_mapping_.memory());
}

void PpapiCommandBufferProxy::FlushInternal() {
  DCHECK(last_state_.error == gpu::error::kNoError);

  DCHECK(flush_info_->flush_pending);
  DCHECK_GE(pending_fence_sync_release_, flushed_fence_sync_release_);

  IPC::Message* message = new PpapiHostMsg_PPBGraphics3D_AsyncFlush(
      ppapi::API_ID_PPB_GRAPHICS_3D, flush_info_->resource,
      flush_info_->put_offset);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);
  Send(message);

  flush_info_->flush_pending = false;
  flush_info_->resource.SetHostResource(0, 0);
  flushed_fence_sync_release_ = pending_fence_sync_release_;
}

}  // namespace proxy
}  // namespace ppapi
