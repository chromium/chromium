// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shm_count.h"

#include "base/memory/unsafe_shared_memory_region.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

TEST(ShmCountTest, BasicUsage) {
  // Create the host count.
  GpuProcessHostShmCount host_count;
  EXPECT_EQ(host_count.GetCount(), 0);

  // Create the service count from host memory.
  GpuProcessShmCount service_count(host_count.CloneRegion());

  // Ensure we can increment and decrement.
  {
    GpuProcessShmCount::ScopedIncrement scoped_increment(&service_count);
    EXPECT_EQ(host_count.GetCount(), 1);
    {
      GpuProcessShmCount::ScopedIncrement scoped_increment2(&service_count);
      EXPECT_EQ(host_count.GetCount(), 2);
    }
    EXPECT_EQ(host_count.GetCount(), 1);
  }
  EXPECT_EQ(host_count.GetCount(), 0);
}

TEST(ShmCountTest, NotInitialized) {
  // Get the service count without providing host memory.
  GpuProcessShmCount service_count{base::UnsafeSharedMemoryRegion()};

  // Increment/decrement should not crash.
  { GpuProcessShmCount::ScopedIncrement scoped_increment(&service_count); }
}

}  // namespace gpu
