// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/partition_alloc_memory_dump_provider.h"

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

TEST(PartitionAllocMemoryDumpProviderTest, Simple) {
  // Make sure there is at least one allocation.
  // Otherwise the hit rate is not computed.
  void* data = WTF::Partitions::FastMalloc(12, "");
  WTF::Partitions::FastFree(data);

  base::HistogramTester histogram_tester;
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  base::trace_event::ProcessMemoryDump pmd(args);
  PartitionAllocMemoryDumpProvider::Instance()->OnMemoryDump(args, &pmd);

#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED) &&         \
    !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  histogram_tester.ExpectTotalCount("Memory.PartitionAlloc.ThreadCache.HitRate",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Memory.PartitionAlloc.ThreadCache.HitRate.MainThread", 1);

  histogram_tester.ExpectTotalCount(
      "Memory.PartitionAlloc.ThreadCache.BatchFillRate", 1);
  histogram_tester.ExpectTotalCount(
      "Memory.PartitionAlloc.ThreadCache.HitRate.MainThread", 1);
#else
  histogram_tester.ExpectTotalCount("Memory.PartitionAlloc.ThreadCache.HitRate",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Memory.PartitionAlloc.ThreadCache.HitRate.MainThread", 0);
  histogram_tester.ExpectTotalCount(
      "Memory.PartitionAlloc.ThreadCache.BatchFillRate", 0);
  histogram_tester.ExpectTotalCount(
      "Memory.PartitionAlloc.ThreadCache.BatchFillRate.MainThread", 0);
#endif  // !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED) &&
        // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
}

}  // namespace blink
