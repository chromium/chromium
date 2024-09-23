// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/dawn_client_serializer.h"

#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"

namespace gpu {
namespace webgpu {

DawnClientSerializer::DawnClientSerializer(
    WebGPUImplementation* client,
    WebGPUCmdHelper* helper,
    DawnClientMemoryTransferService* memory_transfer_service_,
    std::unique_ptr<TransferBuffer> transfer_buffer)
    : client_(client),
      helper_(helper),
      memory_transfer_service_(memory_transfer_service_),
      transfer_buffer_(std::move(transfer_buffer)),
      buffer_initial_size_(transfer_buffer_->GetSize()),
      buffer_(helper_, transfer_buffer_.get()) {
  DCHECK_GT(buffer_initial_size_, 0u);
}

DawnClientSerializer::~DawnClientSerializer() = default;

size_t DawnClientSerializer::GetMaximumAllocationSize() const {
  return transfer_buffer_->GetMaxSize();
}

#if DCHECK_IS_ON()
void DawnClientSerializer::OnSerializeError() {
  NOTREACHED_IN_MIGRATION() << "DawnClientSerializer error";
}
#endif

void* DawnClientSerializer::GetCmdSpace(size_t size) {
  // Note: Dawn will never call this function with |size| >
  // GetMaximumAllocationSize().
  DCHECK_LE(size, GetMaximumAllocationSize());

  // The buffer size must be initialized before any commands are serialized.
  DCHECK_NE(buffer_initial_size_, 0u);

  DCHECK_LE(put_offset_, buffer_.size());
  const bool overflows_remaining_space =
      size > static_cast<size_t>(buffer_.size() - put_offset_);

  if (buffer_.valid() && !overflows_remaining_space) [[likely]] {
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

void DawnClientSerializer::Commit() {
  if (buffer_.valid()) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "DawnClientSerializer::Flush", "bytes", put_offset_);

    bool is_tracing = false;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                                       &is_tracing);
    uint64_t trace_id;
    if (is_tracing) {
      trace_id = base::RandUint64();
      TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                             "DawnCommands", trace_id,
                             TRACE_EVENT_FLAG_FLOW_OUT);
    } else {
      trace_id = 0;
    }

    buffer_.Shrink(put_offset_);
    helper_->DawnCommands(trace_id >> 32, trace_id & 0xFFFF'FFFF,
                          buffer_.shm_id(), buffer_.offset(), put_offset_);
    put_offset_ = 0;
    buffer_.Release();

    memory_transfer_service_->FreeHandles(helper_);
  }
}

void DawnClientSerializer::SetAwaitingFlush(bool awaiting_flush) {
  // Set awaiting_flush_. Even if there are no commands in buffer_, this may be
  // necessary since the buffer_ commands could have been committed and reset,
  // but not yet flushed.
  awaiting_flush_ = awaiting_flush;
}

void DawnClientSerializer::Disconnect() {
  buffer_.Discard();
  if (transfer_buffer_) {
    auto transfer_buffer = std::move(transfer_buffer_);
    // Wait for commands to finish before we free shared memory that
    // the GPU process is using.
    // TODO(crbug.com/40779774): This Finish may not be necessary if the
    // shared memory is not immediately freed. Investigate this and
    // consider optimization.
    helper_->Finish();
    transfer_buffer = nullptr;
  }
}

bool DawnClientSerializer::Flush() {
  Commit();
  return true;
}

}  // namespace webgpu
}  // namespace gpu
