// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_

#include <stddef.h>

#include "base/atomicops.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Interface for observing changes to heap sizing.
class PLATFORM_EXPORT ThreadHeapStatsObserver {
 public:
  // Called upon allocating/releasing chunks of memory that contain objects.
  //
  // Must not trigger GC or allocate.
  virtual void IncreaseAllocatedSpace(size_t) = 0;
  virtual void DecreaseAllocatedSpace(size_t) = 0;

  // Called once per GC cycle with the accurate number of live |bytes|.
  //
  // Must not trigger GC or allocate.
  virtual void ResetAllocatedObjectSize(size_t bytes) = 0;

  // Called after observing at least
  // |ThreadHeapStatsCollector::kUpdateThreshold| changed bytes through
  // allocation or explicit free. Reports both, negative and positive
  // increments, to allow observer to decide whether absolute values or only the
  // deltas is interesting.
  //
  // May trigger GC but most not allocate.
  virtual void IncreaseAllocatedObjectSize(size_t) = 0;
  virtual void DecreaseAllocatedObjectSize(size_t) = 0;
};

#define FOR_ALL_SCOPES(V)             \
  V(AtomicPauseCompaction)            \
  V(AtomicPauseMarkEpilogue)          \
  V(AtomicPauseMarkPrologue)          \
  V(AtomicPauseMarkRoots)             \
  V(AtomicPauseMarkTransitiveClosure) \
  V(AtomicPauseSweepAndCompact)       \
  V(CompleteSweep)                    \
  V(IncrementalMarkingFinalize)       \
  V(IncrementalMarkingStartMarking)   \
  V(IncrementalMarkingStep)           \
  V(InvokePreFinalizers)              \
  V(LazySweepInIdle)                  \
  V(LazySweepOnAllocation)            \
  V(MarkInvokeEphemeronCallbacks)     \
  V(MarkProcessWorklist)              \
  V(MarkNotFullyConstructedObjects)   \
  V(MarkWeakProcessing)               \
  V(UnifiedMarkingStep)               \
  V(VisitCrossThreadPersistents)      \
  V(VisitDOMWrappers)                 \
  V(VisitPersistentRoots)             \
  V(VisitPersistents)                 \
  V(VisitStackRoots)

#define FOR_ALL_CONCURRENT_SCOPES(V) \
  V(ConcurrentMark)                  \
  V(ConcurrentSweep)

// Manages counters and statistics across garbage collection cycles.
//
// Usage:
//   ThreadHeapStatsCollector stats_collector;
//   stats_collector.NotifyMarkingStarted(<BlinkGC::GCReason>);
//   // Use tracer.
//   stats_collector.NotifySweepingCompleted();
//   // Previous event is available using stats_collector.previous().
class PLATFORM_EXPORT ThreadHeapStatsCollector {
  USING_FAST_MALLOC(ThreadHeapStatsCollector);

 public:
  // These ids will form human readable names when used in Scopes.
  enum Id {
#define DECLARE_ENUM(name) k##name,
    FOR_ALL_SCOPES(DECLARE_ENUM)
#undef DECLARE_ENUM
        kNumScopeIds,
  };

  enum ConcurrentId {
#define DECLARE_ENUM(name) k##name,
    FOR_ALL_CONCURRENT_SCOPES(DECLARE_ENUM)
#undef DECLARE_ENUM
        kNumConcurrentScopeIds
  };

  constexpr static const char* ToString(Id id) {
    switch (id) {
#define CASE(name) \
  case k##name:    \
    return "BlinkGC." #name;

      FOR_ALL_SCOPES(CASE)
#undef CASE
      default:
        NOTREACHED();
    }
    return nullptr;
  }

  constexpr static const char* ToString(ConcurrentId id) {
    switch (id) {
#define CASE(name) \
  case k##name:    \
    return "BlinkGC." #name;

      FOR_ALL_CONCURRENT_SCOPES(CASE)
#undef CASE
      default:
        NOTREACHED();
    }
    return nullptr;
  }

  enum TraceCategory { kEnabled, kDisabled, kDevTools };
  enum ScopeContext { kMutatorThread, kConcurrentThread };

  // Trace a particular scope. Will emit a trace event and record the time in
  // the corresponding ThreadHeapStatsCollector.
  template <TraceCategory trace_category = kDisabled,
            ScopeContext scope_category = kMutatorThread>
  class PLATFORM_EXPORT InternalScope {
    DISALLOW_NEW();
    DISALLOW_COPY_AND_ASSIGN(InternalScope);

    using IdType =
        std::conditional_t<scope_category == kMutatorThread, Id, ConcurrentId>;

   public:
    template <typename... Args>
    inline InternalScope(ThreadHeapStatsCollector* tracer,
                         IdType id,
                         Args... args)
        : tracer_(tracer), start_time_(base::TimeTicks::Now()), id_(id) {
      StartTrace(id, args...);
    }

    inline ~InternalScope() {
      StopTrace(id_);
      IncreaseScopeTime(id_);
    }

   private:
    constexpr static const char* TraceCategory() {
      switch (trace_category) {
        case kEnabled:
          return "blink_gc";
        case kDisabled:
          return TRACE_DISABLED_BY_DEFAULT("blink_gc");
        case kDevTools:
          return "blink_gc,devtools.timeline";
      }
    }

    void StartTrace(IdType id) {
      TRACE_EVENT_BEGIN0(TraceCategory(), ToString(id));
    }

    template <typename Value1>
    void StartTrace(IdType id, const char* k1, Value1 v1) {
      TRACE_EVENT_BEGIN1(TraceCategory(), ToString(id), k1, v1);
    }

    template <typename Value1, typename Value2>
    void StartTrace(IdType id,
                    const char* k1,
                    Value1 v1,
                    const char* k2,
                    Value2 v2) {
      TRACE_EVENT_BEGIN2(TraceCategory(), ToString(id), k1, v1, k2, v2);
    }

    void StopTrace(IdType id) {
      TRACE_EVENT_END0(TraceCategory(), ToString(id));
    }

    void IncreaseScopeTime(Id) {
      tracer_->IncreaseScopeTime(id_, base::TimeTicks::Now() - start_time_);
    }

    void IncreaseScopeTime(ConcurrentId) {
      tracer_->IncreaseConcurrentScopeTime(
          id_, base::TimeTicks::Now() - start_time_);
    }

    ThreadHeapStatsCollector* const tracer_;
    const base::TimeTicks start_time_;
    const IdType id_;
  };

  using Scope = InternalScope<kDisabled>;
  using EnabledScope = InternalScope<kEnabled>;
  using ConcurrentScope = InternalScope<kDisabled, kConcurrentThread>;
  using EnabledConcurrentScope = InternalScope<kEnabled, kConcurrentThread>;
  using DevToolsScope = InternalScope<kDevTools>;

  // BlinkGCInV8Scope keeps track of time spent in Blink's GC when called by V8.
  // This is necessary to avoid double-accounting of Blink's time when computing
  // the overall time (V8 + Blink) spent in GC on the main thread.
  class PLATFORM_EXPORT BlinkGCInV8Scope {
    DISALLOW_NEW();
    DISALLOW_COPY_AND_ASSIGN(BlinkGCInV8Scope);

   public:
    template <typename... Args>
    BlinkGCInV8Scope(ThreadHeapStatsCollector* tracer)
        : tracer_(tracer), start_time_(base::TimeTicks::Now()) {}

    ~BlinkGCInV8Scope() {
      if (tracer_)
        tracer_->gc_nested_in_v8_ += base::TimeTicks::Now() - start_time_;
    }

   private:
    ThreadHeapStatsCollector* const tracer_;
    const base::TimeTicks start_time_;
  };

  // POD to hold interesting data accumulated during a garbage collection cycle.
  // The event is always fully populated when looking at previous events but
  // is only be partially populated when looking at the current event. See
  // members on when they are available.
  //
  // Note that all getters include time for stand-alone as well as unified heap
  // GCs. E.g., |atomic_marking_time()| report the marking time of the atomic
  // phase, independent of whether the GC was a stand-alone or unified heap GC.
  struct PLATFORM_EXPORT Event {
    // Overall time spent in the GC cycle. This includes marking time as well as
    // sweeping time.
    base::TimeDelta gc_cycle_time() const;

    // Time spent in the final atomic pause of a GC cycle.
    base::TimeDelta atomic_pause_time() const;

    // Time spent in the final atomic pause for marking the heap.
    base::TimeDelta atomic_marking_time() const;

    // Time spent in the final atomic pause in sweeping and compacting the heap.
    base::TimeDelta atomic_sweep_and_compact_time() const;

    // Time spent incrementally marking the heap.
    base::TimeDelta incremental_marking_time() const;

    // Time spent in foreground tasks marking the heap.
    base::TimeDelta foreground_marking_time() const;

    // Time spent in background tasks marking the heap.
    base::TimeDelta background_marking_time() const;

    // Overall time spent marking the heap.
    base::TimeDelta marking_time() const;

    // Time spent in foreground tasks sweeping the heap.
    base::TimeDelta foreground_sweeping_time() const;

    // Time spent in background tasks sweeping the heap.
    base::TimeDelta background_sweeping_time() const;

    // Overall time spent sweeping the heap.
    base::TimeDelta sweeping_time() const;

    // Marking speed in bytes/s.
    double marking_time_in_bytes_per_second() const;

    // Marked bytes collected during sweeping.
    size_t marked_bytes = 0;
    size_t compaction_freed_bytes = 0;
    size_t compaction_freed_pages = 0;
    base::TimeDelta scope_data[kNumScopeIds];
    base::subtle::Atomic32 concurrent_scope_data[kNumConcurrentScopeIds]{0};
    BlinkGC::GCReason reason = static_cast<BlinkGC::GCReason>(0);
    size_t object_size_in_bytes_before_sweeping = 0;
    size_t allocated_space_in_bytes_before_sweeping = 0;
    size_t partition_alloc_bytes_before_sweeping = 0;
    double live_object_rate = 0;
    size_t wrapper_count_before_sweeping = 0;
    base::TimeDelta gc_nested_in_v8;
  };

  // Indicates a new garbage collection cycle.
  void NotifyMarkingStarted(BlinkGC::GCReason);

  // Indicates that marking of the current garbage collection cycle is
  // completed.
  void NotifyMarkingCompleted(size_t marked_bytes);

  // Indicates the end of a garbage collection cycle. This means that sweeping
  // is finished at this point.
  void NotifySweepingCompleted();

  void IncreaseScopeTime(Id id, base::TimeDelta time) {
    DCHECK(is_started_);
    current_.scope_data[id] += time;
  }

  void IncreaseConcurrentScopeTime(ConcurrentId id, base::TimeDelta time) {
    using Atomic32 = base::subtle::Atomic32;
    DCHECK(is_started_);
    const int64_t ms = time.InMicroseconds();
    DCHECK(ms <= std::numeric_limits<Atomic32>::max());
    base::subtle::NoBarrier_AtomicIncrement(&current_.concurrent_scope_data[id],
                                            static_cast<Atomic32>(ms));
  }

  void UpdateReason(BlinkGC::GCReason);
  void IncreaseCompactionFreedSize(size_t);
  void IncreaseCompactionFreedPages(size_t);
  void IncreaseAllocatedObjectSize(size_t);
  void DecreaseAllocatedObjectSize(size_t);
  void IncreaseAllocatedSpace(size_t);
  void DecreaseAllocatedSpace(size_t);
  void IncreaseWrapperCount(size_t);
  void DecreaseWrapperCount(size_t);
  void IncreaseCollectedWrapperCount(size_t);

  // Called by the GC when it hits a point where allocated memory may be
  // reported and garbage collection is possible. This is necessary, as
  // increments and decrements are reported as close to their actual
  // allocation/reclamation as possible.
  void AllocatedObjectSizeSafepoint();

  // Size of objects on the heap. Based on marked bytes in the previous cycle
  // and newly allocated bytes since the previous cycle.
  size_t object_size_in_bytes() const;

  // Estimated marking time in seconds. Based on marked bytes and mark speed in
  // the previous cycle assuming that the collection rate of the current cycle
  // is similar to the rate of the last GC.
  double estimated_marking_time_in_seconds() const;
  base::TimeDelta estimated_marking_time() const;

  size_t marked_bytes() const;
  base::TimeDelta marking_time_so_far() const;

  int64_t allocated_bytes_since_prev_gc() const;

  size_t allocated_space_bytes() const;

  size_t wrapper_count() const;
  size_t collected_wrapper_count() const;

  bool is_started() const { return is_started_; }

  // Statistics for the previously running garbage collection.
  const Event& previous() const { return previous_; }

  void RegisterObserver(ThreadHeapStatsObserver* observer);
  void UnregisterObserver(ThreadHeapStatsObserver* observer);

  void IncreaseAllocatedObjectSizeForTesting(size_t);
  void DecreaseAllocatedObjectSizeForTesting(size_t);

 private:
  // Observers are implemented using virtual calls. Avoid notifications below
  // reasonably interesting sizes.
  static constexpr int64_t kUpdateThreshold = 1024;

  // Invokes |callback| for all registered observers.
  template <typename Callback>
  void ForAllObservers(Callback callback);

  void AllocatedObjectSizeSafepointImpl();

  // Statistics for the currently running garbage collection. Note that the
  // Event may not be fully populated yet as some phase may not have been run.
  const Event& current() const { return current_; }

  Event current_;
  Event previous_;

  // Allocated bytes since the last garbage collection. These bytes are reset
  // after marking as they are accounted in marked_bytes then.
  int64_t allocated_bytes_since_prev_gc_ = 0;
  int64_t pos_delta_allocated_bytes_since_prev_gc_ = 0;
  int64_t neg_delta_allocated_bytes_since_prev_gc_ = 0;

  // Allocated space in bytes for all arenas.
  size_t allocated_space_bytes_ = 0;

  size_t wrapper_count_ = 0;
  size_t collected_wrapper_count_ = 0;

  bool is_started_ = false;

  // base::TimeDelta for RawScope. These don't need to be nested within a
  // garbage collection cycle to make them easier to use.
  base::TimeDelta gc_nested_in_v8_;

  Vector<ThreadHeapStatsObserver*> observers_;

  FRIEND_TEST_ALL_PREFIXES(ThreadHeapStatsCollectorTest, InitialEmpty);
  FRIEND_TEST_ALL_PREFIXES(ThreadHeapStatsCollectorTest, IncreaseScopeTime);
  FRIEND_TEST_ALL_PREFIXES(ThreadHeapStatsCollectorTest, StopResetsCurrent);
};

#undef FOR_ALL_SCOPES
#undef FOR_ALL_CONCURRENT_SCOPES

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_
