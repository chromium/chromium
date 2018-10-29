// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

void ThreadHeapStatsCollector::IncreaseMarkedObjectSize(size_t bytes) {
  DCHECK(is_started_);
  current_.marked_bytes += bytes;
}

void ThreadHeapStatsCollector::IncreaseCompactionFreedSize(size_t bytes) {
  DCHECK(is_started_);
  current_.compaction_freed_bytes += bytes;
}

void ThreadHeapStatsCollector::IncreaseCompactionFreedPages(size_t pages) {
  DCHECK(is_started_);
  current_.compaction_freed_pages += pages;
}

void ThreadHeapStatsCollector::IncreaseAllocatedObjectSize(size_t bytes) {
  // The current GC may not have been started. This is ok as recording considers
  // the whole time range between garbage collections.
  allocated_bytes_since_prev_gc_ += bytes;
}

void ThreadHeapStatsCollector::DecreaseAllocatedObjectSize(size_t bytes) {
  // See IncreaseAllocatedObjectSize.
  allocated_bytes_since_prev_gc_ -= bytes;
}

void ThreadHeapStatsCollector::IncreaseAllocatedSpace(size_t bytes) {
  allocated_space_bytes_ += bytes;
}

void ThreadHeapStatsCollector::DecreaseAllocatedSpace(size_t bytes) {
  allocated_space_bytes_ -= bytes;
}

void ThreadHeapStatsCollector::IncreaseWrapperCount(size_t count) {
  wrapper_count_ += count;
}

void ThreadHeapStatsCollector::DecreaseWrapperCount(size_t count) {
  wrapper_count_ -= count;
}

void ThreadHeapStatsCollector::IncreaseCollectedWrapperCount(size_t count) {
  collected_wrapper_count_ += count;
}

void ThreadHeapStatsCollector::NotifyMarkingStarted(BlinkGC::GCReason reason) {
  DCHECK(!is_started_);
  DCHECK_EQ(0.0, current_.marking_time_in_ms());
  is_started_ = true;
  current_.reason = reason;
}

void ThreadHeapStatsCollector::NotifyMarkingCompleted() {
  current_.object_size_in_bytes_before_sweeping = object_size_in_bytes();
  current_.allocated_space_in_bytes_before_sweeping = allocated_space_bytes();
  current_.partition_alloc_bytes_before_sweeping =
      WTF::Partitions::TotalSizeOfCommittedPages();
  current_.wrapper_count_before_sweeping = wrapper_count_;
  allocated_bytes_since_prev_gc_ = 0;
  collected_wrapper_count_ = 0;
}

void ThreadHeapStatsCollector::NotifySweepingCompleted() {
  is_started_ = false;
  current_.live_object_rate =
      current_.object_size_in_bytes_before_sweeping
          ? static_cast<double>(current().marked_bytes) /
                current_.object_size_in_bytes_before_sweeping
          : 0.0;
  current_.gc_nested_in_v8_ = gc_nested_in_v8_;
  previous_ = std::move(current_);
  // Reset the current state.
  static_assert(!std::is_polymorphic<Event>::value,
                "Event should not be polymorphic");
  memset(&current_, 0, sizeof(current_));
  gc_nested_in_v8_ = TimeDelta();
}

void ThreadHeapStatsCollector::UpdateReason(BlinkGC::GCReason reason) {
  current_.reason = reason;
}

size_t ThreadHeapStatsCollector::object_size_in_bytes() const {
  return previous().marked_bytes + allocated_bytes_since_prev_gc_;
}

double ThreadHeapStatsCollector::estimated_marking_time_in_seconds() const {
  // Assume 8ms time for an initial heap. 8 ms is long enough for low-end mobile
  // devices to mark common real-world object graphs.
  constexpr double kInitialMarkingTimeInSeconds = 0.008;

  const double prev_marking_speed =
      previous().marking_time_in_bytes_per_second();
  return prev_marking_speed ? prev_marking_speed * object_size_in_bytes()
                            : kInitialMarkingTimeInSeconds;
}

TimeDelta ThreadHeapStatsCollector::estimated_marking_time() const {
  return TimeDelta::FromSecondsD(estimated_marking_time_in_seconds());
}

double ThreadHeapStatsCollector::Event::marking_time_in_ms() const {
  return (scope_data[kIncrementalMarkingStartMarking] +
          scope_data[kIncrementalMarkingStep] +
          scope_data[kIncrementalMarkingFinalizeMarking] +
          scope_data[kAtomicPhaseMarking])
      .InMillisecondsF();
}

double ThreadHeapStatsCollector::Event::marking_time_in_bytes_per_second()
    const {
  return marked_bytes ? marking_time_in_ms() / 1000 / marked_bytes : 0.0;
}

TimeDelta ThreadHeapStatsCollector::Event::sweeping_time() const {
  return scope_data[kCompleteSweep] + scope_data[kEagerSweep] +
         scope_data[kLazySweepInIdle] + scope_data[kLazySweepOnAllocation];
}

size_t ThreadHeapStatsCollector::allocated_bytes_since_prev_gc() const {
  return allocated_bytes_since_prev_gc_;
}

size_t ThreadHeapStatsCollector::allocated_space_bytes() const {
  return allocated_space_bytes_;
}

size_t ThreadHeapStatsCollector::collected_wrapper_count() const {
  return collected_wrapper_count_;
}

size_t ThreadHeapStatsCollector::wrapper_count() const {
  return wrapper_count_;
}

}  // namespace blink
