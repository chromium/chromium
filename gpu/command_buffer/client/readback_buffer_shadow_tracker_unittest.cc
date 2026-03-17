// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/readback_buffer_shadow_tracker.h"

#include <memory>

#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu::gles2 {

class ReadbackBufferShadowTrackerTest : public testing::Test {
 protected:
  void SetUp() override {
    command_buffer_ = std::make_unique<MockClientCommandBuffer>();
    command_buffer_->DelegateToFake();
    helper_ = std::make_unique<GLES2CmdHelper>(command_buffer_.get());
    helper_->Initialize(1024);
    EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(testing::_))
        .Times(testing::AnyNumber());
    mapped_memory_ = std::make_unique<MappedMemoryManager>(helper_.get(), 1024);
    tracker_ = std::make_unique<ReadbackBufferShadowTracker>(
        mapped_memory_.get(), helper_.get());
  }

  std::unique_ptr<gpu::MockClientCommandBuffer> command_buffer_;
  std::unique_ptr<GLES2CmdHelper> helper_;
  std::unique_ptr<MappedMemoryManager> mapped_memory_;
  std::unique_ptr<ReadbackBufferShadowTracker> tracker_;
};

// Test that ReadbackBufferShadowTracker::Buffer::Alloc correctly handles
// MappedMemoryManager::Alloc failures.
TEST_F(ReadbackBufferShadowTrackerTest, AllocFails) {
  const GLuint kBufferId = 1;
  const GLuint kSize = 64;
  tracker_->GetOrCreateBuffer(kBufferId, kSize);

  int32_t shm_id = 0;
  uint32_t shm_offset = 0;
  bool already_allocated = false;

  // Make Alloc fail by setting a very small limit.
  mapped_memory_->set_max_allocated_bytes(1);

  auto* buffer = tracker_->GetBuffer(kBufferId);
  uint32_t allocated_size =
      buffer->Alloc(&shm_id, &shm_offset, &already_allocated);

  EXPECT_EQ(allocated_size, kSize);
  EXPECT_EQ(shm_id, -1);
  EXPECT_EQ(shm_offset, 0u);
  EXPECT_FALSE(already_allocated);
}

}  // namespace gpu::gles2
