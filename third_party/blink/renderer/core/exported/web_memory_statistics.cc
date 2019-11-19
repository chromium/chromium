// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_memory_statistics.h"

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

WebMemoryStatistics WebMemoryStatistics::Get() {
  WebMemoryStatistics statistics;
  statistics.partition_alloc_total_allocated_bytes =
      WTF::Partitions::TotalActiveBytes();
  statistics.blink_gc_total_allocated_bytes =
      ProcessHeap::TotalAllocatedObjectSize();
  return statistics;
}

}  // namespace blink
