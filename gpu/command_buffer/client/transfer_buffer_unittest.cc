// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Command Buffer Helper.

#include "gpu/command_buffer/client/transfer_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace gpu {


class TransferBufferTest : public testing::Test {
 protected:
  static const int32_t kNumCommandEntries = 400;
  static const int32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static const uint32_t kStartingOffset = 64;
  static const uint32_t kAlignment = 4;
  static const uint32_t kTransferBufferSize = 256;

  TransferBufferTest()
      : transfer_buffer_id_(0) {
  }

  void SetUp() override;
  void TearDown() override;

  virtual void Initialize() {
    ASSERT_TRUE(transfer_buffer_->Initialize(
        kTransferBufferSize, kStartingOffset, kTransferBufferSize,
        kTransferBufferSize, kAlignment));
  }

  MockClientCommandBufferMockFlush* command_buffer() const {
    return command_buffer_.get();
  }

  std::unique_ptr<MockClientCommandBufferMockFlush> command_buffer_;
  std::unique_ptr<CommandBufferHelper> helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  int32_t transfer_buffer_id_;
};

void TransferBufferTest::SetUp() {
  command_buffer_.reset(new StrictMock<MockClientCommandBufferMockFlush>());

  helper_.reset(new CommandBufferHelper(command_buffer()));
  ASSERT_EQ(helper_->Initialize(kCommandBufferSizeBytes),
            gpu::ContextResult::kSuccess);

  transfer_buffer_id_ = command_buffer()->GetNextFreeTransferBufferId();

  transfer_buffer_.reset(new TransferBuffer(helper_.get()));
}

void TransferBufferTest::TearDown() {
  if (transfer_buffer_->HaveBuffer()) {
    EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
        .Times(1)
        .RetiresOnSaturation();
  }
  // For command buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), Flush(_)).Times(AtMost(1));
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_)).Times(AtMost(2));
  transfer_buffer_.reset();
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const int32_t TransferBufferTest::kNumCommandEntries;
const int32_t TransferBufferTest::kCommandBufferSizeBytes;
const uint32_t TransferBufferTest::kStartingOffset;
const uint32_t TransferBufferTest::kAlignment;
const uint32_t TransferBufferTest::kTransferBufferSize;
#endif

TEST_F(TransferBufferTest, Basic) {
  Initialize();
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());
  EXPECT_EQ(transfer_buffer_id_, transfer_buffer_->GetShmId());
  EXPECT_EQ(
      kTransferBufferSize - kStartingOffset,
      transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());
  EXPECT_NE(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());
}

TEST_F(TransferBufferTest, Free) {
  Initialize();
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());
  EXPECT_EQ(transfer_buffer_id_, transfer_buffer_->GetShmId());
  EXPECT_NE(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());

  // Free buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());
  EXPECT_EQ(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());
  // See that it gets reallocated.
  EXPECT_EQ(transfer_buffer_id_, transfer_buffer_->GetShmId());
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());
  EXPECT_NE(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());

  // Free buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());
  EXPECT_EQ(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());

  // See that it gets reallocated.
  EXPECT_TRUE(transfer_buffer_->AcquireResultBuffer() != nullptr);
  transfer_buffer_->ReleaseResultBuffer();
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());
  EXPECT_NE(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());

  // Free buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());
  EXPECT_EQ(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());

  // See that it gets reallocated.
  uint32_t size = 0;
  void* data = transfer_buffer_->AllocUpTo(1, &size);
  EXPECT_TRUE(data != nullptr);
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());
  EXPECT_NE(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());
  int32_t token = helper_->InsertToken();
  int32_t put_offset = helper_->GetPutOffsetForTest();
  transfer_buffer_->FreePendingToken(data, token);

  // Free buffer. Should cause an ordering barrier.
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_)).Times(AtMost(1));
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());
  EXPECT_EQ(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());
  // Free should not have caused a finish.
  EXPECT_LT(command_buffer_->GetState().get_offset, put_offset);

  // See that it gets reallocated.
  transfer_buffer_->GetShmId();
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());
  EXPECT_NE(base::UnguessableToken(), transfer_buffer_->shared_memory_guid());

  EXPECT_EQ(
      kTransferBufferSize - kStartingOffset,
      transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());

  // Test freeing twice.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  transfer_buffer_->Free();
}

TEST_F(TransferBufferTest, TooLargeAllocation) {
  Initialize();
  // Check that we can't allocate large than max size.
  void* ptr = transfer_buffer_->Alloc(kTransferBufferSize + 1);
  EXPECT_TRUE(ptr == nullptr);
  // Check we if we try to allocate larger than max we get max.
  uint32_t size_allocated = 0;
  ptr = transfer_buffer_->AllocUpTo(
      kTransferBufferSize + 1, &size_allocated);
  ASSERT_TRUE(ptr != nullptr);
  EXPECT_EQ(kTransferBufferSize - kStartingOffset, size_allocated);
  transfer_buffer_->FreePendingToken(ptr, 1);
}

TEST_F(TransferBufferTest, MemoryAlignmentAfterZeroAllocation) {
  Initialize();
  void* ptr = transfer_buffer_->Alloc(0);
  EXPECT_EQ((reinterpret_cast<uintptr_t>(ptr) & (kAlignment - 1)), 0u);
  transfer_buffer_->FreePendingToken(ptr, helper_->InsertToken());
  // Check that the pointer is aligned on the following allocation.
  ptr = transfer_buffer_->Alloc(4);
  EXPECT_EQ((reinterpret_cast<uintptr_t>(ptr) & (kAlignment - 1)), 0u);
  transfer_buffer_->FreePendingToken(ptr, helper_->InsertToken());
}

class MockClientCommandBufferCanFail : public MockClientCommandBufferMockFlush {
 public:
  MockClientCommandBufferCanFail() = default;
  ~MockClientCommandBufferCanFail() override = default;

  MOCK_METHOD2(CreateTransferBuffer,
               scoped_refptr<Buffer>(uint32_t size, int32_t* id));

  scoped_refptr<gpu::Buffer> RealCreateTransferBuffer(uint32_t size,
                                                      int32_t* id) {
    return MockClientCommandBufferMockFlush::CreateTransferBuffer(size, id);
  }
};

class TransferBufferExpandContractTest : public testing::Test {
 protected:
  static const int32_t kNumCommandEntries = 400;
  static const int32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static const uint32_t kStartingOffset = 64;
  static const uint32_t kAlignment = 4;
  static const uint32_t kStartTransferBufferSize = 256;
  static const uint32_t kMaxTransferBufferSize = 1024;
  static const uint32_t kMinTransferBufferSize = 128;

  TransferBufferExpandContractTest()
      : transfer_buffer_id_(0) {
  }

  void SetUp() override;
  void TearDown() override;

  MockClientCommandBufferCanFail* command_buffer() const {
    return command_buffer_.get();
  }

  std::unique_ptr<MockClientCommandBufferCanFail> command_buffer_;
  std::unique_ptr<CommandBufferHelper> helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  int32_t transfer_buffer_id_;
};

void TransferBufferExpandContractTest::SetUp() {
  command_buffer_.reset(new StrictMock<MockClientCommandBufferCanFail>());
  command_buffer_->SetTokenForSetGetBuffer(0);

  EXPECT_CALL(*command_buffer(),
              CreateTransferBuffer(kCommandBufferSizeBytes, _))
      .WillOnce(Invoke(
          command_buffer(),
          &MockClientCommandBufferCanFail::RealCreateTransferBuffer))
      .RetiresOnSaturation();

  helper_.reset(new CommandBufferHelper(command_buffer()));
  ASSERT_EQ(helper_->Initialize(kCommandBufferSizeBytes),
            gpu::ContextResult::kSuccess);

  transfer_buffer_id_ = command_buffer()->GetNextFreeTransferBufferId();

  EXPECT_CALL(*command_buffer(),
              CreateTransferBuffer(kStartTransferBufferSize, _))
      .WillOnce(Invoke(
          command_buffer(),
          &MockClientCommandBufferCanFail::RealCreateTransferBuffer))
      .RetiresOnSaturation();

  transfer_buffer_.reset(new TransferBuffer(helper_.get()));
  ASSERT_TRUE(transfer_buffer_->Initialize(
      kStartTransferBufferSize, kStartingOffset, kMinTransferBufferSize,
      kMaxTransferBufferSize, kAlignment));
}

void TransferBufferExpandContractTest::TearDown() {
  if (transfer_buffer_->HaveBuffer()) {
    EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
        .Times(1)
        .RetiresOnSaturation();
  }
  // For command buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), Flush(_)).Times(1).RetiresOnSaturation();
  transfer_buffer_.reset();
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const int32_t TransferBufferExpandContractTest::kNumCommandEntries;
const int32_t TransferBufferExpandContractTest::kCommandBufferSizeBytes;
const uint32_t TransferBufferExpandContractTest::kStartingOffset;
const uint32_t TransferBufferExpandContractTest::kAlignment;
const uint32_t TransferBufferExpandContractTest::kStartTransferBufferSize;
const uint32_t TransferBufferExpandContractTest::kMaxTransferBufferSize;
const uint32_t TransferBufferExpandContractTest::kMinTransferBufferSize;
#endif

TEST_F(TransferBufferExpandContractTest, ExpandWithSmallAllocations) {
  int32_t token = helper_->InsertToken();
  EXPECT_FALSE(helper_->HasTokenPassed(token));

  auto ExpectCreateTransferBuffer = [&](int size) {
    EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*command_buffer(), CreateTransferBuffer(size, _))
        .WillOnce(
            Invoke(command_buffer(),
                   &MockClientCommandBufferCanFail::RealCreateTransferBuffer))
        .RetiresOnSaturation();
  };

  // Check it starts at starting size.
  EXPECT_EQ(
      kStartTransferBufferSize - kStartingOffset,
      transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());

  // Fill the free space.
  uint32_t size_allocated = 0;
  void* ptr = transfer_buffer_->AllocUpTo(transfer_buffer_->GetFreeSize(),
                                          &size_allocated);
  transfer_buffer_->FreePendingToken(ptr, token);

  // Allocate one more byte to force expansion.
  ExpectCreateTransferBuffer(kStartTransferBufferSize * 2);
  ptr = transfer_buffer_->AllocUpTo(1, &size_allocated);
  ASSERT_TRUE(ptr != nullptr);
  EXPECT_EQ(1u, size_allocated);
  transfer_buffer_->FreePendingToken(ptr, token);

  // Fill free space and expand again.
  ptr = transfer_buffer_->AllocUpTo(transfer_buffer_->GetFreeSize(),
                                    &size_allocated);
  transfer_buffer_->FreePendingToken(ptr, token);
  ExpectCreateTransferBuffer(kStartTransferBufferSize * 4);
  ptr = transfer_buffer_->AllocUpTo(1, &size_allocated);
  ASSERT_TRUE(ptr != nullptr);
  EXPECT_EQ(1u, size_allocated);
  transfer_buffer_->FreePendingToken(ptr, token);

  // Try to expand again, no expansion should occur because we are at max.
  ptr = transfer_buffer_->AllocUpTo(transfer_buffer_->GetFreeSize(),
                                    &size_allocated);
  transfer_buffer_->FreePendingToken(ptr, token);
  EXPECT_CALL(*command_buffer(), Flush(_)).Times(1).RetiresOnSaturation();
  ptr = transfer_buffer_->AllocUpTo(1, &size_allocated);
  ASSERT_TRUE(ptr != nullptr);
  EXPECT_EQ(1u, size_allocated);
  transfer_buffer_->FreePendingToken(ptr, token);
  EXPECT_EQ(kMaxTransferBufferSize - kStartingOffset,
            transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());
}

// Verify that expansion does not happen when there are blocks in use.
TEST_F(TransferBufferExpandContractTest, NoExpandWithInUseAllocation) {
  EXPECT_CALL(*command_buffer(), Flush(_)).Times(1).RetiresOnSaturation();

  int32_t token = helper_->InsertToken();
  EXPECT_FALSE(helper_->HasTokenPassed(token));

  // Check it starts at starting size.
  EXPECT_EQ(kStartTransferBufferSize - kStartingOffset,
            transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());

  // Fill the free space in two blocks.
  uint32_t block_size_1 = transfer_buffer_->GetFreeSize() / 2;
  uint32_t block_size_2 = transfer_buffer_->GetFreeSize() - block_size_1;
  uint32_t size_allocated = 0;
  void* block1 = transfer_buffer_->AllocUpTo(block_size_1, &size_allocated);
  EXPECT_EQ(block_size_1, size_allocated);
  void* block2 = transfer_buffer_->AllocUpTo(block_size_2, &size_allocated);
  EXPECT_EQ(block_size_2, size_allocated);
  transfer_buffer_->FreePendingToken(block1, token);

  // Expansion tries to happens when GetFreeSize() is not enough for the
  // allocation.
  EXPECT_EQ(0u, transfer_buffer_->GetFreeSize());

  // Allocate one more byte to try to force expansion, however there are
  // blocks in use, so this should not expand.
  void* block3 = transfer_buffer_->AllocUpTo(1, &size_allocated);
  ASSERT_TRUE(block3 != nullptr);
  EXPECT_EQ(1u, size_allocated);
  transfer_buffer_->FreePendingToken(block3, token);
  transfer_buffer_->FreePendingToken(block2, token);

  // No reallocs should have occurred.
  EXPECT_EQ(kStartTransferBufferSize - kStartingOffset,
            transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());
}

TEST_F(TransferBufferExpandContractTest, ExpandWithLargeAllocations) {
  int32_t token = helper_->InsertToken();
  EXPECT_FALSE(helper_->HasTokenPassed(token));

  auto ExpectCreateTransferBuffer = [&](int size) {
    EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*command_buffer(), CreateTransferBuffer(size, _))
        .WillOnce(
            Invoke(command_buffer(),
                   &MockClientCommandBufferCanFail::RealCreateTransferBuffer))
        .RetiresOnSaturation();
  };

  // Check it starts at starting size.
  EXPECT_EQ(kStartTransferBufferSize - kStartingOffset,
            transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());

  // Allocate one byte more than the free space to force expansion.
  uint32_t size_allocated = 0;
  ExpectCreateTransferBuffer(kStartTransferBufferSize * 2);
  void* ptr = transfer_buffer_->AllocUpTo(transfer_buffer_->GetFreeSize() + 1,
                                          &size_allocated);
  transfer_buffer_->FreePendingToken(ptr, token);

  // Expand again.
  ExpectCreateTransferBuffer(kStartTransferBufferSize * 4);
  uint32_t size_requested = transfer_buffer_->GetFreeSize() + 1;
  ptr = transfer_buffer_->AllocUpTo(size_requested, &size_allocated);
  ASSERT_TRUE(ptr != nullptr);
  EXPECT_EQ(size_requested, size_allocated);
  transfer_buffer_->FreePendingToken(ptr, token);

  // Try to expand again, no expansion should occur because we are at max.
  EXPECT_CALL(*command_buffer(), Flush(_)).Times(1).RetiresOnSaturation();
  size_requested =
      transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc() + 1;
  ptr = transfer_buffer_->AllocUpTo(size_requested, &size_allocated);
  EXPECT_LT(size_allocated, size_requested);
  transfer_buffer_->FreePendingToken(ptr, token);
  EXPECT_EQ(kMaxTransferBufferSize - kStartingOffset,
            transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());
}

TEST_F(TransferBufferExpandContractTest, ShrinkRingBuffer) {
  int32_t token = helper_->InsertToken();
  // For this test we want all allocations to be freed immediately.
  command_buffer_->SetToken(token);
  EXPECT_TRUE(helper_->HasTokenPassed(token));

  auto ExpectCreateTransferBuffer = [&](int size) {
    EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*command_buffer(), CreateTransferBuffer(size, _))
        .WillOnce(
            Invoke(command_buffer(),
                   &MockClientCommandBufferCanFail::RealCreateTransferBuffer))
        .RetiresOnSaturation();
  };

  // Expand the ring buffer to the maximum size.
  ExpectCreateTransferBuffer(kMaxTransferBufferSize);
  void* ptr = transfer_buffer_->Alloc(kMaxTransferBufferSize - kStartingOffset);
  EXPECT_TRUE(ptr != nullptr);
  transfer_buffer_->FreePendingToken(ptr, token);

  // We shouldn't shrink before we reach the allocation threshold.
  for (uint32_t allocated = kMaxTransferBufferSize - kStartingOffset;
       allocated < (kStartTransferBufferSize + kStartingOffset) *
                       (TransferBuffer::kShrinkThreshold);) {
    ptr = transfer_buffer_->Alloc(kStartTransferBufferSize);
    EXPECT_TRUE(ptr != nullptr);
    transfer_buffer_->FreePendingToken(ptr, token);
    allocated += kStartTransferBufferSize;
  }
  // The next allocation should trip the threshold and shrink.
  ExpectCreateTransferBuffer(kStartTransferBufferSize * 2);
  ptr = transfer_buffer_->Alloc(1);
  EXPECT_TRUE(ptr != nullptr);
  transfer_buffer_->FreePendingToken(ptr, token);
}

TEST_F(TransferBufferExpandContractTest, Contract) {
  // Check it starts at starting size.
  EXPECT_EQ(
      kStartTransferBufferSize - kStartingOffset,
      transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());

  // Free buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());

  // Try to allocate again, fail first request
  EXPECT_CALL(*command_buffer(),
              CreateTransferBuffer(kStartTransferBufferSize, _))
      .WillOnce(
           DoAll(SetArgPointee<1>(-1), Return(scoped_refptr<gpu::Buffer>())))
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(),
              CreateTransferBuffer(kMinTransferBufferSize, _))
      .WillOnce(Invoke(
          command_buffer(),
          &MockClientCommandBufferCanFail::RealCreateTransferBuffer))
      .RetiresOnSaturation();

  const uint32_t kSize1 = 256 - kStartingOffset;
  const uint32_t kSize2 = 128 - kStartingOffset;
  uint32_t size_allocated = 0;
  void* ptr = transfer_buffer_->AllocUpTo(kSize1, &size_allocated);
  ASSERT_TRUE(ptr != nullptr);
  EXPECT_EQ(kSize2, size_allocated);
  EXPECT_EQ(kSize2, transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());
  transfer_buffer_->FreePendingToken(ptr, 1);

  // Free buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());

  // Try to allocate again,
  EXPECT_CALL(*command_buffer(),
              CreateTransferBuffer(kMinTransferBufferSize, _))
      .WillOnce(Invoke(
          command_buffer(),
          &MockClientCommandBufferCanFail::RealCreateTransferBuffer))
      .RetiresOnSaturation();

  ptr = transfer_buffer_->AllocUpTo(kSize1, &size_allocated);
  ASSERT_TRUE(ptr != nullptr);
  EXPECT_EQ(kSize2, size_allocated);
  EXPECT_EQ(kSize2, transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());
  transfer_buffer_->FreePendingToken(ptr, 1);
}

TEST_F(TransferBufferExpandContractTest, OutOfMemory) {
  // Free buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());

  // Try to allocate again, fail both requests.
  EXPECT_CALL(*command_buffer(), CreateTransferBuffer(_, _))
      .WillOnce(
           DoAll(SetArgPointee<1>(-1), Return(scoped_refptr<gpu::Buffer>())))
      .WillOnce(
           DoAll(SetArgPointee<1>(-1), Return(scoped_refptr<gpu::Buffer>())))
      .WillOnce(
           DoAll(SetArgPointee<1>(-1), Return(scoped_refptr<gpu::Buffer>())))
      .RetiresOnSaturation();

  const uint32_t kSize1 = 512 - kStartingOffset;
  uint32_t size_allocated = 0;
  void* ptr = transfer_buffer_->AllocUpTo(kSize1, &size_allocated);
  ASSERT_TRUE(ptr == nullptr);
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());
}

TEST_F(TransferBufferExpandContractTest, ReallocsToDefault) {
  // Free buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*command_buffer(), OrderingBarrier(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());

  // See that it gets reallocated.
  EXPECT_CALL(*command_buffer(),
              CreateTransferBuffer(kStartTransferBufferSize, _))
      .WillOnce(Invoke(
          command_buffer(),
          &MockClientCommandBufferCanFail::RealCreateTransferBuffer))
      .RetiresOnSaturation();
  EXPECT_EQ(transfer_buffer_id_, transfer_buffer_->GetShmId());
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());

  // Check it's the default size.
  EXPECT_EQ(
      kStartTransferBufferSize - kStartingOffset,
      transfer_buffer_->GetCurrentMaxAllocationWithoutRealloc());
}

TEST_F(TransferBufferExpandContractTest, Shrink) {
  uint32_t alloc_size = transfer_buffer_->GetFreeSize();
  EXPECT_EQ(kStartTransferBufferSize - kStartingOffset, alloc_size);
  uint32_t size_allocated = 0;
  void* ptr = transfer_buffer_->AllocUpTo(alloc_size, &size_allocated);

  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(alloc_size, size_allocated);
  EXPECT_GT(alloc_size, 0u);
  EXPECT_EQ(0u, transfer_buffer_->GetFreeSize());

  // Shrink once.
  const uint32_t shrink_size1 = 64;
  EXPECT_LT(shrink_size1, alloc_size);
  transfer_buffer_->ShrinkLastBlock(shrink_size1 - kAlignment + 1);
  EXPECT_EQ(alloc_size - shrink_size1, transfer_buffer_->GetFreeSize());

  // Shrink again.
  const uint32_t shrink_size2 = 32;
  EXPECT_LT(shrink_size2, shrink_size1);
  transfer_buffer_->ShrinkLastBlock(shrink_size2);
  EXPECT_EQ(alloc_size - shrink_size2, transfer_buffer_->GetFreeSize());

  // Shrink to zero (minimum size is kAlignment).
  transfer_buffer_->ShrinkLastBlock(0);
  EXPECT_EQ(alloc_size - kAlignment, transfer_buffer_->GetFreeSize());

  transfer_buffer_->FreePendingToken(ptr, 1);
}

TEST_F(TransferBufferTest, MultipleAllocsAndFrees) {
  // An arbitrary size, but is aligned so no padding needed.
  constexpr uint32_t kArbitrarySize = 16;

  Initialize();
  uint32_t original_free_size = transfer_buffer_->GetFreeSize();
  EXPECT_EQ(transfer_buffer_->GetSize(), original_free_size);
  EXPECT_EQ(transfer_buffer_->GetFragmentedFreeSize(), original_free_size);

  void* ptr1 = transfer_buffer_->Alloc(kArbitrarySize);
  EXPECT_NE(ptr1, nullptr);
  EXPECT_EQ(transfer_buffer_->GetSize(), original_free_size);
  EXPECT_EQ(transfer_buffer_->GetFreeSize(),
            original_free_size - kArbitrarySize);
  EXPECT_EQ(transfer_buffer_->GetFragmentedFreeSize(),
            original_free_size - kArbitrarySize);

  void* ptr2 = transfer_buffer_->Alloc(kArbitrarySize);
  EXPECT_NE(ptr2, nullptr);
  EXPECT_EQ(transfer_buffer_->GetSize(), original_free_size);
  EXPECT_EQ(transfer_buffer_->GetFreeSize(),
            original_free_size - kArbitrarySize * 2);
  EXPECT_EQ(transfer_buffer_->GetFragmentedFreeSize(),
            original_free_size - kArbitrarySize * 2);

  void* ptr3 = transfer_buffer_->Alloc(kArbitrarySize);
  EXPECT_NE(ptr3, nullptr);
  EXPECT_EQ(transfer_buffer_->GetSize(), original_free_size);
  EXPECT_EQ(transfer_buffer_->GetFreeSize(),
            original_free_size - kArbitrarySize * 3);
  EXPECT_EQ(transfer_buffer_->GetFragmentedFreeSize(),
            original_free_size - kArbitrarySize * 3);

  // Generate tokens in order, but submit out of order.
  auto token1 = helper_->InsertToken();
  auto token2 = helper_->InsertToken();
  auto token3 = helper_->InsertToken();
  auto token4 = helper_->InsertToken();

  // Freeing the final block here, is not perceivable because it's a hole.
  transfer_buffer_->FreePendingToken(ptr3, token3);
  EXPECT_EQ(transfer_buffer_->GetSize(), original_free_size);
  EXPECT_EQ(transfer_buffer_->GetFreeSize(),
            original_free_size - kArbitrarySize * 3);
  EXPECT_EQ(transfer_buffer_->GetFragmentedFreeSize(),
            original_free_size - kArbitrarySize * 3);

  // Freeing the first block here leaves the second plus a hole after, so
  // perceived two blocks not free yet.  The free size (no waiting) has not
  // changed because the free_offset_ has not moved, but the fragmented free
  // size gets bigger because in_use_offset_ has moved past the first block.
  transfer_buffer_->FreePendingToken(ptr1, token1);
  EXPECT_EQ(transfer_buffer_->GetSize(), original_free_size);
  EXPECT_EQ(transfer_buffer_->GetFreeSize(),
            original_free_size - kArbitrarySize * 3);
  EXPECT_EQ(transfer_buffer_->GetFragmentedFreeSize(),
            original_free_size - kArbitrarySize * 2);

  // Allocate a 4th block.  This leaves the state as: Freed Used Freed Used
  void* ptr4 = transfer_buffer_->Alloc(kArbitrarySize);
  EXPECT_EQ(transfer_buffer_->GetSize(), original_free_size);
  EXPECT_EQ(transfer_buffer_->GetFreeSize(),
            original_free_size - kArbitrarySize * 4);
  EXPECT_EQ(transfer_buffer_->GetFragmentedFreeSize(),
            original_free_size - kArbitrarySize * 3);

  // Freeing the second and fourth block makes everything free, so back to
  // original size.
  transfer_buffer_->FreePendingToken(ptr4, token4);
  transfer_buffer_->FreePendingToken(ptr2, token2);
  EXPECT_EQ(transfer_buffer_->GetSize(), original_free_size);
  EXPECT_EQ(transfer_buffer_->GetFreeSize(), original_free_size);
  EXPECT_EQ(transfer_buffer_->GetFragmentedFreeSize(), original_free_size);
}

#if defined(GTEST_HAS_DEATH_TEST) && DCHECK_IS_ON()

TEST_F(TransferBufferTest, ResizeDuringScopedResultPtr) {
  Initialize();
  ScopedResultPtr<int> ptr(transfer_buffer_.get());
  // If an attempt is made to resize the transfer buffer while a result
  // pointer exists, we should hit a CHECK. Allocate just enough to force a
  // resize.
  uint32_t size_allocated;
  ASSERT_DEATH(transfer_buffer_->AllocUpTo(transfer_buffer_->GetFreeSize() + 1,
                                           &size_allocated),
               "outstanding_result_pointer_");
}

TEST_F(TransferBufferTest, AllocDuringScopedResultPtr) {
  Initialize();
  ScopedResultPtr<int> ptr(transfer_buffer_.get());
  // If an attempt is made to allocate any amount in the transfer buffer while a
  // result pointer exists, we should hit a DCHECK.
  uint32_t size_allocated;
  ASSERT_DEATH(transfer_buffer_->AllocUpTo(transfer_buffer_->GetFreeSize() + 1,
                                           &size_allocated),
               "outstanding_result_pointer_");
}

TEST_F(TransferBufferTest, TwoScopedResultPtrs) {
  Initialize();
  // Attempting to create two ScopedResultPtrs at the same time should DCHECK.
  ScopedResultPtr<int> ptr(transfer_buffer_.get());
  ASSERT_DEATH(ScopedResultPtr<int>(transfer_buffer_.get()),
               "outstanding_result_pointer_");
}

#endif  // defined(GTEST_HAS_DEATH_TEST) && DCHECK_IS_ON()

}  // namespace gpu
