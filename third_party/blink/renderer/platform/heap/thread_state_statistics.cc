// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/thread_state_statistics.h"

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"

namespace blink {

ThreadState::Statistics ThreadState::StatisticsCollector::CollectStatistics(
    Statistics::DetailLevel detail_level) const {
  Statistics stats;
  stats.detail_level = detail_level;
  if (detail_level == Statistics::kBrief) {
    ThreadHeapStatsCollector* stats_collector =
        thread_state_->Heap().stats_collector();
    stats.committed_size_bytes = stats_collector->allocated_space_bytes();
    stats.used_size_bytes = stats_collector->object_size_in_bytes();
    return stats;
  }

  thread_state_->CompleteSweep();

  // Detailed statistics.
  thread_state_->Heap().CollectStatistics(&stats);
  stats.detail_level = Statistics::kDetailed;
  return stats;
}

}  // namespace blink
