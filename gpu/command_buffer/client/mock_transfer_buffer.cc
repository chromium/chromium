// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/mock_transfer_buffer.h"

#include "gpu/command_buffer/common/command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

MockTransferBuffer::MockTransferBuffer(CommandBuffer* command_buffer,
                                       unsigned int size,
                                       unsigned int result_size,
                                       unsigned int alignment,
                                       bool initialize_fail)
    : command_buffer_(command_buffer),
      size_(size),
      result_size_(result_size),
      alignment_(alignment),
      actual_buffer_index_(0),
      expected_buffer_index_(0),
      last_alloc_(nullptr),
      expected_offset_(result_size),
      actual_offset_(result_size),
      initialize_fail_(initialize_fail) {
  // We have to allocate the buffers here because
  // we need to know their address before
  // {Raster,GLES2}Implementation::Initialize is called.
  for (int ii = 0; ii < kNumBuffers; ++ii) {
    buffers_[ii] = command_buffer_->CreateTransferBuffer(
        size_ + ii * alignment_, &buffer_ids_[ii]);
    EXPECT_NE(-1, buffer_ids_[ii]);
  }
}

MockTransferBuffer::~MockTransferBuffer() = default;

base::UnguessableToken MockTransferBuffer::shared_memory_guid() const {
  return base::UnguessableToken();
}

bool MockTransferBuffer::Initialize(unsigned int starting_buffer_size,
                                    unsigned int result_size,
                                    unsigned int /* min_buffer_size */,
                                    unsigned int /* max_buffer_size */,
                                    unsigned int alignment) {
  // Just check they match.
  return size_ == starting_buffer_size && result_size_ == result_size &&
         alignment_ == alignment && !initialize_fail_;
}

int MockTransferBuffer::GetShmId() {
  return buffer_ids_[actual_buffer_index_];
}

void* MockTransferBuffer::AcquireResultBuffer() {
  EXPECT_FALSE(outstanding_result_pointer_);
  outstanding_result_pointer_ = true;
  return actual_buffer() + actual_buffer_index_ * alignment_;
}

void MockTransferBuffer::ReleaseResultBuffer() {
  EXPECT_TRUE(outstanding_result_pointer_);
  outstanding_result_pointer_ = false;
}

int MockTransferBuffer::GetResultOffset() {
  return actual_buffer_index_ * alignment_;
}

void MockTransferBuffer::Free() {
  NOTREACHED();
}

bool MockTransferBuffer::HaveBuffer() const {
  return true;
}

void* MockTransferBuffer::AllocUpTo(unsigned int size,
                                    unsigned int* size_allocated) {
  EXPECT_TRUE(size_allocated != nullptr);
  EXPECT_TRUE(last_alloc_ == nullptr);

  // Toggle which buffer we get each time to simulate the buffer being
  // reallocated.
  actual_buffer_index_ = (actual_buffer_index_ + 1) % kNumBuffers;

  size = std::min(size, MaxTransferBufferSize());
  if (actual_offset_ + size > size_) {
    actual_offset_ = result_size_;
  }
  uint32_t offset = actual_offset_;
  actual_offset_ += RoundToAlignment(size);
  *size_allocated = size;

  // Make sure each buffer has a different offset.
  last_alloc_ = actual_buffer() + offset + actual_buffer_index_ * alignment_;
  return last_alloc_;
}

void* MockTransferBuffer::Alloc(unsigned int size) {
  EXPECT_LE(size, MaxTransferBufferSize());
  unsigned int temp = 0;
  void* p = AllocUpTo(size, &temp);
  EXPECT_EQ(temp, size);
  return p;
}

RingBuffer::Offset MockTransferBuffer::GetOffset(void* pointer) const {
  // Make sure each buffer has a different offset.
  return static_cast<uint8_t*>(pointer) - actual_buffer();
}

void MockTransferBuffer::DiscardBlock(void* p) {
  EXPECT_EQ(last_alloc_, p);
  last_alloc_ = nullptr;
}

void MockTransferBuffer::FreePendingToken(void* p, unsigned int /* token */) {
  EXPECT_EQ(last_alloc_, p);
  last_alloc_ = nullptr;
}

unsigned int MockTransferBuffer::GetSize() const {
  return 0;
}

unsigned int MockTransferBuffer::GetFreeSize() const {
  return 0;
}

unsigned int MockTransferBuffer::GetFragmentedFreeSize() const {
  return 0;
}

unsigned int MockTransferBuffer::GetMaxSize() const {
  return 0;
}

void MockTransferBuffer::ShrinkLastBlock(unsigned int new_size) {}

uint32_t MockTransferBuffer::MaxTransferBufferSize() {
  return size_ - result_size_;
}

unsigned int MockTransferBuffer::RoundToAlignment(unsigned int size) {
  return (size + alignment_ - 1) & ~(alignment_ - 1);
}

bool MockTransferBuffer::InSync() {
  return expected_buffer_index_ == actual_buffer_index_ &&
         expected_offset_ == actual_offset_;
}

MockTransferBuffer::ExpectedMemoryInfo MockTransferBuffer::GetExpectedMemory(
    uint32_t size) {
  ExpectedMemoryInfo mem;
  mem.offset = AllocateExpectedTransferBuffer(size);
  mem.id = GetExpectedTransferBufferId();
  mem.ptr = static_cast<uint8_t*>(
      GetExpectedTransferAddressFromOffset(mem.offset, size));
  return mem;
}

MockTransferBuffer::ExpectedMemoryInfo
MockTransferBuffer::GetExpectedResultMemory(uint32_t size) {
  ExpectedMemoryInfo mem;
  mem.offset = GetExpectedResultBufferOffset();
  mem.id = GetExpectedResultBufferId();
  mem.ptr = static_cast<uint8_t*>(
      GetExpectedTransferAddressFromOffset(mem.offset, size));
  return mem;
}

uint32_t MockTransferBuffer::AllocateExpectedTransferBuffer(uint32_t size) {
  EXPECT_LE(size, MaxTransferBufferSize());

  // Toggle which buffer we get each time to simulate the buffer being
  // reallocated.
  expected_buffer_index_ = (expected_buffer_index_ + 1) % kNumBuffers;

  if (expected_offset_ + size > size_) {
    expected_offset_ = result_size_;
  }
  uint32_t offset = expected_offset_;
  expected_offset_ += RoundToAlignment(size);

  // Make sure each buffer has a different offset.
  return offset + expected_buffer_index_ * alignment_;
}

void* MockTransferBuffer::GetExpectedTransferAddressFromOffset(uint32_t offset,
                                                               uint32_t size) {
  EXPECT_GE(offset, expected_buffer_index_ * alignment_);
  EXPECT_LE(offset + size, size_ + expected_buffer_index_ * alignment_);
  return expected_buffer() + offset;
}

int MockTransferBuffer::GetExpectedResultBufferId() {
  return buffer_ids_[expected_buffer_index_];
}

uint32_t MockTransferBuffer::GetExpectedResultBufferOffset() {
  return expected_buffer_index_ * alignment_;
}

int MockTransferBuffer::GetExpectedTransferBufferId() {
  return buffer_ids_[expected_buffer_index_];
}

}  // namespace gpu
