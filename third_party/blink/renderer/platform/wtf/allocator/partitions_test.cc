// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

#include <vector>

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

// Otherwise, PartitionAlloc doesn't allocate any memory, and the tests are
// meaningless.
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

class PartitionsTest : public ::testing::Test {
 protected:
  void TearDown() override {
    base::PartitionAllocMemoryReclaimer::Instance()->ReclaimAll();
  }
};

TEST_F(PartitionsTest, MemoryIsInitiallyCommitted) {
  // std::vector to explicitly not use PartitionAlloc.
  std::vector<void*> allocated_pointers;

  size_t committed_before = Partitions::TotalSizeOfCommittedPages();
  // Need to allocate enough memory to require a new super page. Unless nothing
  // else in the process has allocated anything, this can be after several
  // iterations.
  while (Partitions::TotalSizeOfCommittedPages() == committed_before) {
    void* data = Partitions::BufferMalloc(100, "");
    ASSERT_TRUE(data);
    allocated_pointers.push_back(data);
  }
  size_t committed_after = Partitions::TotalSizeOfCommittedPages();

  // No buffer data committed initially, hence committed size increases.
  EXPECT_GT(committed_after, committed_before);
  // Increase is larger than the allocation.
  EXPECT_GT(committed_after, committed_before + allocated_pointers.size());

  for (void* data : allocated_pointers)
    Partitions::BufferFree(data);

  // Decommit is not triggered by deallocation.
  size_t committed_after_free = Partitions::TotalSizeOfCommittedPages();
  EXPECT_EQ(committed_after_free, committed_after);
}

TEST_F(PartitionsTest, Decommit) {
  std::vector<void*> allocated_pointers;

  size_t committed_before = Partitions::TotalSizeOfCommittedPages();
  while (Partitions::TotalSizeOfCommittedPages() == committed_before) {
    void* data = Partitions::BufferMalloc(100, "");
    ASSERT_TRUE(data);
    allocated_pointers.push_back(data);
  }
  size_t committed_after = Partitions::TotalSizeOfCommittedPages();

  for (void* data : allocated_pointers)
    Partitions::BufferFree(data);

  // Decommit is not triggered by deallocation.
  EXPECT_GT(committed_after, committed_before);
  // Decommit works.
  base::PartitionAllocMemoryReclaimer::Instance()->ReclaimAll();
  EXPECT_LT(Partitions::TotalSizeOfCommittedPages(), committed_after);
}

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

}  // namespace WTF
