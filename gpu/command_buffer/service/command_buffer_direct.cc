// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_direct.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"

namespace gpu {

CommandBufferDirect::CommandBufferDirect() : service_(this, nullptr) {}

CommandBufferDirect::~CommandBufferDirect() = default;

CommandBuffer::State CommandBufferDirect::GetLastState() {
  service_.UpdateState();
  return service_.GetState();
}

CommandBuffer::State CommandBufferDirect::WaitForTokenInRange(int32_t start,
                                                              int32_t end) {
  State state = GetLastState();
  DCHECK(state.error != error::kNoError || InRange(start, end, state.token));
  return state;
}

CommandBuffer::State CommandBufferDirect::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  State state = GetLastState();
  DCHECK(state.error != error::kNoError ||
         (InRange(start, end, state.get_offset) &&
          (set_get_buffer_count == state.set_get_buffer_count)));
  return state;
}

void CommandBufferDirect::Flush(int32_t put_offset) {
  DCHECK(handler_);
  if (GetLastState().error != gpu::error::kNoError)
    return;
  service_.Flush(put_offset, handler_);
}

void CommandBufferDirect::OrderingBarrier(int32_t put_offset) {
  Flush(put_offset);
}

void CommandBufferDirect::SetGetBuffer(int32_t transfer_buffer_id) {
  service_.SetGetBuffer(transfer_buffer_id);
}

scoped_refptr<Buffer> CommandBufferDirect::CreateTransferBuffer(
    uint32_t size,
    int32_t* id,
    uint32_t alignment,
    TransferBufferAllocationOption option) {
  return service_.CreateTransferBuffer(size, id, alignment);
}

void CommandBufferDirect::DestroyTransferBuffer(int32_t id) {
  service_.DestroyTransferBuffer(id);
}

void CommandBufferDirect::ForceLostContext(error::ContextLostReason reason) {
  service_.SetContextLostReason(reason);
  service_.SetParseError(error::kLostContext);
}

CommandBufferServiceClient::CommandBatchProcessedResult
CommandBufferDirect::OnCommandBatchProcessed() {
  return kContinueExecution;
}

void CommandBufferDirect::OnParseError() {}

void CommandBufferDirect::OnConsoleMessage(int32_t id,
                                           const std::string& message) {}

void CommandBufferDirect::CacheBlob(gpu::GpuDiskCacheType type,
                                    const std::string& key,
                                    const std::string& blob) {}

void CommandBufferDirect::OnFenceSyncRelease(uint64_t release) {
  NOTIMPLEMENTED();
}

void CommandBufferDirect::OnDescheduleUntilFinished() {
  service_.SetScheduled(false);
}

void CommandBufferDirect::OnRescheduleAfterFinished() {
  service_.SetScheduled(true);
}

void CommandBufferDirect::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}

scoped_refptr<Buffer> CommandBufferDirect::CreateTransferBufferWithId(
    uint32_t size,
    int32_t id) {
  return service_.CreateTransferBufferWithId(size, id);
}

void CommandBufferDirect::HandleReturnData(base::span<const uint8_t> data) {
  NOTIMPLEMENTED();
}

bool CommandBufferDirect::ShouldYield() {
  return service_.ShouldYield();
}

}  // namespace gpu
