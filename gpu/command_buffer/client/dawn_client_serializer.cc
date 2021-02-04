// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/dawn_client_serializer.h"

#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"

namespace gpu {
namespace webgpu {

// static
std::unique_ptr<DawnClientSerializer> DawnClientSerializer::Create(
    WebGPUImplementation* client,
    WebGPUCmdHelper* helper,
    DawnClientMemoryTransferService* memory_transfer_service,
    const SharedMemoryLimits& limits) {
  std::unique_ptr<TransferBuffer> transfer_buffer =
      std::make_unique<TransferBuffer>(helper);
  if (!transfer_buffer->Initialize(limits.start_transfer_buffer_size,
                                   /* start offset */ 0,
                                   limits.min_transfer_buffer_size,
                                   limits.max_transfer_buffer_size,
                                   /* alignment */ 8)) {
    return nullptr;
  }
  return std::make_unique<DawnClientSerializer>(
      client, helper, memory_transfer_service, std::move(transfer_buffer),
      limits.start_transfer_buffer_size);
}

DawnClientSerializer::DawnClientSerializer(
    WebGPUImplementation* client,
    WebGPUCmdHelper* helper,
    DawnClientMemoryTransferService* memory_transfer_service_,
    std::unique_ptr<TransferBuffer> transfer_buffer,
    uint32_t buffer_initial_size)
    : client_(client),
      helper_(helper),
      memory_transfer_service_(memory_transfer_service_),
      transfer_buffer_(std::move(transfer_buffer)),
      buffer_initial_size_(buffer_initial_size),
      buffer_(helper_, transfer_buffer_.get()) {
  DCHECK_GT(buffer_initial_size_, 0u);
}

DawnClientSerializer::~DawnClientSerializer() = default;

size_t DawnClientSerializer::GetMaximumAllocationSize() const {
  return transfer_buffer_->GetMaxSize();
}

void* DawnClientSerializer::GetCmdSpace(size_t size) {
  // Note: Dawn will never call this function with |size| >
  // GetMaximumAllocationSize().
  DCHECK_LE(size, GetMaximumAllocationSize());

  // The buffer size must be initialized before any commands are serialized.
  DCHECK_NE(buffer_initial_size_, 0u);

  DCHECK_LE(put_offset_, buffer_.size());
  const bool overflows_remaining_space =
      size > static_cast<size_t>(buffer_.size() - put_offset_);

  if (LIKELY(buffer_.valid() && !overflows_remaining_space)) {
    // If the buffer is valid and has sufficient space, return the
    // pointer and increment the offset.
    uint8_t* ptr = static_cast<uint8_t*>(buffer_.address());
    ptr += put_offset_;

    put_offset_ += static_cast<uint32_t>(size);
    return ptr;
  }

  if (!transfer_buffer_) {
    // The serializer hit a fatal error and was disconnected.
    return nullptr;
  }

  // Otherwise, flush and reset the command stream.
  Flush();

  uint32_t allocation_size =
      std::max(buffer_initial_size_, static_cast<uint32_t>(size));
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "DawnClientSerializer::GetCmdSpace", "bytes", allocation_size);
  buffer_.Reset(allocation_size);

  if (!buffer_.valid() || buffer_.size() < size) {
    DLOG(ERROR) << "Dawn wire transfer buffer allocation failed";
    Disconnect();
    client_->OnGpuControlLostContextMaybeReentrant();
    return nullptr;
  }

  put_offset_ = size;
  return buffer_.address();
}

bool DawnClientSerializer::Flush() {
  if (buffer_.valid()) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "DawnClientSerializer::Flush", "bytes", put_offset_);

    TRACE_EVENT_WITH_FLOW0(
        TRACE_DISABLED_BY_DEFAULT("gpu.dawn"), "DawnCommands",
        TRACE_EVENT_FLAG_FLOW_OUT,
        (static_cast<uint64_t>(buffer_.shm_id()) << 32) + buffer_.offset());

    buffer_.Shrink(put_offset_);
    helper_->DawnCommands(buffer_.shm_id(), buffer_.offset(), put_offset_);
    put_offset_ = 0;
    buffer_.Release();
    awaiting_flush_ = false;

    memory_transfer_service_->FreeHandles(helper_);
  }
  return true;
}

void DawnClientSerializer::SetAwaitingFlush(bool awaiting_flush) {
  // If awaiting_flush is true, but the buffer_ is invalid (empty), that
  // means the last command right before this caused a flush. Another flush is
  // not needed.
  awaiting_flush_ = awaiting_flush && buffer_.valid();
}

void DawnClientSerializer::Disconnect() {
  buffer_.Discard();
  transfer_buffer_ = nullptr;
}

}  // namespace webgpu
}  // namespace gpu
