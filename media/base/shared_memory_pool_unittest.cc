// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/shared_memory_pool.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(SharedMemoryPoolTest, CreatesRegion) {
  scoped_refptr<SharedMemoryPool> pool(
      base::MakeRefCounted<SharedMemoryPool>());
  auto handle = pool->MaybeAllocateBuffer(1000);
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetRegion());
  EXPECT_TRUE(handle->GetRegion()->IsValid());
  ASSERT_TRUE(handle->GetMapping());
  EXPECT_TRUE(handle->GetMapping()->IsValid());
}

TEST(SharedMemoryPoolTest, ReusesRegions) {
  scoped_refptr<SharedMemoryPool> pool(
      base::MakeRefCounted<SharedMemoryPool>());
  auto handle = pool->MaybeAllocateBuffer(1000u);
  ASSERT_TRUE(handle);
  base::UnsafeSharedMemoryRegion* region = handle->GetRegion();
  auto id1 = region->GetGUID();

  // Return memory to the pool.
  handle.reset();

  handle = pool->MaybeAllocateBuffer(1000u);
  region = handle->GetRegion();
  // Should reuse the freed region.
  EXPECT_EQ(id1, region->GetGUID());
}

TEST(SharedMemoryPoolTest, RespectsSize) {
  scoped_refptr<SharedMemoryPool> pool(
      base::MakeRefCounted<SharedMemoryPool>());
  auto handle = pool->MaybeAllocateBuffer(1000u);
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetRegion());
  EXPECT_GE(handle->GetRegion()->GetSize(), 1000u);

  handle = pool->MaybeAllocateBuffer(100u);
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetRegion());
  EXPECT_GE(handle->GetRegion()->GetSize(), 100u);

  handle = pool->MaybeAllocateBuffer(1100u);
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->GetRegion());
  EXPECT_GE(handle->GetRegion()->GetSize(), 1100u);
}
}  // namespace media
