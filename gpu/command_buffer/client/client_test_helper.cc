// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Tests for GLES2Implementation.

#include "gpu/command_buffer/client/client_test_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;

namespace gpu {

FakeCommandBufferServiceBase::FakeCommandBufferServiceBase() = default;

FakeCommandBufferServiceBase::~FakeCommandBufferServiceBase() = default;

CommandBuffer::State FakeCommandBufferServiceBase::GetState() {
  return state_;
}

void FakeCommandBufferServiceBase::SetReleaseCount(uint64_t release_count) {
  state_.release_count = release_count;
}

// Get's the Id of the next transfer buffer that will be returned
// by CreateTransferBuffer. This is useful for testing expected ids.
int32_t FakeCommandBufferServiceBase::GetNextFreeTransferBufferId() {
  for (int32_t ii = 0; ii < kMaxTransferBuffers; ++ii) {
    if (!transfer_buffer_buffers_[ii].get()) {
      return kTransferBufferBaseId + ii;
    }
  }
  return -1;
}

void FakeCommandBufferServiceBase::SetGetBufferHelper(int transfer_buffer_id,
                                                      int32_t token) {
  ++state_.set_get_buffer_count;
  state_.get_offset = 0;
  state_.token = token;
}

scoped_refptr<gpu::Buffer>
FakeCommandBufferServiceBase::CreateTransferBufferHelper(uint32_t size,
                                                         int32_t* id) {
  *id = GetNextFreeTransferBufferId();
  if (*id >= 0) {
    int32_t ndx = *id - kTransferBufferBaseId;
    base::UnsafeSharedMemoryRegion shared_memory_region =
        base::UnsafeSharedMemoryRegion::Create(size);
    base::WritableSharedMemoryMapping shared_memory_mapping =
        shared_memory_region.Map();
    transfer_buffer_buffers_[ndx] = MakeBufferFromSharedMemory(
        std::move(shared_memory_region), std::move(shared_memory_mapping));
  }
  return GetTransferBuffer(*id);
}

void FakeCommandBufferServiceBase::DestroyTransferBufferHelper(int32_t id) {
  DCHECK_GE(id, kTransferBufferBaseId);
  DCHECK_LT(id, kTransferBufferBaseId + kMaxTransferBuffers);
  id -= kTransferBufferBaseId;
  transfer_buffer_buffers_[id] = nullptr;
}

scoped_refptr<Buffer> FakeCommandBufferServiceBase::GetTransferBuffer(
    int32_t id) {
  if ((id < kTransferBufferBaseId) ||
      (id >= kTransferBufferBaseId + kMaxTransferBuffers))
    return nullptr;
  return transfer_buffer_buffers_[id - kTransferBufferBaseId];
}

void FakeCommandBufferServiceBase::FlushHelper(int32_t put_offset) {
  state_.get_offset = put_offset;
}

void FakeCommandBufferServiceBase::SetToken(int32_t token) {
  state_.token = token;
}

void FakeCommandBufferServiceBase::SetParseError(error::Error error) {
  state_.error = error;
}

void FakeCommandBufferServiceBase::SetContextLostReason(
    error::ContextLostReason reason) {
  state_.context_lost_reason = reason;
}

const int32_t FakeCommandBufferServiceBase::kTransferBufferBaseId;
const int32_t FakeCommandBufferServiceBase::kMaxTransferBuffers;

MockClientCommandBuffer::MockClientCommandBuffer() {
  DelegateToFake();
}

MockClientCommandBuffer::~MockClientCommandBuffer() = default;

CommandBuffer::State MockClientCommandBuffer::GetLastState() {
  return GetState();
}

CommandBuffer::State MockClientCommandBuffer::WaitForTokenInRange(int32_t start,
                                                                  int32_t end) {
  return GetState();
}

CommandBuffer::State MockClientCommandBuffer::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  State state = GetState();
  EXPECT_EQ(set_get_buffer_count, state.set_get_buffer_count);
  if (state.get_offset != put_offset_) {
    FlushHelper(put_offset_);
    OnFlush();
    state = GetState();
  }
  return state;
}

void MockClientCommandBuffer::SetGetBuffer(int transfer_buffer_id) {
  SetGetBufferHelper(transfer_buffer_id, token_);
}

scoped_refptr<gpu::Buffer> MockClientCommandBuffer::CreateTransferBuffer(
    uint32_t size,
    int32_t* id,
    uint32_t alignment,
    TransferBufferAllocationOption option) {
  return CreateTransferBufferHelper(size, id);
}

void MockClientCommandBuffer::Flush(int32_t put_offset) {
  put_offset_ = put_offset;
}

void MockClientCommandBuffer::OrderingBarrier(int32_t put_offset) {
  put_offset_ = put_offset;
}

void MockClientCommandBuffer::DelegateToFake() {
  ON_CALL(*this, DestroyTransferBuffer(_))
      .WillByDefault(Invoke(
          this, &FakeCommandBufferServiceBase::DestroyTransferBufferHelper));
}

void MockClientCommandBuffer::ForceLostContext(
    error::ContextLostReason reason) {
  // TODO(kbr): add a test for a call to this method.
  SetParseError(error::kLostContext);
  SetContextLostReason(reason);
}

MockClientCommandBufferMockFlush::MockClientCommandBufferMockFlush() {
  DelegateToFake();
}

MockClientCommandBufferMockFlush::~MockClientCommandBufferMockFlush() = default;

void MockClientCommandBufferMockFlush::DelegateToFake() {
  MockClientCommandBuffer::DelegateToFake();
  ON_CALL(*this, Flush(_))
      .WillByDefault(Invoke(this, &MockClientCommandBufferMockFlush::DoFlush));
}

void MockClientCommandBufferMockFlush::DoFlush(int32_t put_offset) {
  MockClientCommandBuffer::Flush(put_offset);
}

MockClientGpuControl::MockClientGpuControl() = default;

MockClientGpuControl::~MockClientGpuControl() = default;

FakeDecoderClient::~FakeDecoderClient() = default;
void FakeDecoderClient::OnConsoleMessage(int32_t, const std::string&) {}
void FakeDecoderClient::CacheBlob(gpu::GpuDiskCacheType,
                                  const std::string&,
                                  const std::string&) {}
void FakeDecoderClient::OnFenceSyncRelease(uint64_t) {}
void FakeDecoderClient::OnDescheduleUntilFinished() {}
void FakeDecoderClient::OnRescheduleAfterFinished() {}
void FakeDecoderClient::OnSwapBuffers(uint64_t, uint32_t) {}
void FakeDecoderClient::ScheduleGrContextCleanup() {}
void FakeDecoderClient::SetActiveURL(GURL) {}
void FakeDecoderClient::HandleReturnData(base::span<const uint8_t>) {}
bool FakeDecoderClient::ShouldYield() {
  return false;
}

}  // namespace gpu
