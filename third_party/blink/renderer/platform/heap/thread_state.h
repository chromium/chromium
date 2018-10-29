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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_H_

#include <memory>

#include "base/atomicops.h"
#include "base/macros.h"
#include "third_party/blink/public/platform/scheduler/web_rail_mode_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/threading_traits.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/address_sanitizer.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace v8 {
class Isolate;
};

namespace blink {

namespace incremental_marking_test {
class IncrementalMarkingScope;
class IncrementalMarkingTestDriver;
}  // namespace incremental_marking_test

class GarbageCollectedMixinConstructorMarkerBase;
class MarkingVisitor;
class PersistentNode;
class PersistentRegion;
class ThreadHeap;
class ThreadState;
class Visitor;

template <ThreadAffinity affinity>
class ThreadStateFor;

// Declare that a class has a pre-finalizer. The pre-finalizer is called
// before any object gets swept, so it is safe to touch on-heap objects
// that may be collected in the same GC cycle. If you cannot avoid touching
// on-heap objects in a destructor (which is not allowed), you can consider
// using the pre-finalizer. The only restriction is that the pre-finalizer
// must not resurrect dead objects (e.g., store unmarked objects into
// Members etc). The pre-finalizer is called on the thread that registered
// the pre-finalizer.
//
// Since a pre-finalizer adds pressure on GC performance, you should use it
// only if necessary.
//
// A pre-finalizer is similar to the
// HeapHashMap<WeakMember<Foo>, std::unique_ptr<Disposer>> idiom.  The
// difference between this and the idiom is that pre-finalizer function is
// called whenever an object is destructed with this feature.  The
// HeapHashMap<WeakMember<Foo>, std::unique_ptr<Disposer>> idiom requires an
// assumption that the HeapHashMap outlives objects pointed by WeakMembers.
// FIXME: Replace all of the
// HeapHashMap<WeakMember<Foo>, std::unique_ptr<Disposer>> idiom usages with the
// pre-finalizer if the replacement won't cause performance regressions.
//
// Usage:
//
// class Foo : GarbageCollected<Foo> {
//     USING_PRE_FINALIZER(Foo, dispose);
// private:
//     void dispose()
//     {
//         bar_->...; // It is safe to touch other on-heap objects.
//     }
//     Member<Bar> bar_;
// };
#define USING_PRE_FINALIZER(Class, preFinalizer)                           \
 public:                                                                   \
  static bool InvokePreFinalizer(void* object) {                           \
    Class* self = reinterpret_cast<Class*>(object);                        \
    if (ThreadHeap::IsHeapObjectAlive(self))                               \
      return false;                                                        \
    self->Class::preFinalizer();                                           \
    return true;                                                           \
  }                                                                        \
                                                                           \
 private:                                                                  \
  ThreadState::PrefinalizerRegistration<Class> prefinalizer_dummy_ = this; \
  using UsingPreFinalizerMacroNeedsTrailingSemiColon = char

class PLATFORM_EXPORT BlinkGCObserver {
 public:
  // The constructor automatically register this object to ThreadState's
  // observer lists. The argument must not be null.
  explicit BlinkGCObserver(ThreadState*);

  // The destructor automatically unregister this object from ThreadState's
  // observer lists.
  virtual ~BlinkGCObserver();

  virtual void OnCompleteSweepDone() = 0;

 private:
  // As a ThreadState must live when a BlinkGCObserver lives, holding a raw
  // pointer is safe.
  ThreadState* thread_state_;
};

class PLATFORM_EXPORT ThreadState final
    : private scheduler::WebRAILModeObserver {
  USING_FAST_MALLOC(ThreadState);

 public:
  // See setGCState() for possible state transitions.
  enum GCState {
    kNoGCScheduled,
    kIdleGCScheduled,
    kIncrementalMarkingStepPaused,
    kIncrementalMarkingStepScheduled,
    kIncrementalMarkingFinalizeScheduled,
    kPreciseGCScheduled,
    kFullGCScheduled,
    kPageNavigationGCScheduled,
    kIncrementalGCScheduled,
  };

  // The phase that the GC is in. The GCPhase will not return kNone for mutators
  // running during incremental marking and lazy sweeping. See SetGCPhase() for
  // possible state transitions.
  enum class GCPhase {
    // GC is doing nothing.
    kNone,
    // GC is in marking phase.
    kMarking,
    // GC is in sweeping phase.
    kSweeping,
  };

  // The NoAllocationScope class is used in debug mode to catch unwanted
  // allocations. E.g. allocations during GC.
  class NoAllocationScope final {
    STACK_ALLOCATED();

   public:
    explicit NoAllocationScope(ThreadState* state) : state_(state) {
      state_->EnterNoAllocationScope();
    }
    ~NoAllocationScope() { state_->LeaveNoAllocationScope(); }

   private:
    ThreadState* state_;
  };

  class SweepForbiddenScope final {
    STACK_ALLOCATED();

   public:
    explicit SweepForbiddenScope(ThreadState* state) : state_(state) {
      DCHECK(!state_->sweep_forbidden_);
      state_->sweep_forbidden_ = true;
    }
    ~SweepForbiddenScope() {
      DCHECK(state_->sweep_forbidden_);
      state_->sweep_forbidden_ = false;
    }

   private:
    ThreadState* state_;
  };

  // Used to denote when access to unmarked objects is allowed but we shouldn't
  // ressurect it by making new references (e.g. during weak processing and pre
  // finalizer).
  class ObjectResurrectionForbiddenScope final {
    STACK_ALLOCATED();

   public:
    explicit ObjectResurrectionForbiddenScope(ThreadState* state)
        : state_(state) {
      state_->EnterObjectResurrectionForbiddenScope();
    }
    ~ObjectResurrectionForbiddenScope() {
      state_->LeaveObjectResurrectionForbiddenScope();
    }

   private:
    ThreadState* state_;
  };

  // Returns true if any thread is currently incremental marking its heap and
  // false otherwise. For an exact check use
  // ThreadState::IsIncrementalMarking().
  ALWAYS_INLINE static bool IsAnyIncrementalMarking() {
    // Stores use full barrier to allow using the simplest relaxed load here.
    return base::subtle::NoBarrier_Load(&incremental_marking_counter_) > 0;
  }

  // Returns true if any thread is currently incremental marking its heap and
  // false otherwise. For an exact check use ThreadState::IsWrapperTracing().
  static bool IsAnyWrapperTracing() {
    // Stores use full barrier to allow using the simplest relaxed load here.
    return base::subtle::NoBarrier_Load(&wrapper_tracing_counter_) > 0;
  }

  static void AttachMainThread();

  // Associate ThreadState object with the current thread. After this
  // call thread can start using the garbage collected heap infrastructure.
  // It also has to periodically check for safepoints.
  static void AttachCurrentThread();

  // Disassociate attached ThreadState from the current thread. The thread
  // can no longer use the garbage collected heap after this call.
  static void DetachCurrentThread();

  static ThreadState* Current() { return **thread_specific_; }

  static ThreadState* MainThreadState() {
    return reinterpret_cast<ThreadState*>(main_thread_state_storage_);
  }

  static ThreadState* FromObject(const void*);

  bool IsMainThread() const { return this == MainThreadState(); }
  bool CheckThread() const { return thread_ == CurrentThread(); }

  ThreadHeap& Heap() const { return *heap_; }
  ThreadIdentifier ThreadId() const { return thread_; }

  // When ThreadState is detaching from non-main thread its
  // heap is expected to be empty (because it is going away).
  // Perform registered cleanup tasks and garbage collection
  // to sweep away any objects that are left on this heap.
  // We assert that nothing must remain after this cleanup.
  // If assertion does not hold we crash as we are potentially
  // in the dangling pointer situation.
  void RunTerminationGC();

  void PerformIdleGC(TimeTicks deadline);
  void PerformIdleLazySweep(TimeTicks deadline);

  void ScheduleIdleGC();
  void ScheduleIdleLazySweep();
  void SchedulePreciseGC();
  void ScheduleIncrementalGC(BlinkGC::GCReason);
  void ScheduleV8FollowupGCIfNeeded(BlinkGC::V8GCType);
  void SchedulePageNavigationGCIfNeeded(float estimated_removal_ratio);
  void SchedulePageNavigationGC();
  void ScheduleFullGC();
  void ScheduleGCIfNeeded();
  void PostIdleGCTask();
  void WillStartV8GC(BlinkGC::V8GCType);
  void SetGCState(GCState);
  GCState GetGCState() const { return gc_state_; }
  void SetGCPhase(GCPhase);
  bool IsMarkingInProgress() const { return gc_phase_ == GCPhase::kMarking; }
  bool IsSweepingInProgress() const { return gc_phase_ == GCPhase::kSweeping; }
  bool IsUnifiedGCMarkingInProgress() const {
    return IsMarkingInProgress() &&
           current_gc_data_.reason == BlinkGC::GCReason::kUnifiedHeapGC;
  }

  void EnableWrapperTracingBarrier();
  void DisableWrapperTracingBarrier();

  // Incremental GC.
  void ScheduleIncrementalMarkingStep();
  void ScheduleIncrementalMarkingFinalize();

  void IncrementalMarkingStart(BlinkGC::GCReason);
  void IncrementalMarkingStep();
  void IncrementalMarkingFinalize();
  bool FinishIncrementalMarkingIfRunning(BlinkGC::StackState,
                                         BlinkGC::MarkingType,
                                         BlinkGC::SweepingType,
                                         BlinkGC::GCReason);

  void EnableIncrementalMarkingBarrier();
  void DisableIncrementalMarkingBarrier();

  void CompleteSweep();
  void FinishSnapshot();
  void PostSweep();

  // Support for disallowing allocation. Mainly used for sanity
  // checks asserts.
  bool IsAllocationAllowed() const {
    // Allocation is not allowed during atomic marking pause, but it is allowed
    // during atomic sweeping pause.
    return !InAtomicMarkingPause() && !no_allocation_count_;
  }
  void EnterNoAllocationScope() { no_allocation_count_++; }
  void LeaveNoAllocationScope() { no_allocation_count_--; }
  bool IsWrapperTracingForbidden() { return IsMixinInConstruction(); }
  bool IsGCForbidden() const {
    return gc_forbidden_count_ || IsMixinInConstruction();
  }
  void EnterGCForbiddenScope() { gc_forbidden_count_++; }
  void LeaveGCForbiddenScope() {
    DCHECK_GT(gc_forbidden_count_, 0u);
    gc_forbidden_count_--;
  }
  bool IsMixinInConstruction() const { return mixins_being_constructed_count_; }
  void EnterMixinConstructionScope() { mixins_being_constructed_count_++; }
  void LeaveMixinConstructionScope() {
    DCHECK_GT(mixins_being_constructed_count_, 0u);
    mixins_being_constructed_count_--;
  }
  bool SweepForbidden() const { return sweep_forbidden_; }
  bool IsObjectResurrectionForbidden() const {
    return object_resurrection_forbidden_;
  }
  void EnterObjectResurrectionForbiddenScope() {
    DCHECK(!object_resurrection_forbidden_);
    object_resurrection_forbidden_ = true;
  }
  void LeaveObjectResurrectionForbiddenScope() {
    DCHECK(object_resurrection_forbidden_);
    object_resurrection_forbidden_ = false;
  }
  bool in_atomic_pause() const { return in_atomic_pause_; }
  void EnterAtomicPause() {
    DCHECK(!in_atomic_pause_);
    in_atomic_pause_ = true;
  }
  void LeaveAtomicPause() {
    DCHECK(in_atomic_pause_);
    in_atomic_pause_ = false;
  }
  bool InAtomicMarkingPause() const {
    return in_atomic_pause() && IsMarkingInProgress();
  }
  bool InAtomicSweepingPause() const {
    return in_atomic_pause() && IsSweepingInProgress();
  }

  bool IsWrapperTracing() const { return wrapper_tracing_; }
  void SetWrapperTracing(bool value) { wrapper_tracing_ = value; }

  bool IsIncrementalMarking() const { return incremental_marking_; }
  void SetIncrementalMarking(bool value) { incremental_marking_ = value; }

  class MainThreadGCForbiddenScope final {
    STACK_ALLOCATED();

   public:
    MainThreadGCForbiddenScope()
        : thread_state_(ThreadState::MainThreadState()) {
      thread_state_->EnterGCForbiddenScope();
    }
    ~MainThreadGCForbiddenScope() { thread_state_->LeaveGCForbiddenScope(); }

   private:
    ThreadState* const thread_state_;
  };

  class GCForbiddenScope final {
    STACK_ALLOCATED();

   public:
    explicit GCForbiddenScope(ThreadState* thread_state)
        : thread_state_(thread_state) {
      thread_state_->EnterGCForbiddenScope();
    }
    ~GCForbiddenScope() { thread_state_->LeaveGCForbiddenScope(); }

   private:
    ThreadState* const thread_state_;
  };

  // Used to mark when we are in an atomic pause for GC.
  class AtomicPauseScope final {
   public:
    explicit AtomicPauseScope(ThreadState* thread_state)
        : thread_state_(thread_state), gc_forbidden_scope(thread_state) {
      thread_state_->EnterAtomicPause();
    }
    ~AtomicPauseScope() { thread_state_->LeaveAtomicPause(); }

   private:
    ThreadState* const thread_state_;
    ScriptForbiddenScope script_forbidden_scope;
    GCForbiddenScope gc_forbidden_scope;
  };

  void FlushHeapDoesNotContainCacheIfNeeded();

  void SafePoint(BlinkGC::StackState);

  void RecordStackEnd(intptr_t* end_of_stack) { end_of_stack_ = end_of_stack; }
#if HAS_FEATURE(safe_stack)
  void RecordUnsafeStackEnd(intptr_t* end_of_unsafe_stack) {
    end_of_unsafe_stack_ = end_of_unsafe_stack;
  }
#endif

  void PushRegistersAndVisitStack();

  // A region of non-weak PersistentNodes allocated on the given thread.
  PersistentRegion* GetPersistentRegion() const {
    return persistent_region_.get();
  }

  // A region of PersistentNodes for WeakPersistents allocated on the given
  // thread.
  PersistentRegion* GetWeakPersistentRegion() const {
    return weak_persistent_region_.get();
  }

  // Visit local thread stack and trace all pointers conservatively.
  void VisitStack(MarkingVisitor*);

  // Visit the asan fake stack frame corresponding to a slot on the
  // real machine stack if there is one.
  void VisitAsanFakeStackForPointer(MarkingVisitor*, Address);

  // Visit all non-weak persistents allocated on this thread.
  void VisitPersistents(Visitor*);

  // Visit all weak persistents allocated on this thread.
  void VisitWeakPersistents(Visitor*);

  // Visit all DOM wrappers allocatd on this thread.
  void VisitDOMWrappers(Visitor*);

  struct GCSnapshotInfo {
    STACK_ALLOCATED();

   public:
    GCSnapshotInfo(wtf_size_t num_object_types);

    // Map from gcInfoIndex (vector-index) to count/size.
    Vector<int> live_count;
    Vector<int> dead_count;
    Vector<size_t> live_size;
    Vector<size_t> dead_size;
  };

  void RegisterTraceDOMWrappers(
      v8::Isolate* isolate,
      void (*trace_dom_wrappers)(v8::Isolate*, Visitor*),
      void (*invalidate_dead_objects_in_wrappers_marking_deque)(v8::Isolate*),
      void (*perform_cleanup)(v8::Isolate*)) {
    isolate_ = isolate;
    DCHECK(!isolate_ || trace_dom_wrappers);
    trace_dom_wrappers_ = trace_dom_wrappers;
    invalidate_dead_objects_in_wrappers_marking_deque_ =
        invalidate_dead_objects_in_wrappers_marking_deque;
    perform_cleanup_ = perform_cleanup;
  }

  // By entering a gc-forbidden scope, conservative GCs will not
  // be allowed while handling an out-of-line allocation request.
  // Intended used when constructing subclasses of GC mixins, where
  // the object being constructed cannot be safely traced & marked
  // fully should a GC be allowed while its subclasses are being
  // constructed.
  void EnterGCForbiddenScopeIfNeeded(
      GarbageCollectedMixinConstructorMarkerBase* gc_mixin_marker) {
    DCHECK(CheckThread());
    if (!gc_mixin_marker_) {
      EnterMixinConstructionScope();
      gc_mixin_marker_ = gc_mixin_marker;
    }
  }
  void LeaveGCForbiddenScopeIfNeeded(
      GarbageCollectedMixinConstructorMarkerBase* gc_mixin_marker) {
    DCHECK(CheckThread());
    if (gc_mixin_marker_ == gc_mixin_marker) {
      LeaveMixinConstructionScope();
      gc_mixin_marker_ = nullptr;
    }
  }

  void FreePersistentNode(PersistentRegion*, PersistentNode*);

  using PersistentClearCallback = void (*)(void*);

  void RegisterStaticPersistentNode(PersistentNode*, PersistentClearCallback);
  void ReleaseStaticPersistentNodes();

#if defined(LEAK_SANITIZER)
  void enterStaticReferenceRegistrationDisabledScope();
  void leaveStaticReferenceRegistrationDisabledScope();
#endif

  v8::Isolate* GetIsolate() const { return isolate_; }

  void CollectGarbage(BlinkGC::StackState,
                      BlinkGC::MarkingType,
                      BlinkGC::SweepingType,
                      BlinkGC::GCReason);
  void CollectAllGarbage();

  // Register the pre-finalizer for the |self| object. The class T must have
  // USING_PRE_FINALIZER().
  template <typename T>
  class PrefinalizerRegistration final {
   public:
    PrefinalizerRegistration(T* self) {
      static_assert(sizeof(&T::InvokePreFinalizer) > 0,
                    "USING_PRE_FINALIZER(T) must be defined.");
      ThreadState* state =
          ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
#if DCHECK_IS_ON()
      DCHECK(state->CheckThread());
#endif
      DCHECK(!state->SweepForbidden());
      DCHECK(!state->ordered_pre_finalizers_.Contains(
          PreFinalizer(self, T::InvokePreFinalizer)));
      state->ordered_pre_finalizers_.insert(
          PreFinalizer(self, T::InvokePreFinalizer));
    }
  };

  // Returns |true| if |object| resides on this thread's heap.
  // It is well-defined to call this method on any heap allocated
  // reference, provided its associated heap hasn't been detached
  // and shut down. Its behavior is undefined for any other pointer
  // value.
  bool IsOnThreadHeap(const void* object) const {
    return &FromObject(object)->Heap() == &Heap();
  }

  int GcAge() const { return gc_age_; }

  MarkingVisitor* CurrentVisitor() { return current_gc_data_.visitor.get(); }

  // Implementation for WebRAILModeObserver
  void OnRAILModeChanged(v8::RAILMode new_mode) override {
    should_optimize_for_load_time_ = new_mode == v8::RAILMode::PERFORMANCE_LOAD;
    // When switching RAIL mode to load we try to avoid incremental marking as
    // the write barrier cost is noticeable on throughput and garbage
    // accumulated during loading is likely to be alive during that phase. The
    // same argument holds for unified heap garbage collections with the
    // difference that these collections are triggered by V8 and should thus be
    // avoided on that end.
    if (should_optimize_for_load_time_ && IsIncrementalMarking() &&
        !IsUnifiedGCMarkingInProgress() &&
        GetGCState() == GCState::kIncrementalMarkingStepScheduled)
      ScheduleIncrementalMarkingFinalize();
  }

 private:
  // Number of ThreadState's that are currently in incremental marking. The
  // counter is incremented by one when some ThreadState enters incremental
  // marking and decremented upon finishing.
  static base::subtle::AtomicWord incremental_marking_counter_;

  // Same semantic as |incremental_marking_counter_|.
  static base::subtle::AtomicWord wrapper_tracing_counter_;

  ThreadState();
  ~ThreadState() override;

  // The following methods are used to compose RunAtomicPause. Public users
  // should use the CollectGarbage entrypoint. Internal users should use these
  // methods to compose a full garbage collection.
  void AtomicPauseMarkPrologue(BlinkGC::StackState,
                               BlinkGC::MarkingType,
                               BlinkGC::GCReason);
  void AtomicPauseMarkTransitiveClosure();
  void AtomicPauseMarkEpilogue(BlinkGC::MarkingType);
  void AtomicPauseSweepAndCompact(BlinkGC::MarkingType marking_type,
                                  BlinkGC::SweepingType sweeping_type);

  void RunAtomicPause(BlinkGC::StackState,
                      BlinkGC::MarkingType,
                      BlinkGC::SweepingType,
                      BlinkGC::GCReason);

  void UpdateStatisticsAfterSweeping();

  // The version is needed to be able to start incremental marking.
  void MarkPhasePrologue(BlinkGC::StackState,
                         BlinkGC::MarkingType,
                         BlinkGC::GCReason);
  void AtomicPausePrologue(BlinkGC::StackState,
                           BlinkGC::MarkingType,
                           BlinkGC::GCReason);
  void AtomicPauseEpilogue(BlinkGC::MarkingType, BlinkGC::SweepingType);
  void MarkPhaseEpilogue(BlinkGC::MarkingType);
  void MarkPhaseVisitRoots();
  void MarkPhaseVisitNotFullyConstructedObjects();
  bool MarkPhaseAdvanceMarking(TimeTicks deadline);
  void VerifyMarking(BlinkGC::MarkingType);

  bool ShouldVerifyMarking() const;

  // shouldScheduleIdleGC and shouldForceConservativeGC
  // implement the heuristics that are used to determine when to collect
  // garbage.
  // If shouldForceConservativeGC returns true, we force the garbage
  // collection immediately. Otherwise, if should*GC returns true, we
  // record that we should garbage collect the next time we return
  // to the event loop. If both return false, we don't need to
  // collect garbage at this point.
  bool ShouldScheduleIdleGC();
  bool ShouldForceConservativeGC();
  bool ShouldScheduleIncrementalMarking();
  // V8 minor or major GC is likely to drop a lot of references to objects
  // on Oilpan's heap. We give a chance to schedule a GC.
  bool ShouldScheduleV8FollowupGC();
  // Page navigation is likely to drop a lot of references to objects
  // on Oilpan's heap. We give a chance to schedule a GC.
  // estimatedRemovalRatio is the estimated ratio of objects that will be no
  // longer necessary due to the navigation.
  bool ShouldSchedulePageNavigationGC(float estimated_removal_ratio);

  void RescheduleIdleGC();

  // Internal helpers to handle memory pressure conditions.

  // Returns true if memory use is in a near-OOM state
  // (aka being under "memory pressure".)
  bool ShouldForceMemoryPressureGC();

  // Returns true if shouldForceMemoryPressureGC() held and a
  // conservative GC was performed to handle the emergency.
  bool ForceMemoryPressureGCIfNeeded();

  size_t EstimatedLiveSize(size_t current_size, size_t size_at_last_gc);
  size_t TotalMemorySize();
  double HeapGrowingRate();
  double PartitionAllocGrowingRate();
  bool JudgeGCThreshold(size_t allocated_object_size_threshold,
                        size_t total_memory_size_threshold,
                        double heap_growing_rate_threshold);

  void RunScheduledGC(BlinkGC::StackState);

  void UpdateIncrementalMarkingStepDuration();

  void EagerSweep();

  void InvokePreFinalizers();

  void ReportMemoryToV8();


  friend class BlinkGCObserver;

  // Adds the given observer to the ThreadState's observer list. This doesn't
  // take ownership of the argument. The argument must not be null. The argument
  // must not be registered before calling this.
  void AddObserver(BlinkGCObserver*);

  // Removes the given observer from the ThreadState's observer list. This
  // doesn't take ownership of the argument. The argument must not be null.
  // The argument must be registered before calling this.
  void RemoveObserver(BlinkGCObserver*);

  static WTF::ThreadSpecific<ThreadState*>* thread_specific_;

  // We can't create a static member of type ThreadState here
  // because it will introduce global constructor and destructor.
  // We would like to manage lifetime of the ThreadState attached
  // to the main thread explicitly instead and still use normal
  // constructor and destructor for the ThreadState class.
  // For this we reserve static storage for the main ThreadState
  // and lazily construct ThreadState in it using placement new.
  static uint8_t main_thread_state_storage_[];

  std::unique_ptr<ThreadHeap> heap_;
  ThreadIdentifier thread_;
  std::unique_ptr<PersistentRegion> persistent_region_;
  std::unique_ptr<PersistentRegion> weak_persistent_region_;
  intptr_t* start_of_stack_;
  intptr_t* end_of_stack_;

#if HAS_FEATURE(safe_stack)
  intptr_t* start_of_unsafe_stack_;
  intptr_t* end_of_unsafe_stack_;
#endif

  bool sweep_forbidden_;
  size_t no_allocation_count_;
  size_t gc_forbidden_count_;
  size_t mixins_being_constructed_count_;
  bool object_resurrection_forbidden_;
  bool in_atomic_pause_;

  TimeDelta next_incremental_marking_step_duration_;
  TimeDelta previous_incremental_marking_time_left_;

  GarbageCollectedMixinConstructorMarkerBase* gc_mixin_marker_;

  GCState gc_state_;
  GCPhase gc_phase_;
  BlinkGC::GCReason reason_for_scheduled_gc_;

  bool should_optimize_for_load_time_;

  using PreFinalizerCallback = bool (*)(void*);
  using PreFinalizer = std::pair<void*, PreFinalizerCallback>;

  // Pre-finalizers are called in the reverse order in which they are
  // registered by the constructors (including constructors of Mixin objects)
  // for an object, by processing the ordered_pre_finalizers_ back-to-front.
  LinkedHashSet<PreFinalizer> ordered_pre_finalizers_;

  v8::Isolate* isolate_;
  void (*trace_dom_wrappers_)(v8::Isolate*, Visitor*);
  void (*invalidate_dead_objects_in_wrappers_marking_deque_)(v8::Isolate*);
  void (*perform_cleanup_)(v8::Isolate*);
  bool wrapper_tracing_;
  bool incremental_marking_;

#if defined(ADDRESS_SANITIZER)
  void* asan_fake_stack_;
#endif

  HashSet<BlinkGCObserver*> observers_;

  // PersistentNodes that are stored in static references;
  // references that either have to be cleared upon the thread
  // detaching from Oilpan and shutting down or references we
  // have to clear before initiating LSan's leak detection.
  HashMap<PersistentNode*, PersistentClearCallback> static_persistents_;

#if defined(LEAK_SANITIZER)
  // Count that controls scoped disabling of persistent registration.
  size_t disabled_static_persistent_registration_;
#endif

  size_t reported_memory_to_v8_;

  int gc_age_ = 0;

  struct GCData {
    BlinkGC::StackState stack_state;
    BlinkGC::MarkingType marking_type;
    BlinkGC::GCReason reason;
    std::unique_ptr<MarkingVisitor> visitor;
  };
  GCData current_gc_data_;

  // Needs to set up visitor for testing purposes.
  friend class incremental_marking_test::IncrementalMarkingScope;
  friend class incremental_marking_test::IncrementalMarkingTestDriver;
  template <typename T>
  friend class PrefinalizerRegistration;
  friend class TestGCScope;
  friend class ThreadStateSchedulingTest;
  friend class UnifiedHeapController;

  DISALLOW_COPY_AND_ASSIGN(ThreadState);
};

template <>
class ThreadStateFor<kMainThreadOnly> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ThreadState* GetState() {
    // This specialization must only be used from the main thread.
    DCHECK(ThreadState::Current()->IsMainThread());
    return ThreadState::MainThreadState();
  }
};

template <>
class ThreadStateFor<kAnyThread> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ThreadState* GetState() { return ThreadState::Current(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_H_
