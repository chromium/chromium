// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

scoped_refptr<Buffer> MakeBufferForTesting(uint32_t num_handles) {
  uint32_t size = sizeof(base::subtle::Atomic32) * num_handles;
  return MakeMemoryBuffer(size);
}

}  // namespace

TEST(DiscardableHandleTest, BasicUsage) {
  scoped_refptr<Buffer> buffer = MakeBufferForTesting(1);

  uint32_t byte_offset = 0;
  int32_t shm_id = 1;
  ClientDiscardableHandle client_handle(buffer, byte_offset, shm_id);
  EXPECT_EQ(client_handle.shm_id(), shm_id);
  EXPECT_TRUE(client_handle.IsLockedForTesting());

  ServiceDiscardableHandle service_handle(buffer, byte_offset, shm_id);
  EXPECT_EQ(service_handle.shm_id(), shm_id);
  EXPECT_TRUE(service_handle.IsLockedForTesting());

  EXPECT_FALSE(service_handle.Delete());
  EXPECT_FALSE(service_handle.IsDeletedForTesting());
  EXPECT_FALSE(client_handle.CanBeReUsed());

  service_handle.Unlock();
  EXPECT_FALSE(service_handle.IsLockedForTesting());
  EXPECT_FALSE(client_handle.IsLockedForTesting());

  EXPECT_TRUE(client_handle.Lock());
  EXPECT_TRUE(client_handle.IsLockedForTesting());
  EXPECT_TRUE(service_handle.IsLockedForTesting());

  service_handle.Unlock();
  EXPECT_FALSE(service_handle.IsLockedForTesting());
  EXPECT_FALSE(client_handle.IsLockedForTesting());

  EXPECT_TRUE(service_handle.Delete());
  EXPECT_TRUE(service_handle.IsDeletedForTesting());
  EXPECT_TRUE(client_handle.CanBeReUsed());
  EXPECT_FALSE(service_handle.IsLockedForTesting());
  EXPECT_FALSE(client_handle.IsLockedForTesting());

  EXPECT_FALSE(client_handle.Lock());
  EXPECT_FALSE(service_handle.IsLockedForTesting());
  EXPECT_FALSE(client_handle.IsLockedForTesting());
  EXPECT_TRUE(service_handle.IsDeletedForTesting());
  EXPECT_TRUE(client_handle.IsDeletedForTesting());
}

TEST(DiscardableHandleTest, MultiLock) {
  scoped_refptr<Buffer> buffer = MakeBufferForTesting(1);

  uint32_t byte_offset = 0;
  int32_t shm_id = 1;
  ClientDiscardableHandle client_handle(buffer, byte_offset, shm_id);
  EXPECT_EQ(client_handle.shm_id(), shm_id);
  EXPECT_TRUE(client_handle.IsLockedForTesting());

  ServiceDiscardableHandle service_handle(buffer, byte_offset, shm_id);
  EXPECT_EQ(service_handle.shm_id(), shm_id);
  EXPECT_TRUE(service_handle.IsLockedForTesting());

  for (int i = 1; i < 10; ++i) {
    EXPECT_TRUE(client_handle.IsLockedForTesting());
    EXPECT_TRUE(service_handle.IsLockedForTesting());
    EXPECT_TRUE(client_handle.Lock());
  }

  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(client_handle.IsLockedForTesting());
    EXPECT_TRUE(service_handle.IsLockedForTesting());
    service_handle.Unlock();
  }

  EXPECT_FALSE(client_handle.IsLockedForTesting());
  EXPECT_FALSE(service_handle.IsLockedForTesting());
}

TEST(DiscardableHandleTest, Suballocations) {
  static const int32_t num_elements = 10;
  scoped_refptr<Buffer> buffer = MakeBufferForTesting(num_elements);

  std::vector<ClientDiscardableHandle> client_handles;
  std::vector<ServiceDiscardableHandle> service_handles;
  for (int32_t i = 0; i < num_elements; ++i) {
    client_handles.emplace_back(buffer, sizeof(base::subtle::Atomic32) * i,
                                i + 1);
    EXPECT_EQ(client_handles[i].shm_id(), i + 1);
    EXPECT_TRUE(client_handles[i].IsLockedForTesting());

    service_handles.emplace_back(buffer, sizeof(base::subtle::Atomic32) * i,
                                 i + 1);
    EXPECT_EQ(service_handles[i].shm_id(), i + 1);
    EXPECT_TRUE(service_handles[i].IsLockedForTesting());
  }

  for (int32_t i = 0; i < num_elements; i += 2) {
    service_handles[i].Unlock();
  }

  for (int32_t i = 1; i < num_elements; i += 2) {
    client_handles[i].Lock();
  }

  for (int32_t i = 0; i < num_elements; ++i) {
    if (i % 2) {
      EXPECT_TRUE(client_handles[i].IsLockedForTesting());
      EXPECT_TRUE(service_handles[i].IsLockedForTesting());
    } else {
      EXPECT_FALSE(client_handles[i].IsLockedForTesting());
      EXPECT_FALSE(service_handles[i].IsLockedForTesting());
    }
  }
}

}  // namespace gpu
