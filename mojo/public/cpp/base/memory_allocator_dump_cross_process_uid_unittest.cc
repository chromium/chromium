// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/memory_allocator_dump_cross_process_uid_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/memory_allocator_dump_cross_process_uid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace memory_allocator_dump_cross_process_uid_unittest {

TEST(MemoryAllocatorDumpCrossProcessUidTest, SerializeFailsOnZeroValue) {
  base::trace_event::MemoryAllocatorDumpGuid in(0);
  base::trace_event::MemoryAllocatorDumpGuid out;

  ASSERT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::MemoryAllocatorDumpCrossProcessUid>(in, out));
  EXPECT_EQ(in, out);
}

TEST(MemoryAllocatorDumpCrossProcessUidTest, SerializeSucceedsOnValidIntValue) {
  base::trace_event::MemoryAllocatorDumpGuid in(12345);
  base::trace_event::MemoryAllocatorDumpGuid out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::MemoryAllocatorDumpCrossProcessUid>(in, out));
  EXPECT_EQ(in, out);
}

TEST(MemoryAllocatorDumpCrossProcessUidTest,
     SerializeSucceedsOnValidStringValue) {
  base::trace_event::MemoryAllocatorDumpGuid in("12345");
  base::trace_event::MemoryAllocatorDumpGuid out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::MemoryAllocatorDumpCrossProcessUid>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace memory_allocator_dump_cross_process_uid_unittest
}  // namespace mojo_base
