// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the BufferTracker.

#include "gpu/command_buffer/client/buffer_tracker.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class MockClientCommandBufferImpl : public MockClientCommandBuffer {
 public:
  MockClientCommandBufferImpl()
      : MockClientCommandBuffer(),
        context_lost_(false) {}
  ~MockClientCommandBufferImpl() override = default;

  scoped_refptr<gpu::Buffer> CreateTransferBuffer(
      uint32_t size,
      int32_t* id,
      uint32_t alignment = 0,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM) override {
    if (context_lost_) {
      *id = -1;
      return nullptr;
    }
    return MockClientCommandBuffer::CreateTransferBuffer(size, id, alignment);
  }

  void set_context_lost(bool context_lost) {
    context_lost_ = context_lost;
  }

 private:
  bool context_lost_;
};

class BufferTrackerTest : public testing::Test {
 protected:
  static const int32_t kNumCommandEntries = 400;
  static const int32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);

  void SetUp() override {
    command_buffer_ = std::make_unique<MockClientCommandBufferImpl>();
    helper_ = std::make_unique<GLES2CmdHelper>(command_buffer_.get());
    helper_->Initialize(kCommandBufferSizeBytes);
    mapped_memory_ = std::make_unique<MappedMemoryManager>(
        helper_.get(), MappedMemoryManager::kNoLimit);
    buffer_tracker_ = std::make_unique<BufferTracker>(mapped_memory_.get());
  }

  void TearDown() override {
    buffer_tracker_.reset();
    mapped_memory_.reset();
    helper_.reset();
    command_buffer_.reset();
  }

  std::unique_ptr<MockClientCommandBufferImpl> command_buffer_;
  std::unique_ptr<GLES2CmdHelper> helper_;
  std::unique_ptr<MappedMemoryManager> mapped_memory_;
  std::unique_ptr<BufferTracker> buffer_tracker_;
};

TEST_F(BufferTrackerTest, Basic) {
  const GLuint kId1 = 123;
  const GLuint kId2 = 124;
  const GLsizeiptr size = 64;

  // Check we can create a Buffer.
  BufferTracker::Buffer* buffer = buffer_tracker_->CreateBuffer(kId1, size);
  ASSERT_TRUE(buffer != nullptr);
  // Check we can get the same Buffer.
  EXPECT_EQ(buffer, buffer_tracker_->GetBuffer(kId1));
  // Check mapped memory address.
  EXPECT_TRUE(buffer->address() != nullptr);
  // Check shared memory was allocated.
  EXPECT_EQ(1lu, mapped_memory_->num_chunks());
  // Check we get nothing for a non-existent buffer.
  EXPECT_TRUE(buffer_tracker_->GetBuffer(kId2) == nullptr);
  // Check we can delete the buffer.
  buffer_tracker_->RemoveBuffer(kId1);
  // Check shared memory was freed.
  mapped_memory_->FreeUnused();
  EXPECT_EQ(0lu, mapped_memory_->num_chunks());
  // Check we get nothing for a non-existent buffer.
  EXPECT_TRUE(buffer_tracker_->GetBuffer(kId1) == nullptr);
}

TEST_F(BufferTrackerTest, ZeroSize) {
  const GLuint kId = 123;

  // Check we can create a Buffer with zero size.
  BufferTracker::Buffer* buffer = buffer_tracker_->CreateBuffer(kId, 0);
  ASSERT_TRUE(buffer != nullptr);
  // Check mapped memory address.
  EXPECT_TRUE(buffer->address() == nullptr);
  // Check no shared memory was allocated.
  EXPECT_EQ(0lu, mapped_memory_->num_chunks());
  // Check we can delete the buffer.
  buffer_tracker_->RemoveBuffer(kId);
}

TEST_F(BufferTrackerTest, LostContext) {
  const GLuint kId = 123;
  const GLsizeiptr size = 64;

  command_buffer_->set_context_lost(true);
  // Check we can create a Buffer when after losing context.
  BufferTracker::Buffer* buffer = buffer_tracker_->CreateBuffer(kId, size);
  ASSERT_TRUE(buffer != nullptr);
  // Check mapped memory address.
  EXPECT_EQ(64u, buffer->size());
  // Check mapped memory address.
  EXPECT_TRUE(buffer->address() == nullptr);
  // Check no shared memory was allocated.
  EXPECT_EQ(0lu, mapped_memory_->num_chunks());
  // Check we can delete the buffer.
  buffer_tracker_->RemoveBuffer(kId);
}

TEST_F(BufferTrackerTest, Unmanage) {
  const GLuint kId = 123;
  const GLsizeiptr size = 64;

  BufferTracker::Buffer* buffer = buffer_tracker_->CreateBuffer(kId, size);
  ASSERT_TRUE(buffer != nullptr);
  EXPECT_EQ(mapped_memory_->bytes_in_use(), static_cast<size_t>(size));

  void* mem = buffer->address();
  buffer_tracker_->Unmanage(buffer);
  buffer_tracker_->RemoveBuffer(kId);
  EXPECT_EQ(mapped_memory_->bytes_in_use(), static_cast<size_t>(size));

  mapped_memory_->Free(mem);
  EXPECT_EQ(mapped_memory_->bytes_in_use(), static_cast<size_t>(0));
}

}  // namespace gles2
}  // namespace gpu
