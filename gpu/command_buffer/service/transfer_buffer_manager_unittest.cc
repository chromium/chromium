// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/transfer_buffer_manager.h"

#include <stddef.h>

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

const static size_t kBufferSize = 1024;

class TransferBufferManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    transfer_buffer_manager_ = std::make_unique<TransferBufferManager>(nullptr);
  }

  std::unique_ptr<TransferBufferManager> transfer_buffer_manager_;
};

TEST_F(TransferBufferManagerTest, ZeroHandleMapsToNull) {
  EXPECT_TRUE(nullptr == transfer_buffer_manager_->GetTransferBuffer(0).get());
}

TEST_F(TransferBufferManagerTest, NegativeHandleMapsToNull) {
  EXPECT_TRUE(nullptr == transfer_buffer_manager_->GetTransferBuffer(-1).get());
}

TEST_F(TransferBufferManagerTest, OutOfRangeHandleMapsToNull) {
  EXPECT_TRUE(nullptr == transfer_buffer_manager_->GetTransferBuffer(1).get());
}

TEST_F(TransferBufferManagerTest, CanRegisterTransferBuffer) {
  base::UnsafeSharedMemoryRegion shm_region =
      base::UnsafeSharedMemoryRegion::Create(kBufferSize);
  base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
  auto shm_guid = shm_region.GetGUID();
  auto backing = std::make_unique<SharedMemoryBufferBacking>(
      std::move(shm_region), std::move(shm_mapping));
  SharedMemoryBufferBacking* backing_raw_ptr = backing.get();

  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, base::MakeRefCounted<Buffer>(std::move(backing))));
  scoped_refptr<Buffer> registered =
      transfer_buffer_manager_->GetTransferBuffer(1);

  // Shared-memory ownership is transfered. It should be the same memory.
  EXPECT_EQ(backing_raw_ptr, registered->backing());
  EXPECT_EQ(shm_guid, backing_raw_ptr->GetGUID());
}

TEST_F(TransferBufferManagerTest, CanDestroyTransferBuffer) {
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, MakeMemoryBuffer(42)));
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  scoped_refptr<Buffer> registered =
      transfer_buffer_manager_->GetTransferBuffer(1);

  scoped_refptr<Buffer> null_buffer;
  EXPECT_EQ(null_buffer, registered);
}

TEST_F(TransferBufferManagerTest, CannotRegregisterTransferBufferId) {
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, MakeMemoryBuffer(42)));
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, MakeMemoryBuffer(42)));
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, MakeMemoryBuffer(42)));
}

TEST_F(TransferBufferManagerTest, CanReuseTransferBufferIdAfterDestroying) {
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, MakeMemoryBuffer(42)));
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, MakeMemoryBuffer(42)));
}

TEST_F(TransferBufferManagerTest, DestroyUnusedTransferBufferIdDoesNotCrash) {
  transfer_buffer_manager_->DestroyTransferBuffer(1);
}

TEST_F(TransferBufferManagerTest, CannotRegisterNullTransferBuffer) {
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(
      0, MakeMemoryBuffer(42)));
}

TEST_F(TransferBufferManagerTest, CannotRegisterNegativeTransferBufferId) {
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(
      -1, MakeMemoryBuffer(42)));
}

}  // namespace gpu
