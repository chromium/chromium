// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"

#include <cmath>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

void ThreadHeapStatsCollector::IncreaseCompactionFreedSize(size_t bytes) {
  DCHECK(is_started_);
  current_.compaction_freed_bytes += bytes;
  current_.compaction_recorded_events = true;
}

void ThreadHeapStatsCollector::IncreaseCompactionFreedPages(size_t pages) {
  DCHECK(is_started_);
  current_.compaction_freed_pages += pages;
  current_.compaction_recorded_events = true;
}

void ThreadHeapStatsCollector::IncreaseAllocatedObjectSize(size_t bytes) {
  // The current GC may not have been started. This is ok as recording considers
  // the whole time range between garbage collections.
  pos_delta_allocated_bytes_since_prev_gc_ += bytes;
}

void ThreadHeapStatsCollector::IncreaseAllocatedObjectSizeForTesting(
    size_t bytes) {
  IncreaseAllocatedObjectSize(bytes);
  AllocatedObjectSizeSafepointImpl();
}

void ThreadHeapStatsCollector::DecreaseAllocatedObjectSize(size_t bytes) {
  // See IncreaseAllocatedObjectSize.
  neg_delta_allocated_bytes_since_prev_gc_ += bytes;
}

void ThreadHeapStatsCollector::DecreaseAllocatedObjectSizeForTesting(
    size_t bytes) {
  DecreaseAllocatedObjectSize(bytes);
  AllocatedObjectSizeSafepointImpl();
}

void ThreadHeapStatsCollector::AllocatedObjectSizeSafepoint() {
  if (std::abs(pos_delta_allocated_bytes_since_prev_gc_ -
               neg_delta_allocated_bytes_since_prev_gc_) > kUpdateThreshold) {
    AllocatedObjectSizeSafepointImpl();
  }
}

void ThreadHeapStatsCollector::AllocatedObjectSizeSafepointImpl() {
  allocated_bytes_since_prev_gc_ +=
      static_cast<int64_t>(pos_delta_allocated_bytes_since_prev_gc_) -
      static_cast<int64_t>(neg_delta_allocated_bytes_since_prev_gc_);

  // These observer methods may start or finalize GC. In case they trigger a
  // final GC pause, the delta counters are reset there and the following
  // observer calls are called with '0' updates.
  ForAllObservers([this](ThreadHeapStatsObserver* observer) {
    // Recompute delta here so that a GC finalization is able to clear the
    // delta for other observer calls.
    int64_t delta = pos_delta_allocated_bytes_since_prev_gc_ -
                    neg_delta_allocated_bytes_since_prev_gc_;
    if (delta < 0) {
      observer->DecreaseAllocatedObjectSize(static_cast<size_t>(-delta));
    } else {
      observer->IncreaseAllocatedObjectSize(static_cast<size_t>(delta));
    }
  });
  pos_delta_allocated_bytes_since_prev_gc_ = 0;
  neg_delta_allocated_bytes_since_prev_gc_ = 0;
}

void ThreadHeapStatsCollector::IncreaseAllocatedSpace(size_t bytes) {
  allocated_space_bytes_ += bytes;
  ForAllObservers([bytes](ThreadHeapStatsObserver* observer) {
    observer->IncreaseAllocatedSpace(bytes);
  });
}

void ThreadHeapStatsCollector::DecreaseAllocatedSpace(size_t bytes) {
  allocated_space_bytes_ -= bytes;
  ForAllObservers([bytes](ThreadHeapStatsObserver* observer) {
    observer->DecreaseAllocatedSpace(bytes);
  });
}

ThreadHeapStatsCollector::Event::Event() {
  static std::atomic<size_t> counter{0};
  unique_id = counter.fetch_add(1);
}

void ThreadHeapStatsCollector::NotifyMarkingStarted(
    BlinkGC::CollectionType collection_type,
    BlinkGC::GCReason reason,
    bool is_forced_gc) {
  DCHECK(!is_started_);
  DCHECK(current_.marking_time().is_zero());
  is_started_ = true;
  current_.reason = reason;
  current_.collection_type = collection_type;
  current_.is_forced_gc = is_forced_gc;
}

void ThreadHeapStatsCollector::NotifyMarkingCompleted(size_t marked_bytes) {
  allocated_bytes_since_prev_gc_ +=
      static_cast<int64_t>(pos_delta_allocated_bytes_since_prev_gc_) -
      static_cast<int64_t>(neg_delta_allocated_bytes_since_prev_gc_);
  current_.marked_bytes = marked_bytes;
  current_.object_size_in_bytes_before_sweeping = object_size_in_bytes();
  current_.allocated_space_in_bytes_before_sweeping = allocated_space_bytes();
  current_.partition_alloc_bytes_before_sweeping =
      WTF::Partitions::TotalSizeOfCommittedPages();
  allocated_bytes_since_prev_gc_ = 0;
  pos_delta_allocated_bytes_since_prev_gc_ = 0;
  neg_delta_allocated_bytes_since_prev_gc_ = 0;

  ForAllObservers([marked_bytes](ThreadHeapStatsObserver* observer) {
    observer->ResetAllocatedObjectSize(marked_bytes);
  });
}

void ThreadHeapStatsCollector::NotifySweepingCompleted() {
  is_started_ = false;
  current_.live_object_rate =
      current_.object_size_in_bytes_before_sweeping
          ? static_cast<double>(current().marked_bytes) /
                current_.object_size_in_bytes_before_sweeping
          : 0.0;
  current_.gc_nested_in_v8 = gc_nested_in_v8_;
  gc_nested_in_v8_ = base::TimeDelta();
  // Reset the current state.
  static_assert(std::is_trivially_copyable<Event>::value,
                "Event should be trivially copyable");
  previous_ = std::move(current_);
  current_ = Event();
}

void ThreadHeapStatsCollector::UpdateReason(BlinkGC::GCReason reason) {
  current_.reason = reason;
}

size_t ThreadHeapStatsCollector::object_size_in_bytes() const {
  DCHECK_GE(static_cast<int64_t>(previous().marked_bytes) +
                allocated_bytes_since_prev_gc_,
            0);
  return static_cast<size_t>(static_cast<int64_t>(previous().marked_bytes) +
                             allocated_bytes_since_prev_gc_);
}

base::TimeDelta ThreadHeapStatsCollector::Event::roots_marking_time() const {
  return scope_data[kVisitRoots];
}

base::TimeDelta ThreadHeapStatsCollector::Event::incremental_marking_time()
    const {
  return scope_data[kIncrementalMarkingStartMarking] +
         scope_data[kIncrementalMarkingStep] + scope_data[kUnifiedMarkingStep];
}

base::TimeDelta
ThreadHeapStatsCollector::Event::worklist_processing_time_foreground() const {
  return scope_data[kMarkProcessWorklists];
}

base::TimeDelta ThreadHeapStatsCollector::Event::flushing_v8_references_time()
    const {
  return scope_data[kMarkFlushV8References];
}

base::TimeDelta ThreadHeapStatsCollector::Event::atomic_marking_time() const {
  return scope_data[kAtomicPauseMarkPrologue] +
         scope_data[kAtomicPauseMarkRoots] +
         scope_data[kAtomicPauseMarkTransitiveClosure] +
         scope_data[kAtomicPauseMarkEpilogue];
}

base::TimeDelta ThreadHeapStatsCollector::Event::atomic_sweep_and_compact_time()
    const {
  return scope_data[ThreadHeapStatsCollector::kAtomicPauseSweepAndCompact];
}

base::TimeDelta ThreadHeapStatsCollector::Event::foreground_marking_time()
    const {
  return incremental_marking_time() + atomic_marking_time();
}

base::TimeDelta ThreadHeapStatsCollector::Event::background_marking_time()
    const {
  return base::TimeDelta::FromMicroseconds(base::subtle::NoBarrier_Load(
      &concurrent_scope_data[kConcurrentMarkingStep]));
}

base::TimeDelta ThreadHeapStatsCollector::Event::marking_time() const {
  return foreground_marking_time() + background_marking_time();
}

base::TimeDelta ThreadHeapStatsCollector::Event::gc_cycle_time() const {
  // Note that scopes added here also have to have a proper BlinkGCInV8Scope
  // scope if they are nested in a V8 scope.
  return incremental_marking_time() + atomic_marking_time() +
         atomic_sweep_and_compact_time() + foreground_sweeping_time();
}

base::TimeDelta ThreadHeapStatsCollector::Event::atomic_pause_time() const {
  return atomic_marking_time() + atomic_sweep_and_compact_time();
}

base::TimeDelta ThreadHeapStatsCollector::Event::foreground_sweeping_time()
    const {
  return scope_data[kCompleteSweep] + scope_data[kLazySweepInIdle] +
         scope_data[kLazySweepOnAllocation];
}

base::TimeDelta ThreadHeapStatsCollector::Event::background_sweeping_time()
    const {
  return base::TimeDelta::FromMicroseconds(
      concurrent_scope_data[kConcurrentSweepingStep]);
}

base::TimeDelta ThreadHeapStatsCollector::Event::sweeping_time() const {
  return foreground_sweeping_time() + background_sweeping_time();
}

int64_t ThreadHeapStatsCollector::allocated_bytes_since_prev_gc() const {
  return allocated_bytes_since_prev_gc_;
}

size_t ThreadHeapStatsCollector::marked_bytes() const {
  return current_.marked_bytes;
}

base::TimeDelta ThreadHeapStatsCollector::marking_time_so_far() const {
  return current_.marking_time();
}

base::TimeDelta ThreadHeapStatsCollector::worklist_processing_time_foreground()
    const {
  return current_.worklist_processing_time_foreground();
}

base::TimeDelta ThreadHeapStatsCollector::flushing_v8_references_time() const {
  return current_.flushing_v8_references_time();
}

size_t ThreadHeapStatsCollector::allocated_space_bytes() const {
  return allocated_space_bytes_;
}

void ThreadHeapStatsCollector::RegisterObserver(
    ThreadHeapStatsObserver* observer) {
  DCHECK(!observers_.Contains(observer));
  observers_.push_back(observer);
}

void ThreadHeapStatsCollector::UnregisterObserver(
    ThreadHeapStatsObserver* observer) {
  wtf_size_t index = observers_.Find(observer);
  DCHECK_NE(WTF::kNotFound, index);
  observers_.EraseAt(index);
}

template <typename Callback>
void ThreadHeapStatsCollector::ForAllObservers(Callback callback) {
  for (ThreadHeapStatsObserver* observer : observers_) {
    callback(observer);
  }
}

}  // namespace blink
