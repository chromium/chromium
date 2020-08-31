// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_SCHEDULING_ORACLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_SCHEDULING_ORACLE_H_

#include <atomic>

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"

namespace blink {

class PLATFORM_EXPORT MarkingSchedulingOracle {
 public:
  // Estimated duration of GC cycle in milliseconds.
  static constexpr double kEstimatedMarkingTimeMs = 500.0;

  // Duration of one incremental marking step. Should be short enough that it
  // doesn't cause jank even though it is scheduled as a normal task.
  static constexpr base::TimeDelta kDefaultIncrementalMarkingStepDuration =
      base::TimeDelta::FromMillisecondsD(0.5);

  // Minimum number of bytes that should be marked during an incremental
  // marking step.
  static constexpr size_t kMinimumMarkedBytesInStep = 64 * 1024;

  // Maximum duration of one incremental marking step. Should be short enough
  // that it doesn't cause jank even though it is scheduled as a normal task.
  static constexpr base::TimeDelta kMaximumIncrementalMarkingStepDuration =
      base::TimeDelta::FromMillisecondsD(2.0);

  explicit MarkingSchedulingOracle();

  void UpdateIncrementalMarkingStats(size_t, base::TimeDelta, base::TimeDelta);
  void AddConcurrentlyMarkedBytes(size_t);

  size_t GetConcurrentlyMarkedBytes();
  size_t GetOverallMarkedBytes();

  base::TimeDelta GetNextIncrementalStepDurationForTask(size_t);

  void SetElapsedTimeForTesting(double elapsed_time) {
    elapsed_time_for_testing_ = elapsed_time;
  }

 private:
  double GetElapsedTimeInMs(base::TimeTicks);
  base::TimeDelta GetMinimumStepDuration();

  base::TimeTicks incremental_marking_start_time_;
  base::TimeDelta incremental_marking_time_so_far_;

  size_t incrementally_marked_bytes_ = 0;
  std::atomic_size_t concurrently_marked_bytes_{0};

  // Using -1 as sentinel to denote
  static constexpr double kNoSetElapsedTimeForTesting = -1;
  double elapsed_time_for_testing_ = kNoSetElapsedTimeForTesting;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_SCHEDULING_ORACLE_H_
