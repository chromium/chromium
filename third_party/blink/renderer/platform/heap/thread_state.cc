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
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_scheduling_oracle.h"
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

constexpr double kMarkingScheduleRatioBeforeConcurrentPriorityIncrease = 0.5;

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
};

}  // namespace

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

  void Restart() {
    DCHECK(!task_.IsActive());
    ScheduleTask();
  }

  // Cancels incremental marking task in case there is any pending.
  void Cancel() { task_.Cancel(); }

 private:
  void Init(BlinkGC::GCReason reason) {
    DCHECK(!task_.IsActive());
    reason_ = reason;
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
      case ThreadState::kIncrementalMarkingStepScheduled:
        thread_state_->IncrementalMarkingStep(BlinkGC::kNoHeapPointersOnStack);
        if (thread_state_->GetGCState() !=
            ThreadState::kIncrementalMarkingStepPaused) {
          ScheduleTask();
        }
        break;
      case ThreadState::kIncrementalMarkingFinalizeScheduled:
        thread_state_->IncrementalMarkingFinalize();
        break;
      default:
        break;
    }
  }

  ThreadState* thread_state_;
  BlinkGC::GCReason reason_;
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
          std::make_unique<IncrementalMarkingScheduler>(this)) {
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
    V8BuildEmbedderGraphCallback v8_build_embedder_graph) {
  DCHECK(isolate);
  isolate_ = isolate;
  v8_build_embedder_graph_ = v8_build_embedder_graph;
  unified_heap_controller_.reset(new UnifiedHeapController(this));
  isolate_->SetEmbedderHeapTracer(unified_heap_controller_.get());
  unified_heap_controller_.get()->SetStackStart(WTF::GetStackStart());
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
  v8_build_embedder_graph_ = nullptr;
  unified_heap_controller_.reset();
}

void ThreadState::RunTerminationGC() {
  DCHECK(!IsMainThread());
  DCHECK(CheckThread());

  FinishIncrementalMarkingIfRunning(BlinkGC::CollectionType::kMajor,
                                    BlinkGC::kNoHeapPointersOnStack,
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
    CollectGarbage(BlinkGC::CollectionType::kMajor,
                   BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
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
      CollectGarbage(BlinkGC::CollectionType::kMajor,
                     BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
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
void ThreadState::VisitStackImpl(MarkingVisitor* visitor,
                                 Address* start_of_stack,
                                 Address* end_of_stack) {
  DCHECK_EQ(current_gc_data_.stack_state, BlinkGC::kHeapPointersOnStack);

  // Ensure that current is aligned by address size otherwise the loop below
  // will read past start address.
  Address* current = reinterpret_cast<Address*>(
      reinterpret_cast<intptr_t>(end_of_stack) & ~(sizeof(Address) - 1));

  for (; current < start_of_stack; ++current) {
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
    VisitAsanFakeStackForPointer(visitor, ptr, start_of_stack, end_of_stack);
  }
}

void ThreadState::VisitStack(MarkingVisitor* visitor, Address* end_of_stack) {
  VisitStackImpl(visitor, start_of_stack_, end_of_stack);
}

void ThreadState::VisitUnsafeStack(MarkingVisitor* visitor) {
#if HAS_FEATURE(safe_stack)
  VisitStackImpl(visitor,
                 static_cast<Address*>(__builtin___get_unsafe_stack_top()),
                 static_cast<Address*>(__builtin___get_unsafe_stack_ptr()));
#endif  // HAS_FEATURE(safe_stack)
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

void ThreadState::VisitRememberedSets(MarkingVisitor* visitor) {
  ThreadHeapStatsCollector::EnabledScope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kVisitRememberedSets);
  Heap().VisitRememberedSets(visitor);
}

void ThreadState::VisitWeakPersistents(Visitor* visitor) {
  ProcessHeap::GetCrossThreadWeakPersistentRegion().TraceNodes(visitor);
  weak_persistent_region_->TraceNodes(visitor);
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
    sweep_completed = Heap().AdvanceLazySweep(deadline);
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

void ThreadState::PerformConcurrentSweep(base::JobDelegate* job) {
  VLOG(2) << "[state:" << this << "] [threadid:" << CurrentThread() << "] "
          << "ConcurrentSweep";
  ThreadHeapStatsCollector::EnabledConcurrentScope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kConcurrentSweepingStep);

  if (Heap().AdvanceConcurrentSweep(job))
    has_unswept_pages_.store(false, std::memory_order_relaxed);
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

  has_unswept_pages_ = true;
  sweeper_handle_ = base::PostJob(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      ConvertToBaseRepeatingCallback(
          WTF::CrossThreadBindRepeating(&ThreadState::PerformConcurrentSweep,
                                        WTF::CrossThreadUnretained(this))),
      ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          [](ThreadState* state, size_t /*worker_count*/) -> size_t {
            return state->has_unswept_pages_.load(std::memory_order_relaxed)
                       ? 1
                       : 0;
          },
          WTF::CrossThreadUnretained(this))));
}

namespace {

#define UNEXPECTED_GCSTATE(s)                                   \
  case ThreadState::s:                                          \
    LOG(FATAL) << "Unexpected transition while in GCState " #s; \
    return

void UnexpectedGCState(ThreadState::GCState gc_state) {
  switch (gc_state) {
    UNEXPECTED_GCSTATE(kNoGCScheduled);
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
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled ||
                              gc_state_ == kForcedGCForTestingScheduled ||
                              gc_state_ == kIncrementalMarkingStepPaused ||
                              gc_state_ == kIncrementalMarkingStepScheduled ||
                              gc_state_ ==
                                  kIncrementalMarkingFinalizeScheduled ||
                              gc_state_ == kIncrementalGCScheduled);
      break;
    case kIncrementalMarkingStepScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled ||
                              gc_state_ == kIncrementalMarkingStepScheduled ||
                              gc_state_ == kIncrementalGCScheduled ||
                              gc_state_ == kIncrementalMarkingStepPaused);
      break;
    case kIncrementalMarkingFinalizeScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kIncrementalMarkingStepScheduled);
      break;
    case kForcedGCForTestingScheduled:
      DCHECK(CheckThread());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled ||
                              gc_state_ == kIncrementalMarkingStepPaused ||
                              gc_state_ == kIncrementalMarkingStepScheduled ||
                              gc_state_ ==
                                  kIncrementalMarkingFinalizeScheduled ||
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
      forced_scheduled_gc_for_testing_ = true;
      CollectAllGarbageForTesting();
      forced_scheduled_gc_for_testing_ = false;
      break;
    default:
      break;
  }
}

void ThreadState::AtomicPauseMarkPrologue(
    BlinkGC::CollectionType collection_type,
    BlinkGC::StackState stack_state,
    BlinkGC::MarkingType marking_type,
    BlinkGC::GCReason reason) {
  ThreadHeapStatsCollector::EnabledScope mark_prologue_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkPrologue);
  EnterAtomicPause();
  EnterNoAllocationScope();
  EnterGCForbiddenScope();

  if (HeapPointersOnStackForced()) {
    stack_state = BlinkGC::kHeapPointersOnStack;
  }

  if (IsMarkingInProgress()) {
    // Incremental marking is already in progress. Only update the state
    // that is necessary to update.
    SetGCState(kNoGCScheduled);
    if (base::FeatureList::IsEnabled(
            blink::features::kBlinkHeapConcurrentMarking)) {
      // Stop concurrent markers and wait synchronously until they have all
      // returned.
      marker_handle_.Cancel();
    }
#if DCHECK_IS_ON()
    MarkingWorklist* marking_worklist = Heap().GetMarkingWorklist();
    WriteBarrierWorklist* write_barrier_worklist =
        Heap().GetWriteBarrierWorklist();
    for (int concurrent_task = WorklistTaskId::ConcurrentThreadBase;
         concurrent_task < MarkingWorklist::kNumTasks; ++concurrent_task) {
      DCHECK(marking_worklist->IsLocalEmpty(concurrent_task));
      DCHECK(write_barrier_worklist->IsLocalEmpty(concurrent_task));
    }
#endif  // DCHECK_IS_ON()
    // Compaction needs to be canceled when incremental marking ends with a
    // conservative GC.
    if (stack_state == BlinkGC::kHeapPointersOnStack)
      Heap().Compaction()->Cancel();
    DisableIncrementalMarkingBarrier();
    current_gc_data_.reason = reason;
    current_gc_data_.stack_state = stack_state;
    Heap().stats_collector()->UpdateReason(reason);
  } else {
    DCHECK(!Heap().Compaction()->IsCompacting());
    MarkPhasePrologue(collection_type, stack_state, marking_type, reason);
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

  // CompleteSweep() can be called recursively if finalizers can allocate
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
        Heap().stats_collector(), ThreadHeapStatsCollector::kCompleteSweep);
    // Boost priority of sweeping job to complete ASAP and avoid taking time on
    // the main thread.
    if (sweeper_handle_)
      sweeper_handle_.UpdatePriority(base::TaskPriority::USER_BLOCKING);
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
  if (sweeper_handle_)
    sweeper_handle_.Cancel();

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
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForMarkingRoots",
                      event.roots_marking_time());
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

  base::TimeDelta marking_duration = event.foreground_marking_time();
  constexpr size_t kMinMarkedBytesForReportingThroughput = 1024 * 1024;
  if (base::TimeTicks::IsHighResolution() &&
      (event.marked_bytes > kMinMarkedBytesForReportingThroughput) &&
      !marking_duration.is_zero()) {
    DCHECK_GT(marking_duration.InMillisecondsF(), 0.0);
    const int main_thread_marking_throughput_mb_per_s = static_cast<int>(
        static_cast<double>(event.marked_bytes) /
        marking_duration.InMillisecondsF() * 1000 / 1024 / 1024);
    UMA_HISTOGRAM_COUNTS_100000("BlinkGC.MainThreadMarkingThroughput",
                                main_thread_marking_throughput_mb_per_s);
  }

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, object_size_freed_by_heap_compaction,
      ("BlinkGC.ObjectSizeFreedByHeapCompaction", 1, 4 * 1024 * 1024, 50));
  if (event.compaction_recorded_events) {
    object_size_freed_by_heap_compaction.Count(
        CappedSizeInKB(event.compaction_freed_bytes));
  }

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

    COUNT_BY_GC_REASON(ForcedGCForTesting)
    COUNT_BY_GC_REASON(ThreadTerminationGC)
    COUNT_BY_GC_REASON(UnifiedHeapGC)
    COUNT_BY_GC_REASON(UnifiedHeapForMemoryReductionGC)
    COUNT_BY_GC_REASON(UnifiedHeapForcedForTestingGC)

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
  // Visit registers, native stack, and asan fake stack.
  PushAllRegisters(this, ThreadState::VisitStackAfterPushingRegisters);
  // For builds that use safe stack, also visit the unsafe stack.
  VisitUnsafeStack(static_cast<MarkingVisitor*>(CurrentVisitor()));
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

void ThreadState::InvokePreFinalizers() {
  DCHECK(CheckThread());
  DCHECK(!SweepForbidden());

  ThreadHeapStatsCollector::Scope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kInvokePreFinalizers);
  SweepForbiddenScope sweep_forbidden(this);
  // Pre finalizers are forbidden from allocating objects
  NoAllocationScope no_allocation_scope(this);

  // Call the prefinalizers in the opposite order to their registration.
  //
  // Deque does not support modification during iteration, so
  // copy items first.
  //
  // The prefinalizer callback wrapper returns |true| when its associated
  // object is unreachable garbage and the prefinalizer callback has run.
  // The registered prefinalizer entry must then be removed and deleted.
  LivenessBroker broker = internal::LivenessBrokerFactory::Create();
  Deque<PreFinalizer> remaining_ordered_pre_finalizers;
  for (auto rit = ordered_pre_finalizers_.rbegin();
       rit != ordered_pre_finalizers_.rend(); ++rit) {
    const PreFinalizer& pre_finalizer = *rit;
    if (!(pre_finalizer.second)(broker, pre_finalizer.first))
      remaining_ordered_pre_finalizers.push_front(pre_finalizer);
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
  Heap().stats_collector()->NotifyMarkingStarted(
      BlinkGC::CollectionType::kMajor, reason, IsForcedGC(reason));
  {
    ThreadHeapStatsCollector::EnabledScope stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kIncrementalMarkingStartMarking, "reason",
        BlinkGC::ToString(reason));
    AtomicPauseScope atomic_pause_scope(this);
    ScriptForbiddenScope script_forbidden_scope;
    MarkPhasePrologue(BlinkGC::CollectionType::kMajor,
                      BlinkGC::kNoHeapPointersOnStack,
                      BlinkGC::kIncrementalAndConcurrentMarking, reason);
    {
      MutexLocker persistent_lock(ProcessHeap::CrossThreadPersistentMutex());
      MarkPhaseVisitRoots();
    }
    DCHECK(Heap().GetV8ReferencesWorklist()->IsGlobalEmpty());
    EnableIncrementalMarkingBarrier();
    if (base::FeatureList::IsEnabled(
            blink::features::kBlinkHeapConcurrentMarking)) {
      current_gc_data_.visitor->FlushMarkingWorklists();
      static_assert(
          MarkingWorklist::kNumTasks == WriteBarrierWorklist::kNumTasks,
          "Marking worklist and write-barrier worklist should be the "
          "same size");
      last_concurrently_marked_bytes_ = 0;
      last_concurrently_marked_bytes_update_ = base::TimeTicks::Now();
      concurrent_marking_priority_increased_ = false;
      ScheduleConcurrentMarking();
    }
    SetGCState(kIncrementalMarkingStepScheduled);
    DCHECK(IsMarkingInProgress());
  }
}

void ThreadState::IncrementalMarkingStep(BlinkGC::StackState stack_state) {
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

  bool complete;
  if (skip_incremental_marking_for_testing_) {
    complete = true;
    skip_incremental_marking_for_testing_ = false;
  } else {
    complete = MarkPhaseAdvanceMarking(
            marking_scheduling_->GetNextIncrementalStepDurationForTask(
                Heap().stats_collector()->object_size_in_bytes()),
        EphemeronProcessing::kPartialProcessing);
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kBlinkHeapConcurrentMarking)) {
    complete = ConcurrentMarkingStep() && complete;
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
  current_gc_data_.visitor->FlushMarkingWorklists();
  if (Heap().HasWorkForConcurrentMarking()) {
    // Notifies the scheduler that max concurrency might have increased.
    // This will adjust the number of markers if necessary.
    marker_handle_.NotifyConcurrencyIncrease();
    if (!concurrent_marking_priority_increased_) {
      // If concurrent tasks aren't executed, it might delay GC finalization.
      // As long as GC is active so is the write barrier, which incurs a
      // performance cost. Marking is estimated to take overall
      // |MarkingSchedulingOracle::kEstimatedMarkingTimeMs| (500ms). If
      // concurrent marking tasks have not reported any progress (i.e. the
      // concurrently marked bytes count as not changed) in over
      // |kMarkingScheduleRatioBeforeConcurrentPriorityIncrease| (50%) of
      // that expected duration, we increase the concurrent task priority
      // for the duration of the current GC. This is meant to prevent the
      // GC from exceeding it's expected end time.
      size_t current_concurrently_marked_bytes_ =
          marking_scheduling_->GetConcurrentlyMarkedBytes();
      if (current_concurrently_marked_bytes_ >
          last_concurrently_marked_bytes_) {
        last_concurrently_marked_bytes_ = current_concurrently_marked_bytes_;
        last_concurrently_marked_bytes_update_ = base::TimeTicks::Now();
      } else if ((base::TimeTicks::Now() -
                  last_concurrently_marked_bytes_update_)
                     .InMilliseconds() >
                 kMarkingScheduleRatioBeforeConcurrentPriorityIncrease *
                     MarkingSchedulingOracle::kEstimatedMarkingTimeMs) {
        marker_handle_.UpdatePriority(base::TaskPriority::USER_BLOCKING);
        concurrent_marking_priority_increased_ = true;
      }
    }
    return false;
  }
  return marker_handle_.IsCompleted();
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
  CollectGarbage(current_gc_data_.collection_type,
                 BlinkGC::kNoHeapPointersOnStack, current_gc_data_.marking_type,
                 BlinkGC::kConcurrentAndLazySweeping, current_gc_data_.reason);
}

bool ThreadState::FinishIncrementalMarkingIfRunning(
    BlinkGC::CollectionType collection_type,
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
      RunAtomicPause(collection_type, stack_state, marking_type, sweeping_type,
                     reason);
    }
    return true;
  }
  return false;
}

void ThreadState::RestartIncrementalMarkingIfPaused() {
  if (GetGCState() != ThreadState::kIncrementalMarkingStepPaused)
    return;
  SetGCState(ThreadState::kIncrementalMarkingStepScheduled);
  incremental_marking_scheduler_->Restart();
}

void ThreadState::CollectGarbage(BlinkGC::CollectionType collection_type,
                                 BlinkGC::StackState stack_state,
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
      collection_type, stack_state, marking_type, sweeping_type, reason);

  // We don't want floating garbage for the specific garbage collection types
  // mentioned below. In this case we will follow up with a regular full
  // garbage collection.
  const bool should_do_full_gc =
      !was_incremental_marking ||
      reason == BlinkGC::GCReason::kForcedGCForTesting ||
      reason == BlinkGC::GCReason::kThreadTerminationGC;
  if (should_do_full_gc) {
    CompleteSweep();
    SetGCState(kNoGCScheduled);
    Heap().stats_collector()->NotifyMarkingStarted(collection_type, reason,
                                                   IsForcedGC(reason));
    RunAtomicPause(collection_type, stack_state, marking_type, sweeping_type,
                   reason);
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
    COUNT_BY_GC_REASON(ForcedGCForTesting)
    COUNT_BY_GC_REASON(ThreadTerminationGC)
    COUNT_BY_GC_REASON(UnifiedHeapGC)
    COUNT_BY_GC_REASON(UnifiedHeapForMemoryReductionGC)
    COUNT_BY_GC_REASON(UnifiedHeapForcedForTestingGC)
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
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkRoots);
  MarkPhaseVisitRoots();
  MarkPhaseVisitNotFullyConstructedObjects();
}

void ThreadState::AtomicPauseMarkTransitiveClosure() {
  ThreadHeapStatsCollector::EnabledScope advance_tracing_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure);
  // base::TimeTicks::Now() + base::TimeDelta::Max() == base::TimeTicks::Max()
  CHECK(MarkPhaseAdvanceMarking(base::TimeDelta::Max(),
                                EphemeronProcessing::kFullProcessing));
}

void ThreadState::AtomicPauseMarkEpilogue(BlinkGC::MarkingType marking_type) {
  ThreadHeapStatsCollector::EnabledScope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkEpilogue);
  MarkPhaseEpilogue(marking_type);
  LeaveGCForbiddenScope();
  LeaveNoAllocationScope();
  LeaveAtomicPause();
  static_cast<MutexBase&>(ProcessHeap::CrossThreadPersistentMutex()).unlock();
}

void ThreadState::AtomicPauseSweepAndCompact(
    BlinkGC::CollectionType collection_type,
    BlinkGC::MarkingType marking_type,
    BlinkGC::SweepingType sweeping_type) {
  ThreadHeapStatsCollector::EnabledScope stats(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseSweepAndCompact);
  AtomicPauseScope atomic_pause_scope(this);
  ScriptForbiddenScope script_forbidden_scope;

  DCHECK(InAtomicMarkingPause());
  DCHECK(CheckThread());
  Heap().PrepareForSweep(collection_type);

  // We have to set the GCPhase to Sweeping before calling pre-finalizers
  // to disallow a GC during the pre-finalizers.
  SetGCPhase(GCPhase::kSweeping);

  InvokePreFinalizers();

  if (collection_type == BlinkGC::CollectionType::kMajor) {
    // Slots filtering requires liveness information which is only present
    // before sweeping any arena.
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kAtomicPauseCompaction);
    Heap().Compaction()->FilterNonLiveSlots();
  }

  // Last point where all mark bits are present.
  VerifyMarking(marking_type);

  if (collection_type == BlinkGC::CollectionType::kMajor) {
    // Any sweep compaction must happen after pre-finalizers, as it will
    // finalize dead objects in compactable arenas (e.g., backing stores
    // for container objects.)
    //
    // As per-contract for prefinalizers, those finalizable objects must
    // still be accessible when the prefinalizer runs, hence we cannot
    // schedule compaction until those have run.
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

void ThreadState::RunAtomicPause(BlinkGC::CollectionType collection_type,
                                 BlinkGC::StackState stack_state,
                                 BlinkGC::MarkingType marking_type,
                                 BlinkGC::SweepingType sweeping_type,
                                 BlinkGC::GCReason reason) {
  // Legacy scope that is used to add stand-alone Oilpan GCs to DevTools
  // timeline.
  TRACE_EVENT1("blink_gc,devtools.timeline", "BlinkGC.AtomicPhase", "forced",
               IsForcedGC(reason));

  AtomicPauseMarkPrologue(collection_type, stack_state, marking_type, reason);
  AtomicPauseMarkRoots(stack_state, marking_type, reason);
  AtomicPauseMarkTransitiveClosure();
  AtomicPauseMarkEpilogue(marking_type);
  AtomicPauseSweepAndCompact(collection_type, marking_type, sweeping_type);
  AtomicPauseEpilogue();
}

namespace {

MarkingVisitor::MarkingMode GetMarkingMode(bool should_compact) {
  return (should_compact) ? MarkingVisitor::kGlobalMarkingWithCompaction
                          : MarkingVisitor::kGlobalMarking;
}

}  // namespace

void ThreadState::MarkPhasePrologue(BlinkGC::CollectionType collection_type,
                                    BlinkGC::StackState stack_state,
                                    BlinkGC::MarkingType marking_type,
                                    BlinkGC::GCReason reason) {
  SetGCPhase(GCPhase::kMarking);

  const bool compaction_enabled =
      Heap().Compaction()->ShouldCompact(stack_state, marking_type, reason);

  Heap().SetupWorklists(compaction_enabled);

  if (compaction_enabled) {
    Heap().Compaction()->Initialize(this);
  }

#if BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
  if (collection_type == BlinkGC::CollectionType::kMajor) {
    // Unmark heap before doing major collection cycle.
    Heap().Unmark();
  }
#endif

  current_gc_data_.reason = reason;
  current_gc_data_.collection_type = collection_type;
  current_gc_data_.visitor =
      IsUnifiedGCMarkingInProgress()
          ? std::make_unique<UnifiedHeapMarkingVisitor>(
                this, GetMarkingMode(compaction_enabled), GetIsolate())
          : std::make_unique<MarkingVisitor>(
                this, GetMarkingMode(compaction_enabled));
  current_gc_data_.stack_state = stack_state;
  current_gc_data_.marking_type = marking_type;

  marking_scheduling_ = std::make_unique<MarkingSchedulingOracle>();
}

void ThreadState::MarkPhaseVisitRoots() {
  ThreadHeapStatsCollector::EnabledScope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kVisitRoots);

  Visitor* visitor = current_gc_data_.visitor.get();

  VisitPersistents(visitor);

  if (current_gc_data_.stack_state == BlinkGC::kHeapPointersOnStack) {
    ThreadHeapStatsCollector::Scope stack_stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kVisitStackRoots);
    PushRegistersAndVisitStack();
  }

  // Visit remembered sets (card tables) for minor collections.
  if (current_gc_data_.collection_type == BlinkGC::CollectionType::kMinor) {
    VisitRememberedSets(static_cast<MarkingVisitor*>(visitor));
  }
}

bool ThreadState::MarkPhaseAdvanceMarkingBasedOnSchedule(
    base::TimeDelta max_deadline,
    EphemeronProcessing ephemeron_processing) {
  return MarkPhaseAdvanceMarking(
      std::min(max_deadline,
               marking_scheduling_->GetNextIncrementalStepDurationForTask(
                   Heap().stats_collector()->object_size_in_bytes())),
      ephemeron_processing);
}

bool ThreadState::MarkPhaseAdvanceMarking(
    base::TimeDelta deadline,
    EphemeronProcessing ephemeron_processing) {
  MarkingVisitor* visitor = current_gc_data_.visitor.get();
  ThreadHeapStatsCollector::EnabledScope deadline_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kIncrementalMarkingWithDeadline, "deadline_ms",
      deadline.InMillisecondsF());
  const bool finished = Heap().AdvanceMarking(
      reinterpret_cast<MarkingVisitor*>(visitor),
      base::TimeTicks::Now() + deadline, ephemeron_processing);
  // visitor->marked_bytes() can also include bytes marked during roots
  // visitation which is not counted in worklist_processing_time_foreground.
  // Since the size of the roots is usually small relative to the size of
  // the object graph, this is fine.
  marking_scheduling_->UpdateIncrementalMarkingStats(
      visitor->marked_bytes(),
      Heap().stats_collector()->worklist_processing_time_foreground(),
      Heap().stats_collector()->flushing_v8_references_time());
  return finished;
}

bool ThreadState::IsVerifyMarkingEnabled() const {
  bool should_verify_marking = base::FeatureList::IsEnabled(
      blink::features::kBlinkHeapIncrementalMarkingStress);
#if BUILDFLAG(BLINK_HEAP_VERIFICATION)
  should_verify_marking = (disable_heap_verification_scope_ == 0);
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


  current_gc_data_.visitor->FlushCompactionWorklists();
  current_gc_data_.visitor.reset();

  Heap().stats_collector()->NotifyMarkingCompleted(
      marking_scheduling_->GetOverallMarkedBytes());
  marking_scheduling_.reset();

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

void ThreadState::CollectGarbageForTesting(
    BlinkGC::CollectionType collection_type,
    BlinkGC::StackState stack_state,
    BlinkGC::MarkingType marking_type,
    BlinkGC::SweepingType sweeping_type,
    BlinkGC::GCReason reason) {
  CollectGarbage(collection_type, stack_state, marking_type, sweeping_type,
                 reason);
}

void ThreadState::CollectAllGarbageForTesting(BlinkGC::StackState stack_state) {
  // We need to run multiple GCs to collect a chain of persistent handles.
  size_t previous_live_objects = 0;
  for (int i = 0; i < 5; ++i) {
    if (isolate_) {
      unified_heap_controller()->GarbageCollectionForTesting(
          stack_state == BlinkGC::kNoHeapPointersOnStack
              ? v8::EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers
              : v8::EmbedderHeapTracer::EmbedderStackState::
                    kMayContainHeapPointers);
    } else {
      CollectGarbage(BlinkGC::CollectionType::kMajor, stack_state,
                     BlinkGC::kAtomicMarking, BlinkGC::kEagerSweeping,
                     BlinkGC::GCReason::kForcedGCForTesting);
    }
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
  DCHECK(base::FeatureList::IsEnabled(
      blink::features::kBlinkHeapConcurrentMarking));

  marker_handle_ = base::PostJob(
      FROM_HERE, {base::ThreadPool(), base::TaskPriority::USER_VISIBLE},
      ConvertToBaseRepeatingCallback(
          WTF::CrossThreadBindRepeating(&ThreadState::PerformConcurrentMark,
                                        WTF::CrossThreadUnretained(this))),
      ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          [](ThreadState* state, size_t active_worker_count) -> size_t {
            // We need to account for local segments in addition to
            // ConcurrentMarkingGlobalWorkSize().
            return std::min<size_t>(
                state->Heap().ConcurrentMarkingGlobalWorkSize() +
                    active_worker_count,
                MarkingWorklist::kNumTasks -
                    WorklistTaskId::ConcurrentThreadBase);
          },
          WTF::CrossThreadUnretained(this))));
}

void ThreadState::PerformConcurrentMark(base::JobDelegate* job) {
  VLOG(2) << "[state:" << this << "] [threadid:" << CurrentThread() << "] "
          << "ConcurrentMark";
  ThreadHeapStatsCollector::EnabledConcurrentScope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kConcurrentMarkingStep);

  if (!Heap().HasWorkForConcurrentMarking())
    return;

  uint8_t task_id = job->GetTaskId() + 1;

  std::unique_ptr<ConcurrentMarkingVisitor> concurrent_visitor =
      IsUnifiedGCMarkingInProgress()
          ? std::make_unique<ConcurrentUnifiedHeapMarkingVisitor>(
                this, GetMarkingMode(Heap().Compaction()->IsCompacting()),
                GetIsolate(), task_id)
          : std::make_unique<ConcurrentMarkingVisitor>(
                this, GetMarkingMode(Heap().Compaction()->IsCompacting()),
                task_id);

  Heap().AdvanceConcurrentMarking(concurrent_visitor.get(), job,
                                  marking_scheduling_.get());

  marking_scheduling_->AddConcurrentlyMarkedBytes(
      concurrent_visitor->RecentlyMarkedBytes());

  concurrent_visitor->FlushWorklists();
}

}  // namespace blink
