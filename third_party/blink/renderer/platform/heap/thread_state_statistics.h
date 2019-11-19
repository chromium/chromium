// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_STATISTICS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_STATISTICS_H_

#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

struct PLATFORM_EXPORT ThreadState::Statistics final {
  enum DetailLevel : uint32_t {
    kBrief,
    kDetailed,
  };

  struct ObjectStatistics {
    size_t num_types = 0;
    Vector<std::string> type_name;
    Vector<size_t> type_count;
    Vector<size_t> type_bytes;
  };

  struct PageStatistics {
    size_t committed_size_bytes = 0;
    size_t used_size_bytes = 0;
  };

  struct FreeListStatistics {
    Vector<size_t> bucket_size;
    Vector<size_t> free_count;
    Vector<size_t> free_size;
  };

  struct ArenaStatistics {
    std::string name;
    size_t committed_size_bytes = 0;
    size_t used_size_bytes = 0;
    Vector<PageStatistics> page_stats;
    FreeListStatistics free_list_stats;
    // Only filled when NameClient::HideInternalName() is false.
    ObjectStatistics object_stats;
  };

  size_t committed_size_bytes = 0;
  size_t used_size_bytes = 0;
  DetailLevel detail_level;

  // Only filled when detail_level is kDetailed.
  Vector<ArenaStatistics> arena_stats;
};

class PLATFORM_EXPORT ThreadState::StatisticsCollector {
 public:
  explicit StatisticsCollector(ThreadState* thread_state)
      : thread_state_(thread_state) {}

  ThreadState::Statistics CollectStatistics(Statistics::DetailLevel) const;

 private:
  ThreadState* const thread_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_STATISTICS_H_
