/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/heap/thread_state.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <memory>

#include "base/atomicops.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task_runner.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/address_cache.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/cancelable_task_scheduler.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/page_pool.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_controller.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/stack_util.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#endif

#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#endif

#if defined(OS_FREEBSD)
#include <pthread_np.h>
#endif

namespace blink {

WTF::ThreadSpecific<ThreadState*>* ThreadState::thread_specific_ = nullptr;
uint8_t ThreadState::main_thread_state_storage_[sizeof(ThreadState)];

namespace {

// Concurrent marking should stop every once in a while to flush private
// segments to v8 marking worklist. It should also stop to avoid priority
// inversion.
//
// TODO(omerkatz): What is a good value to set here?
constexpr base::TimeDelta kConcurrentMarkingStepDuration =
    base::TimeDelta::FromMilliseconds(2);
// Number of concurrent marking tasks to use.
//
// TODO(omerkatz): kNumberOfMarkingTasks should be set heuristically
// instead of a constant.
constexpr uint8_t kNumberOfConcurrentMarkingTasks = 1u;

constexpr size_t kMaxTerminationGCLoops = 20;

// Helper function to convert a byte count to a KB count, capping at
// INT_MAX if the number is larger than that.
constexpr base::Histogram::Sample CappedSizeInKB(size_t size_in_bytes) {
  return base::saturated_cast<base::Histogram::Sample>(size_in_bytes / 1024);
}

class WorkerPoolTaskRunner : public base::TaskRunner {
 public:
  bool PostDelayedTask(const base::Location& location,
                       base::OnceClosure task,
                       base::TimeDelta) override {
    worker_pool::PostTask(location, WTF::CrossThreadBindOnce(std::move(task)));
    return true;
  }

  bool RunsTasksInCurrentSequence() const override { return false; }
};

}  // namespace

constexpr base::TimeDelta ThreadState::kDefaultIncrementalMarkingStepDuration;

class ThreadState::IncrementalMarkingScheduler {
 public:
  explicit IncrementalMarkingScheduler(ThreadState* thread_state)
      : thread_state_(thread_state) {}

  // Starts incremental marking with further scheduled steps.
  void Start(BlinkGC::GCReason reason) {
    Init(reason);
    thread_state_->IncrementalMarkingStart(reason_);
    ScheduleTask();
  }

  // Cancels incremental marking task in case there is any pending.
  void Cancel() { task_.Cancel(); }

 private:
  void Init(BlinkGC::GCReason reason) {
    DCHECK(!task_.IsActive());
    reason_ = reason;
    next_incremental_marking_step_duration_ =
        kDefaultIncrementalMarkingStepDuration;
    previous_incremental_marking_time_left_ = base::TimeDelta::Max();
  }

  void ScheduleTask() {
    // Reassigning to the task will cancel the currently scheduled one.
    task_ = PostNonNestableCancellableTask(
        *ThreadScheduler::Current()->V8TaskRunner(), FROM_HERE,
        WTF::Bind(&IncrementalMarkingScheduler::Dispatch,
                  WTF::Unretained(this)));
  }

  void Dispatch() {
    switch (thread_state_->GetGCState()) {
      case ThreadState::kIncrementalGCScheduled:
        thread_state_->IncrementalMarkingStart(reason_);
        ScheduleTask();
        break;
      case ThreadState::kIncrementalMarkingStepScheduled:
        thread_state_->IncrementalMarkingStep(
            BlinkGC::kNoHeapPointersOnStack,
            next_incremental_marking_step_duration_);
        UpdateIncrementalMarkingStepDuration();
        ScheduleTask();
        break;
      case ThreadState::kIncrementalMarkingFinalizeScheduled:
        thread_state_->IncrementalMarkingFinalize();
        break;
      default:
        break;
    }
  }

  void UpdateIncrementalMarkingStepDuration() {
    const ThreadHeap& heap = thread_state_->Heap();
    base::TimeDelta time_left =
        heap.stats_collector()->estimated_marking_time() -
        heap.stats_collector()->marking_time_so_far();
    // Increase step size if estimated time left is increasing.
    if (previous_incremental_marking_time_left_ < time_left) {
      constexpr double ratio = 2.0;
      next_incremental_marking_step_duration_ *= ratio;
    }
    previous_incremental_marking_time_left_ = time_left;
  }

  ThreadState* thread_state_;
  BlinkGC::GCReason reason_;
  base::TimeDelta next_incremental_marking_step_duration_ =
      kDefaultIncrementalMarkingStepDuration;
  base::TimeDelta previous_incremental_marking_time_left_ =
      base::TimeDelta::Max();
  TaskHandle task_;
};

ThreadState::ThreadState()
    : thread_(CurrentThread()),
      persistent_region_(std::make_unique<PersistentRegion>()),
      weak_persistent_region_(std::make_unique<PersistentRegion>()),
      start_of_stack_(reinterpret_cast<Address*>(WTF::GetStackStart())),
#if defined(ADDRESS_SANITIZER)
      asan_fake_stack_(__asan_get_current_fake_stack()),
#endif
      incremental_marking_scheduler_(
          std::make_unique<IncrementalMarkingScheduler>(this)),
      marker_scheduler_(std::make_unique<CancelableTaskScheduler>(
          base::MakeRefCounted<WorkerPoolTaskRunner>())),
      sweeper_scheduler_(std::make_unique<CancelableTaskScheduler>(
          base::MakeRefCounted<WorkerPoolTaskRunner>())) {
  DCHECK(CheckThread());
  DCHECK(!**thread_specific_);
  **thread_specific_ = this;
  heap_ = std::make_unique<ThreadHeap>(this);
}

ThreadState::~ThreadState() {
  DCHECK(CheckThread());
  if (IsMainThread())
    DCHECK_EQ(0u, Heap().stats_collector()->allocated_space_bytes());
  CHECK(GetGCState() == ThreadState::kNoGCScheduled);

  **thread_specific_ = nullptr;
}

ThreadState* ThreadState::AttachMainThread() {
  thread_specific_ = new WTF::ThreadSpecific<ThreadState*>();
  return new (main_thread_state_storage_) ThreadState();
}

ThreadState* ThreadState::AttachCurrentThread() {
  return new ThreadState();
}

void ThreadState::DetachCurrentThread() {
  ThreadState* state = Current();
  DCHECK(!state->IsMainThread());
  state->RunTerminationGC();
  delete state;
}

void ThreadState::AttachToIsolate(
    v8::Isolate* isolate,
    V8TraceRootsCallback v8_trace_roots,
    V8BuildEmbedderGraphCallback v8_build_embedder_graph) {
  DCHECK(isolate);
  isolate_ = isolate;
  v8_trace_roots_ = v8_trace_roots;
  v8_build_embedder_graph_ = v8_build_embedder_graph;
  unified_heap_controller_.reset(new UnifiedHeapController(this));
  isolate_->SetEmbedderHeapTracer(unified_heap_controller_.get());
  if (v8::HeapProfiler* profiler = isolate->GetHeapProfiler()) {
    profiler->AddBuildEmbedderGraphCallback(v8_build_embedder_graph, nullptr);
  }
}

void ThreadState::DetachFromIsolate() {
  if (isolate_) {
    isolate_->SetEmbedderHeapTracer(nullptr);
    if (v8::HeapProfiler* profiler = isolate_->GetHeapProfiler()) {
      profiler->RemoveBuildEmbedderGraphCallback(v8_build_embedder_graph_,
                                                 nullptr);
    }
  }
  isolate_ = nullptr;
  v8_trace_roots_ = nullptr;
  v8_build_embedder_graph_ = nullptr;
  unified_heap_controller_.reset();
}

void ThreadState::RunTerminationGC() {
  DCHECK(!IsMainThread());
  DCHECK(CheckThread());

  FinishIncrementalMarkingIfRunning(BlinkGC::kNoHeapPointersOnStack,
                                    BlinkGC::kIncrementalAndConcurrentMarking,
                                    BlinkGC::kConcurrentAndLazySweeping,
                                    BlinkGC::GCReason::kThreadTerminationGC);

  // Finish sweeping.
  CompleteSweep();

  ReleaseStaticPersistentNodes();

  // PrepareForThreadStateTermination removes strong references so no need to
  // call it on CrossThreadWeakPersistentRegion.
  ProcessHeap::GetCrossThreadPersistentRegion()
      .PrepareForThreadStateTermination(this);

  // Do thread local GC's as long as the count of thread local Persistents
  // changes and is above zero.
  int old_count = -1;
  int current_count = GetPersistentRegion()->NodesInUse();
  DCHECK_GE(current_count, 0);
  while (current_count != old_count) {
    CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                   BlinkGC::kEagerSweeping,
                   BlinkGC::GCReason::kThreadTerminationGC);
    // Release the thread-local static persistents that were
    // instantiated while running the termination GC.
    ReleaseStaticPersistentNodes();
    old_count = current_count;
    current_count = GetPersistentRegion()->NodesInUse();
  }

  // We should not have any persistents left when getting to this point,
  // if we have it is a bug, and we have a reference cycle or a missing
  // RegisterAsStaticReference. Clearing out all the Persistents will avoid
  // stale pointers and gets them reported as nullptr dereferences.
  if (current_count) {
    for (size_t i = 0;
         i < kMaxTerminationGCLoops && GetPersistentRegion()->NodesInUse();
         i++) {
      GetPersistentRegion()->PrepareForThreadStateTermination(this);
      CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kEagerSweeping,
                     BlinkGC::GCReason::kThreadTerminationGC);
    }
  }

  CHECK(!GetPersistentRegion()->NodesInUse());

  // All of pre-finalizers should be consumed.
  DCHECK(ordered_pre_finalizers_.empty());
  CHECK_EQ(GetGCState(), kNoGCScheduled);

  Heap().RemoveAllPages();
}

NO_SANITIZE_ADDRESS
void ThreadState::VisitAsanFakeStackForPointer(MarkingVisitor* visitor,
                                               Address ptr,
                                               Address* start_of_stack,
                                               Address* end_of_stack) {
#if defined(ADDRESS_SANITIZER)
  Address* fake_frame_start = nullptr;
  Address* fake_frame_end = nullptr;
  Address* maybe_fake_frame = reinterpret_cast<Address*>(ptr);
  Address* real_frame_for_fake_frame = reinterpret_cast<Address*>(
      __asan_addr_is_in_fake_stack(asan_fake_stack_, maybe_fake_frame,
                                   reinterpret_cast<void**>(&fake_frame_start),
                                   reinterpret_cast<void**>(&fake_frame_end)));
  if (real_frame_for_fake_frame) {
    // This is a fake frame from the asan fake stack.
    if (real_frame_for_fake_frame > end_of_stack &&
        start_of_stack > real_frame_for_fake_frame) {
      // The real stack address for the asan fake frame is
      // within the stack range that we need to scan so we need
      // to visit the values in the fake frame.
      for (Address* p = fake_frame_start; p < fake_frame_end; ++p)
        heap_->CheckAndMarkPointer(visitor, *p);
    }
  }
#endif  // ADDRESS_SANITIZER
}

// Stack scanning may overrun the bounds of local objects and/or race with
// other threads that use this stack.
NO_SANITIZE_ADDRESS
NO_SANITIZE_HWADDRESS
NO_SANITIZE_THREAD
void ThreadState::VisitStack(MarkingVisitor* visitor, Address* end_of_stack) {
  DCHECK_EQ(current_gc_data_.stack_state, BlinkGC::kHeapPointersOnStack);

  // Ensure that current is aligned by address size otherwise the loop below
  // will read past start address.
  Address* current = reinterpret_cast<Address*>(
      reinterpret_cast<intptr_t>(end_of_stack) & ~(sizeof(Address) - 1));

  for (; current < start_of_stack_; ++current) {
    Address ptr = *current;
#if defined(MEMORY_SANITIZER)
    // |ptr| may be uninitialized by design. Mark it as initialized to keep
    // MSan from complaining.
    // Note: it may be tempting to get rid of |ptr| and simply use |current|
    // here, but that would be incorrect. We intentionally use a local
    // variable because we don't want to unpoison the original stack.
    __msan_unpoison(&ptr, sizeof(ptr));
#endif
    heap_->CheckAndMarkPointer(visitor, ptr);
    VisitAsanFakeStackForPointer(visitor, ptr, start_of_stack_, end_of_stack);
  }
}

void ThreadState::VisitDOMWrappers(Visitor* visitor) {
  if (v8_trace_roots_) {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kVisitDOMWrappers);
    v8_trace_roots_(isolate_, visitor);
  }
}

void ThreadState::VisitPersistents(Visitor* visitor) {
  ThreadHeapStatsCollector::Scope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kVisitPersistentRoots);
  {
    ThreadHeapStatsCollector::Scope inner_stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kVisitCrossThreadPersistents);
    ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
    ProcessHeap::GetCrossThreadPersistentRegion().TraceNodes(visitor);
  }
  {
    ThreadHeapStatsCollector::Scope inner_stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kVisitPersistents);
    persistent_region_->TraceNodes(visitor);
  }
}

void ThreadState::VisitWeakPersistents(Visitor* visitor) {
  ProcessHeap::GetCrossThreadWeakPersistentRegion().TraceNodes(visitor);
  weak_persistent_region_->TraceNodes(visitor);
}

void ThreadState::WillStartV8GC(BlinkGC::V8GCType gc_type) {
  // Finish Oilpan's complete sweeping before running a V8 major GC.
  // This will let the GC collect more V8 objects.
  if (gc_type == BlinkGC::kV8MajorGC)
    CompleteSweep();
}

void ThreadState::ScheduleForcedGCForTesting() {
  DCHECK(CheckThread());
  CompleteSweep();
  SetGCState(kForcedGCForTestingScheduled);
}

void ThreadState::ScheduleGCIfNeeded() {
  VLOG(2) << "[state:" << this << "] ScheduleGCIfNeeded";
  DCHECK(CheckThread());

  // Allocation is allowed during sweeping, but those allocations should not
  // trigger nested GCs.
  if (IsGCForbidden() || SweepForbidden())
    return;

  // This method should not call out to V8 during unified heap garbage
  // collections. Specifically, reporting memory to V8 may trigger a marking
  // step which is not allowed during construction of an object. The reason is
  // that a parent object's constructor is potentially being invoked which may
  // have already published the object. In that case the object may be colored
  // black in a v8 marking step which invalidates the assumption that write
  // barriers may be avoided when constructing an object as it is white.
  if (IsUnifiedGCMarkingInProgress())
    return;

  if (GetGCState() == kNoGCScheduled &&
      base::FeatureList::IsEnabled(
          blink::features::kBlinkHeapIncrementalMarkingStress)) {
    VLOG(2) << "[state:" << this << "] "
            << "ScheduleGCIfNeeded: Scheduled incremental marking for testing";
    StartIncrementalMarking(BlinkGC::GCReason::kForcedGCForTesting);
    return;
  }
}

ThreadState* ThreadState::FromObject(const void* object) {
  DCHECK(object);
  BasePage* page = PageFromObject(object);
  DCHECK(page);
  DCHECK(page->Arena());
  return page->Arena()->GetThreadState();
}

void ThreadState::PerformIdleLazySweep(base::TimeTicks deadline) {
  DCHECK(CheckThread());

  // If we are not in a sweeping phase, there is nothing to do here.
  if (!IsSweepingInProgress())
    return;

  // This check is here to prevent performIdleLazySweep() from being called
  // recursively. I'm not sure if it can happen but it would be safer to have
  // the check just in case.
  if (SweepForbidden())
    return;

  RUNTIME_CALL_TIMER_SCOPE_IF_ISOLATE_EXISTS(
      GetIsolate(), RuntimeCallStats::CounterId::kPerformIdleLazySweep);

  bool sweep_completed = false;
  {
    AtomicPauseScope atomic_pause_scope(this);
    ScriptForbiddenScope script_forbidden_scope;
    SweepForbiddenScope scope(this);
    ThreadHeapStatsCollector::EnabledScope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kLazySweepInIdle,
        "idleDeltaInSeconds", (deadline - base::TimeTicks::Now()).InSecondsF());
    sweep_completed =
        Heap().AdvanceSweep(ThreadHeap::SweepingType::kMutator, deadline);
    // We couldn't finish the sweeping within the deadline.
    // We request another idle task for the remaining sweeping.
    if (sweep_completed) {
      SynchronizeAndFinishConcurrentSweeping();
    } else {
      ScheduleIdleLazySweep();
    }
  }

  if (sweep_completed) {
    NotifySweepDone();
  }
}

void ThreadState::PerformConcurrentSweep() {
  VLOG(2) << "[state:" << this << "] [threadid:" << CurrentThread() << "] "
          << "ConcurrentSweep";
  // As opposed to PerformIdleLazySweep, this function doesn't receive deadline
  // from the scheduler, but defines it itself.
  static constexpr base::TimeDelta kConcurrentSweepStepDuration =
      base::TimeDelta::FromMilliseconds(2);
  // Concurrent sweeper doesn't call finalizers - this guarantees that sweeping
  // is not called recursively.
  ThreadHeapStatsCollector::EnabledConcurrentScope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kConcurrentSweep);
  const bool finished = Heap().AdvanceSweep(
      ThreadHeap::SweepingType::kConcurrent,
      base::TimeTicks::Now() + kConcurrentSweepStepDuration);
  if (!finished) {
    // Reschedule itself. It is safe even if the task timeouts and reposts
    // itself while the mutator thread is waiting on CancelAndWait(). The
    // mutator thread will simply wake up and cancel the newly post task itself.
    sweeper_scheduler_->ScheduleTask(
        WTF::CrossThreadBindOnce(&ThreadState::PerformConcurrentSweep,
                                 WTF::CrossThreadUnretained(this)));
  }
}

void ThreadState::StartIncrementalMarking(BlinkGC::GCReason reason) {
  DCHECK(CheckThread());
  // Schedule an incremental GC only when no GC is scheduled. Otherwise, already
  // scheduled GCs should be prioritized.
  if (GetGCState() != kNoGCScheduled) {
    return;
  }
  CompleteSweep();
  reason_for_scheduled_gc_ = reason;
  SetGCState(kIncrementalGCScheduled);
  incremental_marking_scheduler_->Start(reason);
}

void ThreadState::ScheduleIdleLazySweep() {
  ThreadScheduler::Current()->PostIdleTask(
      FROM_HERE,
      WTF::Bind(&ThreadState::PerformIdleLazySweep, WTF::Unretained(this)));
}

void ThreadState::ScheduleConcurrentAndLazySweep() {
  ScheduleIdleLazySweep();

  if (!base::FeatureList::IsEnabled(
          blink::features::kBlinkHeapConcurrentSweeping)) {
    return;
  }

  static constexpr size_t kNumberOfSweepingTasks = 1u;

  for (size_t i = 0; i < kNumberOfSweepingTasks; ++i) {
    sweeper_scheduler_->ScheduleTask(
        WTF::CrossThreadBindOnce(&ThreadState::PerformConcurrentSweep,
                                 WTF::CrossThreadUnretained(this)));
  }
}

void ThreadState::SchedulePreciseGC() {
  DCHECK(CheckThread());
  CompleteSweep();
  SetGCState(kPreciseGCScheduled);
}

namespace {

#define UNEXPECTED_GCSTATE(s)                                   \
  case ThreadState::s:                                          \
    LOG(FATAL) << "Unexpected transition while in GCState " #s; \
    return

void UnexpectedGCState(ThreadState::GCState gc_state) {
  switch (gc_state) {
    UNEXPECTED_GCSTATE(kNoGCScheduled);
    UNEXPECTED_GCSTATE(kPreciseGCScheduled);
    UNEXPECTED_GCSTATE(kForcedGCForTestingScheduled);
    UNEXPECTED_GCSTATE(kIncrementalMarkingStepPaused);
    UNEXPECTED_GCSTATE(kIncrementalMarkingStepScheduled);
    UNEXPECTED_GCSTATE(kIncrementalMarkingFinalizeScheduled);
    UNEXPECTED_GCSTATE(kIncrementalGCScheduled);
  }
}

#undef UNEXPECTED_GCSTATE

}  // namespace

#define VERIFY_STATE_TRANSITION(condition) \
  if (UNLIKELY(!(condition)))              \
  UnexpectedGCState(gc_state_)

void ThreadState::SetGCState(GCState gc_state) {
  switch (gc_state) {
    case kNoGCScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(
          gc_state_ == kNoGCScheduled || gc_state_ == kPreciseGCScheduled ||
          gc_state_ == kForcedGCForTestingScheduled ||
          gc_state_ == kIncrementalMarkingStepPaused ||
          gc_state_ == kIncrementalMarkingStepScheduled ||
          gc_state_ == kIncrementalMarkingFinalizeScheduled ||
          gc_state_ == kIncrementalGCScheduled);
      break;
    case kIncrementalMarkingStepScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled ||
                              gc_state_ == kIncrementalMarkingStepScheduled ||
                              gc_state_ == kIncrementalGCScheduled);
      break;
    case kIncrementalMarkingFinalizeScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kIncrementalMarkingStepScheduled);
      break;
    case kForcedGCForTestingScheduled:
    case kPreciseGCScheduled:
      DCHECK(CheckThread());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled ||
                              gc_state_ == kIncrementalMarkingStepPaused ||
                              gc_state_ == kIncrementalMarkingStepScheduled ||
                              gc_state_ ==
                                  kIncrementalMarkingFinalizeScheduled ||
                              gc_state_ == kPreciseGCScheduled ||
                              gc_state_ == kForcedGCForTestingScheduled ||
                              gc_state_ == kIncrementalGCScheduled);
      break;
    case kIncrementalGCScheduled:
      DCHECK(CheckThread());
      DCHECK(!IsMarkingInProgress());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled);
      break;
    case kIncrementalMarkingStepPaused:
      DCHECK(CheckThread());
      DCHECK(IsMarkingInProgress());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(gc_state_ == kIncrementalMarkingStepScheduled);
      break;
    default:
      NOTREACHED();
  }
  gc_state_ = gc_state;
}

#undef VERIFY_STATE_TRANSITION

void ThreadState::SetGCPhase(GCPhase gc_phase) {
  switch (gc_phase) {
    case GCPhase::kNone:
      DCHECK_EQ(gc_phase_, GCPhase::kSweeping);
      break;
    case GCPhase::kMarking:
      DCHECK_EQ(gc_phase_, GCPhase::kNone);
      break;
    case GCPhase::kSweeping:
      DCHECK_EQ(gc_phase_, GCPhase::kMarking);
      break;
  }
  gc_phase_ = gc_phase;
}

void ThreadState::RunScheduledGC(BlinkGC::StackState stack_state) {
  DCHECK(CheckThread());
  if (stack_state != BlinkGC::kNoHeapPointersOnStack)
    return;

  // If a safe point is entered while initiating a GC, we clearly do
  // not want to do another as part of that -- the safe point is only
  // entered after checking if a scheduled GC ought to run first.
  // Prevent that from happening by marking GCs as forbidden while
  // one is initiated and later running.
  if (IsGCForbidden())
    return;

  switch (GetGCState()) {
    case kForcedGCForTestingScheduled:
      CollectAllGarbageForTesting();
      break;
    case kPreciseGCScheduled:
      CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kConcurrentAndLazySweeping,
                     BlinkGC::GCReason::kPreciseGC);
      break;
    default:
      break;
  }
}

void ThreadState::AtomicPauseMarkPrologue(BlinkGC::StackState stack_state,
                                          BlinkGC::MarkingType marking_type,
                                          BlinkGC::GCReason reason) {
  ThreadHeapStatsCollector::EnabledScope mark_prologue_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkPrologue, "epoch", gc_age_,
      "forced", IsForcedGC(reason));
  EnterAtomicPause();
  EnterNoAllocationScope();
  EnterGCForbiddenScope();
  // Compaction needs to be canceled when incremental marking ends with a
  // conservative GC.
  if (stack_state == BlinkGC::kHeapPointersOnStack)
    Heap().Compaction()->Cancel();

  if (IsMarkingInProgress()) {
    // Incremental marking is already in progress. Only update the state
    // that is necessary to update.
    SetGCState(kNoGCScheduled);
    if (base::FeatureList::IsEnabled(
            blink::features::kBlinkHeapConcurrentMarking)) {
      // Stop concurrent markers
      marker_scheduler_->CancelAndWait();
      active_markers_ = 0;
      available_concurrent_marking_task_ids_.clear();
    }
#if DCHECK_IS_ON()
    MarkingWorklist* worklist = Heap().GetMarkingWorklist();
    for (int concurrent_task = WorklistTaskId::ConcurrentThreadBase;
         concurrent_task < worklist->num_tasks(); ++concurrent_task) {
      DCHECK(worklist->IsLocalEmpty(concurrent_task));
    }
#endif  // DCHECK_IS_ON()
    DisableIncrementalMarkingBarrier();
    current_gc_data_.reason = reason;
    current_gc_data_.stack_state = stack_state;
    Heap().stats_collector()->UpdateReason(reason);
  } else {
    MarkPhasePrologue(stack_state, marking_type, reason);
  }

  if (stack_state == BlinkGC::kNoHeapPointersOnStack) {
    Heap().FlushNotFullyConstructedObjects();
  }

  DCHECK(InAtomicMarkingPause());
  Heap().MakeConsistentForGC();
  // AtomicPauseMarkPrologue is the common entry point for marking. The
  // requirement is to lock from roots marking to weakness processing which is
  // why the lock is taken at the end of the prologue.
  static_cast<MutexBase&>(ProcessHeap::CrossThreadPersistentMutex()).lock();
}

void ThreadState::AtomicPauseEpilogue() {
  if (!IsSweepingInProgress()) {
    // Sweeping was finished during the atomic pause. Update statistics needs to
    // run outside of the top-most stats scope.
    PostSweep();
  }
}

void ThreadState::CompleteSweep() {
  DCHECK(CheckThread());
  // If we are not in a sweeping phase, there is nothing to do here.
  if (!IsSweepingInProgress())
    return;

  // completeSweep() can be called recursively if finalizers can allocate
  // memory and the allocation triggers completeSweep(). This check prevents
  // the sweeping from being executed recursively.
  if (SweepForbidden())
    return;

  {
    // CompleteSweep may be called during regular mutator execution, from a
    // task, or from the atomic pause in which the atomic scope has already been
    // opened.
    const bool was_in_atomic_pause = in_atomic_pause();
    if (!was_in_atomic_pause)
      EnterAtomicPause();
    ScriptForbiddenScope script_forbidden;
    SweepForbiddenScope scope(this);
    ThreadHeapStatsCollector::EnabledScope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kCompleteSweep,
        "forced", IsForcedGC(current_gc_data_.reason));
    Heap().CompleteSweep();
    SynchronizeAndFinishConcurrentSweeping();

    if (!was_in_atomic_pause)
      LeaveAtomicPause();
  }
  NotifySweepDone();
}

void ThreadState::SynchronizeAndFinishConcurrentSweeping() {
  DCHECK(CheckThread());
  DCHECK(IsSweepingInProgress());
  DCHECK(SweepForbidden());

  // Wait for concurrent sweepers.
  sweeper_scheduler_->CancelAndWait();

  // Concurrent sweepers may perform some work at the last stage (e.g.
  // sweeping the last page and preparing finalizers).
  Heap().InvokeFinalizersOnSweptPages();
}

BlinkGCObserver::BlinkGCObserver(ThreadState* thread_state)
    : thread_state_(thread_state) {
  thread_state_->AddObserver(this);
}

BlinkGCObserver::~BlinkGCObserver() {
  thread_state_->RemoveObserver(this);
}

namespace {

// Update trace counters with statistics from the current and previous garbage
// collection cycle. We allow emitting current values here since these values
// can be useful for inspecting traces.
void UpdateTraceCounters(const ThreadHeapStatsCollector& stats_collector) {
  bool gc_tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                                     &gc_tracing_enabled);
  if (!gc_tracing_enabled)
    return;

  // Previous garbage collection cycle values.
  const ThreadHeapStatsCollector::Event& event = stats_collector.previous();
  const int collection_rate_percent =
      static_cast<int>(100 * (1.0 - event.live_object_rate));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.CollectionRate", collection_rate_percent);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.MarkedObjectSizeAtLastCompleteSweepKB",
                 CappedSizeInKB(event.marked_bytes));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.ObjectSizeAtLastGCKB",
                 CappedSizeInKB(event.object_size_in_bytes_before_sweeping));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"), "BlinkGC.AllocatedSpaceAtLastGCKB",
      CappedSizeInKB(event.allocated_space_in_bytes_before_sweeping));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.PartitionAllocSizeAtLastGCKB",
                 CappedSizeInKB(event.partition_alloc_bytes_before_sweeping));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.WrapperCountAtLastGC",
                 event.wrapper_count_before_sweeping);

  // Current values.
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.AllocatedSpaceKB",
                 CappedSizeInKB(stats_collector.allocated_space_bytes()));
  size_t allocated_bytes_since_prev_gc =
      stats_collector.allocated_bytes_since_prev_gc() > 0
          ? static_cast<size_t>(stats_collector.allocated_bytes_since_prev_gc())
          : 0;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.AllocatedObjectSizeSincePreviousGCKB",
                 CappedSizeInKB(allocated_bytes_since_prev_gc));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "PartitionAlloc.TotalSizeOfCommittedPagesKB",
                 CappedSizeInKB(WTF::Partitions::TotalSizeOfCommittedPages()));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "BlinkGC.WrapperCount",
                 stats_collector.wrapper_count());
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.CollectedWrapperCount",
                 stats_collector.collected_wrapper_count());
}

// Update histograms with statistics from the previous garbage collection cycle.
// Anything that is part of a histogram should have a well-defined lifetime wrt.
// to a garbage collection cycle.
void UpdateHistograms(const ThreadHeapStatsCollector::Event& event) {
  UMA_HISTOGRAM_ENUMERATION("BlinkGC.GCReason", event.reason);

  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForAtomicPhase", event.atomic_pause_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForAtomicPhaseMarking",
                      event.atomic_marking_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForGCCycle", event.gc_cycle_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForIncrementalMarking",
                      event.incremental_marking_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForMarking.Foreground",
                      event.foreground_marking_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForMarking.Background",
                      event.background_marking_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForMarking", event.marking_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForNestedInV8", event.gc_nested_in_v8);
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForSweepingForeground",
                      event.foreground_sweeping_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForSweepingBackground",
                      event.background_sweeping_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForSweepingSum", event.sweeping_time());
  UMA_HISTOGRAM_TIMES(
      "BlinkGC.TimeForCompleteSweep",
      event.scope_data[ThreadHeapStatsCollector::kCompleteSweep]);
  UMA_HISTOGRAM_TIMES(
      "BlinkGC.TimeForInvokingPreFinalizers",
      event.scope_data[ThreadHeapStatsCollector::kInvokePreFinalizers]);
  UMA_HISTOGRAM_TIMES(
      "BlinkGC.TimeForHeapCompaction",
      event.scope_data[ThreadHeapStatsCollector::kAtomicPauseCompaction]);
  UMA_HISTOGRAM_TIMES(
      "BlinkGC.TimeForGlobalWeakProcessing",
      event.scope_data[ThreadHeapStatsCollector::kMarkWeakProcessing]);

  base::TimeDelta marking_duration = event.marking_time();
  constexpr size_t kMinObjectSizeForReportingThroughput = 1024 * 1024;
  if (base::TimeTicks::IsHighResolution() &&
      (event.object_size_in_bytes_before_sweeping >
       kMinObjectSizeForReportingThroughput) &&
      !marking_duration.is_zero()) {
    DCHECK_GT(marking_duration.InMillisecondsF(), 0.0);
    const int main_thread_marking_throughput_mb_per_s = static_cast<int>(
        static_cast<double>(event.object_size_in_bytes_before_sweeping) /
        marking_duration.InMillisecondsF() * 1000 / 1024 / 1024);
    UMA_HISTOGRAM_COUNTS_100000("BlinkGC.MainThreadMarkingThroughput",
                                main_thread_marking_throughput_mb_per_s);
  }

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, object_size_freed_by_heap_compaction,
      ("BlinkGC.ObjectSizeFreedByHeapCompaction", 1, 4 * 1024 * 1024, 50));
  object_size_freed_by_heap_compaction.Count(
      CappedSizeInKB(event.compaction_freed_bytes));

  DEFINE_STATIC_LOCAL(CustomCountHistogram, object_size_before_gc_histogram,
                      ("BlinkGC.ObjectSizeBeforeGC", 1, 4 * 1024 * 1024, 50));
  object_size_before_gc_histogram.Count(
      CappedSizeInKB(event.object_size_in_bytes_before_sweeping));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, object_size_after_gc_histogram,
                      ("BlinkGC.ObjectSizeAfterGC", 1, 4 * 1024 * 1024, 50));
  object_size_after_gc_histogram.Count(CappedSizeInKB(event.marked_bytes));

  const int collection_rate_percent =
      static_cast<int>(100 * (1.0 - event.live_object_rate));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, collection_rate_histogram,
                      ("BlinkGC.CollectionRate", 1, 100, 20));
  collection_rate_histogram.Count(collection_rate_percent);

  // Per GCReason metrics.
  switch (event.reason) {
#define COUNT_BY_GC_REASON(reason)                                        \
  case BlinkGC::GCReason::k##reason: {                                    \
    UMA_HISTOGRAM_TIMES("BlinkGC.AtomicPhaseMarking_" #reason,            \
                        event.atomic_marking_time());                     \
    DEFINE_STATIC_LOCAL(CustomCountHistogram,                             \
                        collection_rate_reason_histogram,                 \
                        ("BlinkGC.CollectionRate_" #reason, 1, 100, 20)); \
    collection_rate_reason_histogram.Count(collection_rate_percent);      \
    break;                                                                \
  }

    // COUNT_BY_GC_REASON(IdleGC)
    COUNT_BY_GC_REASON(PreciseGC)
    COUNT_BY_GC_REASON(ConservativeGC)
    COUNT_BY_GC_REASON(ForcedGCForTesting)
    COUNT_BY_GC_REASON(MemoryPressureGC)
    COUNT_BY_GC_REASON(ThreadTerminationGC)
    COUNT_BY_GC_REASON(IncrementalV8FollowupGC)
    COUNT_BY_GC_REASON(UnifiedHeapGC)
    COUNT_BY_GC_REASON(UnifiedHeapForMemoryReductionGC)

#undef COUNT_BY_GC_REASON
  }
}

}  // namespace

void ThreadState::NotifySweepDone() {
  DCHECK(CheckThread());
  SetGCPhase(GCPhase::kNone);
  if (!in_atomic_pause()) {
    PostSweep();
  }
}

void ThreadState::PostSweep() {
  DCHECK(!in_atomic_pause());
  DCHECK(!IsSweepingInProgress());

  gc_age_++;

  for (auto* const observer : observers_)
    observer->OnCompleteSweepDone();

  Heap().stats_collector()->NotifySweepingCompleted();

  if (IsMainThread())
    UpdateHistograms(Heap().stats_collector()->previous());
  // Emit trace counters for all threads.
  UpdateTraceCounters(*Heap().stats_collector());
}

void ThreadState::SafePoint(BlinkGC::StackState stack_state) {
  DCHECK(CheckThread());

  RunScheduledGC(stack_state);
}

using PushAllRegistersCallback = void (*)(ThreadState*, intptr_t*);
extern "C" void PushAllRegisters(ThreadState*, PushAllRegistersCallback);

// static
void ThreadState::VisitStackAfterPushingRegisters(ThreadState* state,
                                                  intptr_t* end_of_stack) {
  state->VisitStack(static_cast<MarkingVisitor*>(state->CurrentVisitor()),
                    reinterpret_cast<Address*>(end_of_stack));
}

void ThreadState::PushRegistersAndVisitStack() {
  DCHECK(CheckThread());
  DCHECK(IsGCForbidden());
  DCHECK_EQ(current_gc_data_.stack_state, BlinkGC::kHeapPointersOnStack);
  PushAllRegisters(this, ThreadState::VisitStackAfterPushingRegisters);
}

void ThreadState::AddObserver(BlinkGCObserver* observer) {
  DCHECK(observer);
  DCHECK(!observers_.Contains(observer));
  observers_.insert(observer);
}

void ThreadState::RemoveObserver(BlinkGCObserver* observer) {
  DCHECK(observer);
  DCHECK(observers_.Contains(observer));
  observers_.erase(observer);
}

void ThreadState::EnterStaticReferenceRegistrationDisabledScope() {
  static_persistent_registration_disabled_count_++;
}

void ThreadState::LeaveStaticReferenceRegistrationDisabledScope() {
  DCHECK(static_persistent_registration_disabled_count_);
  static_persistent_registration_disabled_count_--;
}

void ThreadState::RegisterStaticPersistentNode(PersistentNode* node) {
  if (static_persistent_registration_disabled_count_)
    return;

  DCHECK(!static_persistents_.Contains(node));
  static_persistents_.insert(node);
}

void ThreadState::ReleaseStaticPersistentNodes() {
  HashSet<PersistentNode*> static_persistents;
  static_persistents.swap(static_persistents_);

  PersistentRegion* persistent_region = GetPersistentRegion();
  for (PersistentNode* it : static_persistents)
    persistent_region->ReleaseNode(it);
}

void ThreadState::FreePersistentNode(PersistentRegion* persistent_region,
                                     PersistentNode* persistent_node) {
  persistent_region->FreeNode(persistent_node);
  // Do not allow static persistents to be freed before
  // they're all released in releaseStaticPersistentNodes().
  //
  // There's no fundamental reason why this couldn't be supported,
  // but no known use for it.
  if (persistent_region == GetPersistentRegion())
    DCHECK(!static_persistents_.Contains(persistent_node));
}

void ThreadState::RegisterPreFinalizer(void* object,
                                       PreFinalizerCallback callback) {
#if DCHECK_IS_ON()
  DCHECK(CheckThread());
#endif
  DCHECK(!SweepForbidden());

  HeapObjectHeader* header = HeapObjectHeader::FromInnerAddress(object);
  DCHECK(ordered_pre_finalizers_.end() ==
         std::find(ordered_pre_finalizers_.begin(),
                   ordered_pre_finalizers_.end(),
                   PreFinalizer{header, object, callback}));
  ordered_pre_finalizers_.push_back(PreFinalizer{header, object, callback});
}

void ThreadState::InvokePreFinalizers() {
  DCHECK(CheckThread());
  DCHECK(!SweepForbidden());

  ThreadHeapStatsCollector::Scope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kInvokePreFinalizers);
  SweepForbiddenScope sweep_forbidden(this);
  // Pre finalizers are forbidden from allocating objects.
  NoAllocationScope no_allocation_scope(this);

  // Call the prefinalizers in the opposite order to their registration.
  Deque<PreFinalizer> remaining_ordered_pre_finalizers;
  for (auto rit = ordered_pre_finalizers_.rbegin();
       rit != ordered_pre_finalizers_.rend(); ++rit) {
    const PreFinalizer& pre_finalizer = *rit;
    // Check if pre-finalizer should be executed.
    if (pre_finalizer.header->IsMarked()) {
      // Re-queue for checking in next garbage collection.
      remaining_ordered_pre_finalizers.push_front(pre_finalizer);
    } else {
      // Execute pre-finalizer.
      pre_finalizer.callback(pre_finalizer.object);
    }
  }

  ordered_pre_finalizers_ = std::move(remaining_ordered_pre_finalizers);
}

// static
AtomicEntryFlag ThreadState::incremental_marking_flag_;

void ThreadState::EnableIncrementalMarkingBarrier() {
  CHECK(!IsIncrementalMarking());
  incremental_marking_flag_.Enter();
  SetIncrementalMarking(true);
}

void ThreadState::DisableIncrementalMarkingBarrier() {
  CHECK(IsIncrementalMarking());
  incremental_marking_flag_.Exit();
  SetIncrementalMarking(false);
}

void ThreadState::IncrementalMarkingStartForTesting() {
  // kIncrementalGCScheduled state requires sweeping to not be in progress.
  CompleteSweep();
  SetGCState(kIncrementalGCScheduled);
  IncrementalMarkingStart(BlinkGC::GCReason::kForcedGCForTesting);
}

void ThreadState::IncrementalMarkingStart(BlinkGC::GCReason reason) {
  DCHECK(!IsGCForbidden());
  DCHECK_EQ(kIncrementalGCScheduled, GetGCState());

  VLOG(2) << "[state:" << this << "] "
          << "IncrementalMarking: Start";
  DCHECK(!IsMarkingInProgress());
  // Sweeping is performed in driver functions.
  DCHECK(!IsSweepingInProgress());
  Heap().stats_collector()->NotifyMarkingStarted(reason);
  {
    ThreadHeapStatsCollector::EnabledScope stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kIncrementalMarkingStartMarking, "reason",
        BlinkGC::ToString(reason));
    AtomicPauseScope atomic_pause_scope(this);
    ScriptForbiddenScope script_forbidden_scope;
    MarkPhasePrologue(BlinkGC::kNoHeapPointersOnStack,
                      BlinkGC::kIncrementalAndConcurrentMarking, reason);
    {
      MutexLocker persistent_lock(ProcessHeap::CrossThreadPersistentMutex());
      MarkPhaseVisitRoots();
    }
    DCHECK(Heap().GetV8ReferencesWorklist()->IsGlobalEmpty());
    EnableIncrementalMarkingBarrier();
    if (base::FeatureList::IsEnabled(
            blink::features::kBlinkHeapConcurrentMarking)) {
      // No active concurrent markers yet, so it is safe to write to
      // concurrently_marked_bytes_ without a lock.
      concurrently_marked_bytes_ = 0;
      current_gc_data_.visitor->FlushMarkingWorklist();
      // Check that the marking worklist has enough private segments for all
      // concurrent marking tasks.
      const uint8_t max_concurrent_task_id =
          WorklistTaskId::ConcurrentThreadBase +
          kNumberOfConcurrentMarkingTasks;
      DCHECK_LE(max_concurrent_task_id,
                Heap().GetMarkingWorklist()->num_tasks());
      // Initialize concurrent marking task ids.
      for (uint8_t i = WorklistTaskId::ConcurrentThreadBase;
           i < max_concurrent_task_id; ++i) {
        available_concurrent_marking_task_ids_.push_back(i);
      }
      ScheduleConcurrentMarking();
    }
    SetGCState(kIncrementalMarkingStepScheduled);
    DCHECK(IsMarkingInProgress());
  }
}

void ThreadState::IncrementalMarkingStep(BlinkGC::StackState stack_state,
                                         base::TimeDelta duration) {
  DCHECK(IsMarkingInProgress());
  DCHECK_EQ(kIncrementalMarkingStepScheduled, GetGCState());

  ThreadHeapStatsCollector::EnabledScope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kIncrementalMarkingStep);
  VLOG(2) << "[state:" << this << "] "
          << "IncrementalMarking: Step "
          << "Reason: " << BlinkGC::ToString(current_gc_data_.reason);
  AtomicPauseScope atomic_pause_scope(this);
  ScriptForbiddenScope script_forbidden_scope;
  if (stack_state == BlinkGC::kNoHeapPointersOnStack) {
    Heap().FlushNotFullyConstructedObjects();
  }

  bool complete = MarkPhaseAdvanceMarking(base::TimeTicks::Now() + duration);

  if (base::FeatureList::IsEnabled(
          blink::features::kBlinkHeapConcurrentMarking)) {
    complete = complete && ConcurrentMarkingStep();
  }

  if (complete) {
    if (IsUnifiedGCMarkingInProgress()) {
      // If there are no more objects to mark for unified garbage collections
      // just bail out of helping incrementally using tasks. V8 will drive
      // further marking if new objects are discovered. Otherwise, just process
      // the rest in the atomic pause.
      DCHECK(IsUnifiedGCMarkingInProgress());
      SetGCState(kIncrementalMarkingStepPaused);
    } else {
      SetGCState(kIncrementalMarkingFinalizeScheduled);
    }
  } else {
    SetGCState(kIncrementalMarkingStepScheduled);
  }
  DCHECK(IsMarkingInProgress());
}

bool ThreadState::ConcurrentMarkingStep() {
  current_gc_data_.visitor->FlushMarkingWorklist();
  if (!Heap().GetMarkingWorklist()->IsGlobalPoolEmpty()) {
    ScheduleConcurrentMarking();
    return false;
  }
  base::AutoLock lock(concurrent_marker_bootstrapping_lock_);
  return active_markers_ == 0;
}

void ThreadState::IncrementalMarkingFinalize() {
  DCHECK(IsMarkingInProgress());
  DCHECK(!IsUnifiedGCMarkingInProgress());
  DCHECK_EQ(kIncrementalMarkingFinalizeScheduled, GetGCState());

  ThreadHeapStatsCollector::EnabledScope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kIncrementalMarkingFinalize);
  VLOG(2) << "[state:" << this << "] "
          << "IncrementalMarking: Finalize "
          << "Reason: " << BlinkGC::ToString(current_gc_data_.reason);
  // Call into the regular bottleneck instead of the internal version to get
  // UMA accounting and allow follow up GCs if necessary.
  DCHECK_EQ(BlinkGC::kIncrementalAndConcurrentMarking,
            current_gc_data_.marking_type);
  CollectGarbage(BlinkGC::kNoHeapPointersOnStack, current_gc_data_.marking_type,
                 BlinkGC::kConcurrentAndLazySweeping, current_gc_data_.reason);
}

bool ThreadState::FinishIncrementalMarkingIfRunning(
    BlinkGC::StackState stack_state,
    BlinkGC::MarkingType marking_type,
    BlinkGC::SweepingType sweeping_type,
    BlinkGC::GCReason reason) {
  if (IsMarkingInProgress()) {
    // TODO(mlippautz): Consider improving this mechanism as it will pull in
    // finalization of V8 upon Oilpan GCs during a unified GC. Alternative
    // include either breaking up the GCs or avoiding the call in first place.
    if (IsUnifiedGCMarkingInProgress()) {
      unified_heap_controller()->FinalizeTracing();
    } else {
      RunAtomicPause(stack_state, marking_type, sweeping_type, reason);
    }
    return true;
  }
  return false;
}

void ThreadState::CollectGarbage(BlinkGC::StackState stack_state,
                                 BlinkGC::MarkingType marking_type,
                                 BlinkGC::SweepingType sweeping_type,
                                 BlinkGC::GCReason reason) {
  // Nested garbage collection invocations are not supported.
  CHECK(!IsGCForbidden());
  // Garbage collection during sweeping is not supported. This can happen when
  // finalizers trigger garbage collections.
  if (SweepForbidden())
    return;

  base::TimeTicks start_total_collect_garbage_time = base::TimeTicks::Now();
  RUNTIME_CALL_TIMER_SCOPE_IF_ISOLATE_EXISTS(
      GetIsolate(), RuntimeCallStats::CounterId::kCollectGarbage);

  const bool was_incremental_marking = FinishIncrementalMarkingIfRunning(
      stack_state, marking_type, sweeping_type, reason);

  // We don't want floating garbage for the specific garbage collection types
  // mentioned below. In this case we will follow up with a regular full
  // garbage collection.
  const bool should_do_full_gc =
      !was_incremental_marking ||
      reason == BlinkGC::GCReason::kForcedGCForTesting ||
      reason == BlinkGC::GCReason::kMemoryPressureGC ||
      reason == BlinkGC::GCReason::kThreadTerminationGC;
  if (should_do_full_gc) {
    CompleteSweep();
    SetGCState(kNoGCScheduled);
    Heap().stats_collector()->NotifyMarkingStarted(reason);
    RunAtomicPause(stack_state, marking_type, sweeping_type, reason);
  }

  const base::TimeDelta total_collect_garbage_time =
      base::TimeTicks::Now() - start_total_collect_garbage_time;
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForTotalCollectGarbage",
                      total_collect_garbage_time);

#define COUNT_BY_GC_REASON(reason)                                     \
  case BlinkGC::GCReason::k##reason: {                                 \
    UMA_HISTOGRAM_TIMES("BlinkGC.TimeForTotalCollectGarbage_" #reason, \
                        total_collect_garbage_time);                   \
    break;                                                             \
  }

  switch (reason) {
    COUNT_BY_GC_REASON(PreciseGC)
    COUNT_BY_GC_REASON(ConservativeGC)
    COUNT_BY_GC_REASON(ForcedGCForTesting)
    COUNT_BY_GC_REASON(MemoryPressureGC)
    COUNT_BY_GC_REASON(ThreadTerminationGC)
    COUNT_BY_GC_REASON(IncrementalV8FollowupGC)
    COUNT_BY_GC_REASON(UnifiedHeapGC)
    COUNT_BY_GC_REASON(UnifiedHeapForMemoryReductionGC)
  }
#undef COUNT_BY_GC_REASON

  VLOG(1) << "[state:" << this << "]"
          << " CollectGarbage: time: " << std::setprecision(2)
          << total_collect_garbage_time.InMillisecondsF() << "ms"
          << " stack: " << BlinkGC::ToString(stack_state)
          << " marking: " << BlinkGC::ToString(marking_type)
          << " sweeping: " << BlinkGC::ToString(sweeping_type)
          << " reason: " << BlinkGC::ToString(reason);
}

void ThreadState::AtomicPauseMarkRoots(BlinkGC::StackState stack_state,
                                       BlinkGC::MarkingType marking_type,
                                       BlinkGC::GCReason reason) {
  ThreadHeapStatsCollector::EnabledScope advance_tracing_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kAtomicPauseMarkRoots,
      "epoch", gc_age_, "forced", IsForcedGC(current_gc_data_.reason));
  MarkPhaseVisitRoots();
  MarkPhaseVisitNotFullyConstructedObjects();
}

void ThreadState::AtomicPauseMarkTransitiveClosure() {
  ThreadHeapStatsCollector::EnabledScope advance_tracing_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure, "epoch",
      gc_age_, "forced", IsForcedGC(current_gc_data_.reason));
  CHECK(MarkPhaseAdvanceMarking(base::TimeTicks::Max()));
}

void ThreadState::AtomicPauseMarkEpilogue(BlinkGC::MarkingType marking_type) {
  ThreadHeapStatsCollector::EnabledScope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkEpilogue, "epoch", gc_age_,
      "forced", IsForcedGC(current_gc_data_.reason));
  MarkPhaseEpilogue(marking_type);
  LeaveGCForbiddenScope();
  LeaveNoAllocationScope();
  LeaveAtomicPause();
  static_cast<MutexBase&>(ProcessHeap::CrossThreadPersistentMutex()).unlock();
}

namespace {

class ClearReferencesInDeadObjectsVisitor final
    : public v8::PersistentHandleVisitor,
      public v8::EmbedderHeapTracer::TracedGlobalHandleVisitor {
 public:
  explicit ClearReferencesInDeadObjectsVisitor(ThreadHeap* heap)
      : heap_(heap) {}

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t class_id) final {
    if (InDeadObject(value))
      value->Reset();
  }

  void VisitTracedGlobalHandle(const v8::TracedGlobal<v8::Value>&) final {
    CHECK(false) << "Blink does not use v8::TracedGlobal.";
  }

  void VisitTracedReference(const v8::TracedReference<v8::Value>& value) final {
    // TODO(mlippautz): Avoid const_cast after changing the API to allow
    // modificaton of the handle.
    if (InDeadObject(&const_cast<v8::TracedReference<v8::Value>&>(value)))
      const_cast<v8::TracedReference<v8::Value>&>(value).Reset();
  }

 private:
  bool InDeadObject(void* address) {
    // Filter slots not on the heap.
    if (!heap_->LookupPageForAddress(reinterpret_cast<Address>(address)))
      return false;

    return !HeapObjectHeader::FromInnerAddress(address)->IsMarked();
  }

  ThreadHeap* const heap_;
};

}  // namespace

void ThreadState::AtomicPauseSweepAndCompact(
    BlinkGC::MarkingType marking_type,
    BlinkGC::SweepingType sweeping_type) {
  ThreadHeapStatsCollector::EnabledScope stats(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseSweepAndCompact, "epoch", gc_age_,
      "forced", IsForcedGC(current_gc_data_.reason));
  AtomicPauseScope atomic_pause_scope(this);
  ScriptForbiddenScope script_forbidden_scope;

  DCHECK(InAtomicMarkingPause());
  DCHECK(CheckThread());
  Heap().PrepareForSweep();

  // We have to set the GCPhase to Sweeping before calling pre-finalizers
  // to disallow a GC during the pre-finalizers.
  SetGCPhase(GCPhase::kSweeping);

  if (!IsUnifiedHeapGC() && GetIsolate()) {
    // Clear any Blink->V8 references in dead objects in case of a stand-alone
    // garbage collection. This is necessary to avoid calling destructors on the
    // references.
    ClearReferencesInDeadObjectsVisitor visitor(&Heap());
    GetIsolate()->VisitHandlesWithClassIds(&visitor);
    unified_heap_controller()->IterateTracedGlobalHandles(&visitor);
  }

  // Allocation is allowed during the pre-finalizers and destructors.
  // However, they must not mutate an object graph in a way in which
  // a dead object gets resurrected.
  InvokePreFinalizers();

  // Slots filtering requires liveness information which is only present before
  // sweeping any arena.
  {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kAtomicPauseCompaction);
    Heap().Compaction()->UpdateBackingStoreCallbacks();
    Heap().Compaction()->FilterNonLiveSlots();
  }

  // Last point where all mark bits are present.
  VerifyMarking(marking_type);

  // Any sweep compaction must happen after pre-finalizers, as it will
  // finalize dead objects in compactable arenas (e.g., backing stores
  // for container objects.)
  //
  // As per-contract for prefinalizers, those finalizable objects must
  // still be accessible when the prefinalizer runs, hence we cannot
  // schedule compaction until those have run.
  {
    SweepForbiddenScope scope(this);
    NoAllocationScope no_allocation_scope(this);
    Heap().Compact();
    Heap().DestroyCompactionWorklists();
  }

#if defined(ADDRESS_SANITIZER)
  PoisonUnmarkedObjects();
#endif  // ADDRESS_SANITIZER
  DCHECK(IsSweepingInProgress());
  if (sweeping_type == BlinkGC::kEagerSweeping) {
    // Eager sweeping should happen only in testing.
    CompleteSweep();
  } else {
    DCHECK(sweeping_type == BlinkGC::kConcurrentAndLazySweeping);
    // The default behavior is concurrent and lazy sweeping.
    ScheduleConcurrentAndLazySweep();
  }
}

#if defined(ADDRESS_SANITIZER)
namespace {

// Visitor unpoisoning all handles. Unpoisoning is required when dead objects
// are poisoned until they are later on processed.
//
// The raceful operations are:
// a. Running destructor that clears the handle.
// b. Running a stand-alone V8 GC (e.g. Scavenger) that clears the handle.
//
// Both operations run on the main thread and not concurrent.
class UnpoisonHandlesVisitor final
    : public v8::PersistentHandleVisitor,
      public v8::EmbedderHeapTracer::TracedGlobalHandleVisitor {
 public:
  explicit UnpoisonHandlesVisitor(ThreadHeap* heap) : heap_(heap) {}

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t class_id) final {
    VisitSlot(value, sizeof(v8::Persistent<v8::Value>));
  }

  void VisitTracedGlobalHandle(const v8::TracedGlobal<v8::Value>&) final {
    CHECK(false) << "Blink does not use v8::TracedGlobal.";
  }

  void VisitTracedReference(const v8::TracedReference<v8::Value>& value) final {
    // TODO(mlippautz): Avoid const_cast after changing the API to allow
    // modificaton of the handle.
    VisitSlot(&const_cast<v8::TracedReference<v8::Value>&>(value),
              sizeof(v8::TracedReference<v8::Value>));
  }

 private:
  void VisitSlot(void* address, size_t size) {
    // Filter slots not on the heap.
    if (!heap_->LookupPageForAddress(reinterpret_cast<Address>(address)))
      return;

    HeapObjectHeader* header = HeapObjectHeader::FromInnerAddress(address);
    if (!header->IsMarked()) {
      DCHECK(ASAN_REGION_IS_POISONED(address, size));
      ASAN_UNPOISON_MEMORY_REGION(address, size);
    }
  }

  ThreadHeap* const heap_;
};

}  // namespace

void ThreadState::PoisonUnmarkedObjects() {
  {
    // This lock must be held because other threads may access cross-thread
    // persistents and should not observe them in a poisoned state.
    MutexLocker lock(ProcessHeap::CrossThreadPersistentMutex());

    Heap().PoisonUnmarkedObjects();

    // CrossThreadPersistents in unmarked objects may be accessed from other
    // threads (e.g. in CrossThreadPersistentRegion::ShouldTracePersistent) and
    // that would be fine.
    ProcessHeap::GetCrossThreadPersistentRegion()
        .UnpoisonCrossThreadPersistents();
    ProcessHeap::GetCrossThreadWeakPersistentRegion()
        .UnpoisonCrossThreadPersistents();
  }

  // Similarly, unmarked object may contain handles to V8 that may be accessed
  // (cleared) until the destructors are run.
  if (GetIsolate()) {
    UnpoisonHandlesVisitor visitor(&Heap());
    GetIsolate()->VisitHandlesWithClassIds(&visitor);
    unified_heap_controller()->IterateTracedGlobalHandles(&visitor);
  }
}
#endif  // ADDRESS_SANITIZER

void ThreadState::RunAtomicPause(BlinkGC::StackState stack_state,
                                 BlinkGC::MarkingType marking_type,
                                 BlinkGC::SweepingType sweeping_type,
                                 BlinkGC::GCReason reason) {
  // Legacy scope that is used to add stand-alone Oilpan GCs to DevTools
  // timeline.
  TRACE_EVENT1("blink_gc,devtools.timeline", "BlinkGC.AtomicPhase", "forced",
               IsForcedGC(reason));

  AtomicPauseMarkPrologue(stack_state, marking_type, reason);
  AtomicPauseMarkRoots(stack_state, marking_type, reason);
  AtomicPauseMarkTransitiveClosure();
  AtomicPauseMarkEpilogue(marking_type);
  AtomicPauseSweepAndCompact(marking_type, sweeping_type);
  AtomicPauseEpilogue();
}

namespace {

MarkingVisitor::MarkingMode GetMarkingMode(bool should_compact) {
  return (should_compact) ? MarkingVisitor::kGlobalMarkingWithCompaction
                          : MarkingVisitor::kGlobalMarking;
}

}  // namespace

void ThreadState::MarkPhasePrologue(BlinkGC::StackState stack_state,
                                    BlinkGC::MarkingType marking_type,
                                    BlinkGC::GCReason reason) {
  SetGCPhase(GCPhase::kMarking);
  Heap().SetupWorklists();

  const bool compaction_enabled =
      Heap().Compaction()->ShouldCompact(stack_state, marking_type, reason);
  if (compaction_enabled) {
    Heap().Compaction()->Initialize(this);
  }

  current_gc_data_.reason = reason;
  current_gc_data_.visitor =
      IsUnifiedGCMarkingInProgress()
          ? std::make_unique<UnifiedHeapMarkingVisitor>(
                this, GetMarkingMode(compaction_enabled), GetIsolate())
          : std::make_unique<MarkingVisitor>(
                this, GetMarkingMode(compaction_enabled));
  current_gc_data_.stack_state = stack_state;
  current_gc_data_.marking_type = marking_type;
}

void ThreadState::MarkPhaseVisitRoots() {
  Visitor* visitor = current_gc_data_.visitor.get();

  VisitPersistents(visitor);

  // DOM wrapper references from V8 are considered as roots. Exceptions are:
  // - Unified garbage collections: The cross-component references between
  //   V8<->Blink are found using collaborative tracing where both GCs report
  //   live references to each other.
  // - Termination GCs that do not care about V8 any longer.
  if (!IsUnifiedGCMarkingInProgress() &&
      current_gc_data_.reason != BlinkGC::GCReason::kThreadTerminationGC) {
    VisitDOMWrappers(visitor);
  }

  // For unified garbage collections any active ScriptWrappable objects are
  // considered as roots.
  if (IsUnifiedGCMarkingInProgress()) {
    ActiveScriptWrappableBase::TraceActiveScriptWrappables(isolate_, visitor);
  }

  if (current_gc_data_.stack_state == BlinkGC::kHeapPointersOnStack) {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kVisitStackRoots);
    AddressCache::EnabledScope address_cache_scope(Heap().address_cache());
    PushRegistersAndVisitStack();
  }
}

bool ThreadState::MarkPhaseAdvanceMarking(base::TimeTicks deadline) {
  return Heap().AdvanceMarking(
      reinterpret_cast<MarkingVisitor*>(current_gc_data_.visitor.get()),
      deadline);
}

bool ThreadState::IsVerifyMarkingEnabled() const {
  bool should_verify_marking = base::FeatureList::IsEnabled(
      blink::features::kBlinkHeapIncrementalMarkingStress);
#if BUILDFLAG(BLINK_HEAP_VERIFICATION)
  should_verify_marking = true;
#endif  // BLINK_HEAP_VERIFICATION
  return should_verify_marking;
}

void ThreadState::MarkPhaseVisitNotFullyConstructedObjects() {
  Heap().MarkNotFullyConstructedObjects(
      reinterpret_cast<MarkingVisitor*>(current_gc_data_.visitor.get()));
}

void ThreadState::MarkPhaseEpilogue(BlinkGC::MarkingType marking_type) {
  MarkingVisitor* visitor = current_gc_data_.visitor.get();
  {
    ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
    VisitWeakPersistents(visitor);
    Heap().WeakProcessing(visitor);
  }
  Heap().DestroyMarkingWorklists(current_gc_data_.stack_state);

  incremental_marking_scheduler_->Cancel();

  size_t marked_bytes = concurrently_marked_bytes_;

  current_gc_data_.visitor->FlushCompactionWorklists();
  marked_bytes += current_gc_data_.visitor->marked_bytes();
  current_gc_data_.visitor.reset();

  Heap().stats_collector()->NotifyMarkingCompleted(marked_bytes);

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, total_object_space_histogram,
      ("BlinkGC.TotalObjectSpace", 0, 4 * 1024 * 1024, 50));
  total_object_space_histogram.Count(
      CappedSizeInKB(ProcessHeap::TotalAllocatedObjectSize()));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, total_allocated_space_histogram,
      ("BlinkGC.TotalAllocatedSpace", 0, 4 * 1024 * 1024, 50));
  total_allocated_space_histogram.Count(
      CappedSizeInKB(ProcessHeap::TotalAllocatedSpace()));
}

void ThreadState::VerifyMarking(BlinkGC::MarkingType marking_type) {
  if (IsVerifyMarkingEnabled())
    Heap().VerifyMarking();
}

void ThreadState::CollectAllGarbageForTesting(BlinkGC::StackState stack_state) {
  // We need to run multiple GCs to collect a chain of persistent handles.
  size_t previous_live_objects = 0;
  for (int i = 0; i < 5; ++i) {
    CollectGarbage(stack_state, BlinkGC::kAtomicMarking,
                   BlinkGC::kEagerSweeping,
                   BlinkGC::GCReason::kForcedGCForTesting);
    const size_t live_objects =
        Heap().stats_collector()->previous().marked_bytes;
    if (live_objects == previous_live_objects)
      break;
    previous_live_objects = live_objects;
  }
}

void ThreadState::EnableCompactionForNextGCForTesting() {
  Heap().Compaction()->EnableCompactionForNextGCForTesting();
}

void ThreadState::ScheduleConcurrentMarking() {
  base::AutoLock lock(concurrent_marker_bootstrapping_lock_);

  DCHECK(base::FeatureList::IsEnabled(
      blink::features::kBlinkHeapConcurrentMarking));

  for (uint8_t i = active_markers_; i < kNumberOfConcurrentMarkingTasks; ++i) {
    marker_scheduler_->ScheduleTask(WTF::CrossThreadBindOnce(
        &ThreadState::PerformConcurrentMark, WTF::CrossThreadUnretained(this)));
  }

  active_markers_ = kNumberOfConcurrentMarkingTasks;
}

void ThreadState::PerformConcurrentMark() {
  VLOG(2) << "[state:" << this << "] [threadid:" << CurrentThread() << "] "
          << "ConcurrentMark";
  ThreadHeapStatsCollector::EnabledConcurrentScope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kConcurrentMark);

  uint8_t task_id;
  {
    base::AutoLock lock(concurrent_marker_bootstrapping_lock_);
    DCHECK(!available_concurrent_marking_task_ids_.IsEmpty());
    task_id = available_concurrent_marking_task_ids_.back();
    available_concurrent_marking_task_ids_.pop_back();
  }

  std::unique_ptr<ConcurrentMarkingVisitor> concurrent_visitor =
      IsUnifiedGCMarkingInProgress()
          ? std::make_unique<ConcurrentUnifiedHeapMarkingVisitor>(
                this, GetMarkingMode(Heap().Compaction()->IsCompacting()),
                GetIsolate(), task_id)
          : std::make_unique<ConcurrentMarkingVisitor>(
                this, GetMarkingMode(Heap().Compaction()->IsCompacting()),
                task_id);

  const bool finished = Heap().AdvanceConcurrentMarking(
      concurrent_visitor.get(),
      base::TimeTicks::Now() + kConcurrentMarkingStepDuration);

  concurrent_visitor->FlushWorklists();
  {
    base::AutoLock lock(concurrent_marker_bootstrapping_lock_);
    // When marking is done, flush visitor worklists and decrement number of
    // active markers so we know how many markers are left
    concurrently_marked_bytes_ += concurrent_visitor->marked_bytes();
    available_concurrent_marking_task_ids_.push_back(task_id);
    if (finished) {
      --active_markers_;
      return;
    }
  }

  // Reschedule this marker
  marker_scheduler_->ScheduleTask(WTF::CrossThreadBindOnce(
      &ThreadState::PerformConcurrentMark, WTF::CrossThreadUnretained(this)));
}

}  // namespace blink
