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

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/heap/atomic_entry_flag.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/threading_traits.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace v8 {
class EmbedderGraph;
class Isolate;
}  // namespace v8

namespace blink {

namespace incremental_marking_test {
class IncrementalMarkingScope;
}  // namespace incremental_marking_test

class CancelableTaskScheduler;
class HeapObjectHeader;
class MarkingVisitor;
class PersistentNode;
class PersistentRegion;
class ThreadHeap;
class ThreadState;
template <ThreadAffinity affinity>
class ThreadStateFor;
class UnifiedHeapController;
class Visitor;

// Declare that a class has a pre-finalizer which gets invoked before objects
// get swept. It is thus safe to touch on-heap objects that may be collected in
// the same GC cycle. This is useful when it's not possible to avoid touching
// on-heap objects in a destructor which is forbidden.
//
// Note that:
// (a) Pre-finalizers *must* not resurrect dead objects.
// (b) Run on the same thread they are registered.
// (c) Decrease GC performance which means that they should only be used if
//     absolute necessary.
//
// Usage:
//   class Foo : GarbageCollected<Foo> {
//     USING_PRE_FINALIZER(Foo, Dispose);
//    private:
//     void Dispose() {
//       bar_->...; // It is safe to touch other on-heap objects.
//     }
//     Member<Bar> bar_;
//   };
#define USING_PRE_FINALIZER(Class, preFinalizer)                          \
 private:                                                                 \
  static void PreFinalizerDispatch(void* object) {                        \
    reinterpret_cast<Class*>(object)->Class::preFinalizer();              \
  }                                                                       \
                                                                          \
  friend class ThreadState::PreFinalizerRegistration<Class>;              \
  ThreadState::PreFinalizerRegistration<Class> prefinalizer_dummy_{this}; \
  using UsingPreFinalizerMacroNeedsTrailingSemiColon = char

class PLATFORM_EXPORT BlinkGCObserver {
  USING_FAST_MALLOC(BlinkGCObserver);

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

class PLATFORM_EXPORT ThreadState final {
  USING_FAST_MALLOC(ThreadState);

 public:
  // Register the pre-finalizer for the |self| object. The class T be using
  // USING_PRE_FINALIZER() macro.
  template <typename T>
  class PreFinalizerRegistration final {
    DISALLOW_NEW();

   public:
    PreFinalizerRegistration(T* self) {
      static_assert(sizeof(&T::PreFinalizerDispatch) > 0,
                    "USING_PRE_FINALIZER(T) must be defined.");
      ThreadState* state =
          ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
      state->RegisterPreFinalizer(self, T::PreFinalizerDispatch);
    }
  };

  // See setGCState() for possible state transitions.
  enum GCState {
    kNoGCScheduled,
    kIncrementalMarkingStepPaused,
    kIncrementalMarkingStepScheduled,
    kIncrementalMarkingFinalizeScheduled,
    kPreciseGCScheduled,
    kForcedGCForTestingScheduled,
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

  class AtomicPauseScope;
  class GCForbiddenScope;
  class LsanDisabledScope;
  class MainThreadGCForbiddenScope;
  class NoAllocationScope;
  class StatisticsCollector;
  struct Statistics;
  class SweepForbiddenScope;

  using V8TraceRootsCallback = void (*)(v8::Isolate*, Visitor*);
  using V8BuildEmbedderGraphCallback = void (*)(v8::Isolate*,
                                                v8::EmbedderGraph*,
                                                void*);

  // Returns true if some thread (possibly the current thread) may be doing
  // incremental marking. If false is returned, the *current* thread is
  // definitely not doing incremental marking. See atomic_entry_flag.h for
  // details.
  //
  // For an exact check, use ThreadState::IsIncrementalMarking.
  ALWAYS_INLINE static bool IsAnyIncrementalMarking() {
    return incremental_marking_flag_.MightBeEntered();
  }

  static ThreadState* AttachMainThread();

  // Associate ThreadState object with the current thread. After this
  // call thread can start using the garbage collected heap infrastructure.
  // It also has to periodically check for safepoints.
  static ThreadState* AttachCurrentThread();

  // Disassociate attached ThreadState from the current thread. The thread
  // can no longer use the garbage collected heap after this call.
  //
  // When ThreadState is detaching from non-main thread its heap is expected to
  // be empty (because it is going away). Perform registered cleanup tasks and
  // garbage collection to sweep away any objects that are left on this heap.
  //
  // This method asserts that no objects remain after this cleanup. If assertion
  // does not hold we crash as we are potentially in the dangling pointer
  // situation.
  static void DetachCurrentThread();

  static ThreadState* Current() { return **thread_specific_; }

  static ThreadState* MainThreadState() {
    return reinterpret_cast<ThreadState*>(main_thread_state_storage_);
  }

  static ThreadState* FromObject(const void*);

  bool IsMainThread() const { return this == MainThreadState(); }
  bool CheckThread() const { return thread_ == CurrentThread(); }

  ThreadHeap& Heap() const { return *heap_; }
  base::PlatformThreadId ThreadId() const { return thread_; }

  // Associates |ThreadState| with a given |v8::Isolate|, essentially tying
  // there garbage collectors together.
  void AttachToIsolate(v8::Isolate*,
                       V8TraceRootsCallback,
                       V8BuildEmbedderGraphCallback);

  // Removes the association from a potentially attached |v8::Isolate|.
  void DetachFromIsolate();

  // Returns an |UnifiedHeapController| if ThreadState is attached to a V8
  // isolate (see |AttachToIsolate|) and nullptr otherwise.
  UnifiedHeapController* unified_heap_controller() const {
    DCHECK(isolate_);
    return unified_heap_controller_.get();
  }

  void PerformIdleLazySweep(base::TimeTicks deadline);
  void PerformConcurrentSweep();

  void SchedulePreciseGC();
  void ScheduleForcedGCForTesting();
  void ScheduleGCIfNeeded();
  void WillStartV8GC(BlinkGC::V8GCType);
  void SetGCState(GCState);
  GCState GetGCState() const { return gc_state_; }
  void SetGCPhase(GCPhase);

  // Immediately starts incremental marking and schedules further steps if
  // necessary.
  void StartIncrementalMarking(BlinkGC::GCReason);

  // Returns true if marking is in progress.
  bool IsMarkingInProgress() const { return gc_phase_ == GCPhase::kMarking; }

  // Returns true if unified heap marking is in progress.
  bool IsUnifiedGCMarkingInProgress() const {
    return IsMarkingInProgress() && IsUnifiedHeapGC();
  }

  // Returns true if sweeping is in progress.
  bool IsSweepingInProgress() const { return gc_phase_ == GCPhase::kSweeping; }

  // Returns true if the current GC is a memory reducing GC.
  bool IsMemoryReducingGC() const {
    return current_gc_data_.reason ==
           BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC;
  }

  bool IsUnifiedHeapGC() const {
    return current_gc_data_.reason == BlinkGC::GCReason::kUnifiedHeapGC ||
           current_gc_data_.reason ==
               BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC;
  }

  void EnableCompactionForNextGCForTesting();

  bool FinishIncrementalMarkingIfRunning(BlinkGC::StackState,
                                         BlinkGC::MarkingType,
                                         BlinkGC::SweepingType,
                                         BlinkGC::GCReason);

  void EnableIncrementalMarkingBarrier();
  void DisableIncrementalMarkingBarrier();

  // Returns true if concurrent marking is finished (i.e. all current threads
  // terminated and the worklist is empty)
  bool ConcurrentMarkingStep();
  void ScheduleConcurrentMarking();
  void PerformConcurrentMark();

  void CompleteSweep();
  void NotifySweepDone();
  void PostSweep();

  // Returns whether it is currently allowed to allocate an object. Mainly used
  // for sanity checks asserts.
  bool IsAllocationAllowed() const {
    // Allocation is not allowed during atomic marking pause, but it is allowed
    // during atomic sweeping pause.
    return !InAtomicMarkingPause() && !no_allocation_count_;
  }

  // Returns whether it is currently forbidden to trigger a GC.
  bool IsGCForbidden() const { return gc_forbidden_count_; }

  // Returns whether it is currently forbidden to sweep objects.
  bool SweepForbidden() const { return sweep_forbidden_; }

  bool in_atomic_pause() const { return in_atomic_pause_; }

  bool InAtomicMarkingPause() const {
    return in_atomic_pause() && IsMarkingInProgress();
  }
  bool InAtomicSweepingPause() const {
    return in_atomic_pause() && IsSweepingInProgress();
  }

  bool IsIncrementalMarking() const { return incremental_marking_; }
  void SetIncrementalMarking(bool value) { incremental_marking_ = value; }

  void SafePoint(BlinkGC::StackState);

  // A region of non-weak PersistentNodes allocated on the given thread.
  PersistentRegion* GetPersistentRegion() const {
    return persistent_region_.get();
  }

  // A region of PersistentNodes for WeakPersistents allocated on the given
  // thread.
  PersistentRegion* GetWeakPersistentRegion() const {
    return weak_persistent_region_.get();
  }

  void RegisterStaticPersistentNode(PersistentNode*);
  void ReleaseStaticPersistentNodes();
  void FreePersistentNode(PersistentRegion*, PersistentNode*);

  v8::Isolate* GetIsolate() const { return isolate_; }

  // Use CollectAllGarbageForTesting below for testing!
  void CollectGarbage(BlinkGC::StackState,
                      BlinkGC::MarkingType,
                      BlinkGC::SweepingType,
                      BlinkGC::GCReason);

  // Forced garbage collection for testing.
  void CollectAllGarbageForTesting(
      BlinkGC::StackState stack_state =
          BlinkGC::StackState::kNoHeapPointersOnStack);

  // Returns |true| if |object| resides on this thread's heap.
  // It is well-defined to call this method on any heap allocated
  // reference, provided its associated heap hasn't been detached
  // and shut down. Its behavior is undefined for any other pointer
  // value.
  bool IsOnThreadHeap(const void* object) const {
    return &FromObject(object)->Heap() == &Heap();
  }

  int GcAge() const { return gc_age_; }

  MarkingVisitor* CurrentVisitor() const {
    return current_gc_data_.visitor.get();
  }

  // Returns true if the marking verifier is enabled, false otherwise.
  bool IsVerifyMarkingEnabled() const;

 private:
  class IncrementalMarkingScheduler;

  using PreFinalizerCallback = void (*)(void*);
  struct PreFinalizer {
    HeapObjectHeader* header;
    void* object;
    PreFinalizerCallback callback;

    bool operator==(const PreFinalizer& other) const {
      return object == other.object && callback == other.callback;
    }
  };

  // Duration of one incremental marking step. Should be short enough that it
  // doesn't cause jank even though it is scheduled as a normal task.
  static constexpr base::TimeDelta kDefaultIncrementalMarkingStepDuration =
      base::TimeDelta::FromMilliseconds(2);

  // Stores whether some ThreadState is currently in incremental marking.
  static AtomicEntryFlag incremental_marking_flag_;

  static WTF::ThreadSpecific<ThreadState*>* thread_specific_;

  // We can't create a static member of type ThreadState here because it will
  // introduce global constructor and destructor. We would like to manage
  // lifetime of the ThreadState attached to the main thread explicitly instead
  // and still use normal constructor and destructor for the ThreadState class.
  // For this we reserve static storage for the main ThreadState and lazily
  // construct ThreadState in it using placement new.
  static uint8_t main_thread_state_storage_[];

  // Callback executed directly after pushing all callee-saved registers.
  // |end_of_stack| denotes the end of the stack that can hold references to
  // managed objects.
  static void VisitStackAfterPushingRegisters(ThreadState*,
                                              intptr_t* end_of_stack);

  ThreadState();
  ~ThreadState();

  void EnterNoAllocationScope() { no_allocation_count_++; }
  void LeaveNoAllocationScope() { no_allocation_count_--; }

  void EnterAtomicPause() {
    DCHECK(!in_atomic_pause_);
    in_atomic_pause_ = true;
  }
  void LeaveAtomicPause() {
    DCHECK(in_atomic_pause_);
    in_atomic_pause_ = false;
  }

  void EnterGCForbiddenScope() { gc_forbidden_count_++; }
  void LeaveGCForbiddenScope() {
    DCHECK_GT(gc_forbidden_count_, 0u);
    gc_forbidden_count_--;
  }

  void EnterStaticReferenceRegistrationDisabledScope();
  void LeaveStaticReferenceRegistrationDisabledScope();

  // The following methods are used to compose RunAtomicPause. Public users
  // should use the CollectGarbage entrypoint. Internal users should use these
  // methods to compose a full garbage collection.
  void AtomicPauseMarkPrologue(BlinkGC::StackState,
                               BlinkGC::MarkingType,
                               BlinkGC::GCReason);
  void AtomicPauseMarkRoots(BlinkGC::StackState,
                            BlinkGC::MarkingType,
                            BlinkGC::GCReason);
  void AtomicPauseMarkTransitiveClosure();
  void AtomicPauseMarkEpilogue(BlinkGC::MarkingType);
  void AtomicPauseSweepAndCompact(BlinkGC::MarkingType marking_type,
                                  BlinkGC::SweepingType sweeping_type);
  void AtomicPauseEpilogue();

  // RunAtomicPause composes the final atomic pause that finishes a mark-compact
  // phase of a garbage collection. Depending on SweepingType it may also finish
  // sweeping or schedule lazy/concurrent sweeping.
  void RunAtomicPause(BlinkGC::StackState,
                      BlinkGC::MarkingType,
                      BlinkGC::SweepingType,
                      BlinkGC::GCReason);

  // The version is needed to be able to start incremental marking.
  void MarkPhasePrologue(BlinkGC::StackState,
                         BlinkGC::MarkingType,
                         BlinkGC::GCReason);
  void MarkPhaseEpilogue(BlinkGC::MarkingType);
  void MarkPhaseVisitRoots();
  void MarkPhaseVisitNotFullyConstructedObjects();
  bool MarkPhaseAdvanceMarking(base::TimeTicks deadline);
  void VerifyMarking(BlinkGC::MarkingType);

  // Visit the stack after pushing registers onto the stack.
  void PushRegistersAndVisitStack();

  // Visit local thread stack and trace all pointers conservatively. Never call
  // directly but always call through |PushRegistersAndVisitStack|.
  void VisitStack(MarkingVisitor*, Address*);

  // Visit the asan fake stack frame corresponding to a slot on the real machine
  // stack if there is one. Never call directly but always call through
  // |PushRegistersAndVisitStack|.
  void VisitAsanFakeStackForPointer(MarkingVisitor*,
                                    Address,
                                    Address*,
                                    Address*);

  // Visit all non-weak persistents allocated on this thread.
  void VisitPersistents(Visitor*);

  // Visit all weak persistents allocated on this thread.
  void VisitWeakPersistents(Visitor*);

  // Visit all DOM wrappers allocatd on this thread.
  void VisitDOMWrappers(Visitor*);

  // Incremental marking implementation functions.
  void IncrementalMarkingStartForTesting();
  void IncrementalMarkingStart(BlinkGC::GCReason);
  void IncrementalMarkingStep(
      BlinkGC::StackState,
      base::TimeDelta duration = kDefaultIncrementalMarkingStepDuration);
  void IncrementalMarkingFinalize();

  // Schedule helpers.
  void ScheduleIdleLazySweep();
  void ScheduleConcurrentAndLazySweep();

  // See |DetachCurrentThread|.
  void RunTerminationGC();

  void RunScheduledGC(BlinkGC::StackState);

  void SynchronizeAndFinishConcurrentSweeping();

  void RegisterPreFinalizer(void*, PreFinalizerCallback);
  void InvokePreFinalizers();

  // Adds the given observer to the ThreadState's observer list. This doesn't
  // take ownership of the argument. The argument must not be null. The argument
  // must not be registered before calling this.
  void AddObserver(BlinkGCObserver*);

  // Removes the given observer from the ThreadState's observer list. This
  // doesn't take ownership of the argument. The argument must not be null.
  // The argument must be registered before calling this.
  void RemoveObserver(BlinkGCObserver*);

  bool IsForcedGC(BlinkGC::GCReason reason) const {
    return reason == BlinkGC::GCReason::kThreadTerminationGC ||
           reason == BlinkGC::GCReason::kForcedGCForTesting;
  }

#if defined(ADDRESS_SANITIZER)
  // Poisons payload of unmarked objects.
  //
  // Also unpoisons memory areas for handles that may require resetting which
  // can race with destructors. Note that cross-thread access still requires
  // synchronization using a lock.
  void PoisonUnmarkedObjects();
#endif  // ADDRESS_SANITIZER

  std::unique_ptr<ThreadHeap> heap_;
  base::PlatformThreadId thread_;
  std::unique_ptr<PersistentRegion> persistent_region_;
  std::unique_ptr<PersistentRegion> weak_persistent_region_;

  // Start of the stack which is the boundary until conservative stack scanning
  // needs to search for managed pointers.
  Address* start_of_stack_;

  bool in_atomic_pause_ = false;
  bool sweep_forbidden_ = false;
  bool incremental_marking_ = false;
  bool should_optimize_for_load_time_ = false;
  size_t no_allocation_count_ = 0;
  size_t gc_forbidden_count_ = 0;
  size_t static_persistent_registration_disabled_count_ = 0;

  GCState gc_state_ = GCState::kNoGCScheduled;
  GCPhase gc_phase_ = GCPhase::kNone;
  BlinkGC::GCReason reason_for_scheduled_gc_ =
      BlinkGC::GCReason::kForcedGCForTesting;

  // Pre-finalizers are called in the reverse order in which they are
  // registered by the constructors (including constructors of Mixin objects)
  // for an object, by processing the ordered_pre_finalizers_ back-to-front.
  Deque<PreFinalizer> ordered_pre_finalizers_;

  v8::Isolate* isolate_ = nullptr;
  V8TraceRootsCallback v8_trace_roots_ = nullptr;
  V8BuildEmbedderGraphCallback v8_build_embedder_graph_ = nullptr;
  std::unique_ptr<UnifiedHeapController> unified_heap_controller_;

#if defined(ADDRESS_SANITIZER)
  void* asan_fake_stack_;
#endif

  HashSet<BlinkGCObserver*> observers_;

  // PersistentNodes that are stored in static references;
  // references that either have to be cleared upon the thread
  // detaching from Oilpan and shutting down or references we
  // have to clear before initiating LSan's leak detection.
  HashSet<PersistentNode*> static_persistents_;

  int gc_age_ = 0;

  struct GCData {
    BlinkGC::StackState stack_state;
    BlinkGC::MarkingType marking_type;
    BlinkGC::GCReason reason;
    std::unique_ptr<MarkingVisitor> visitor;
  };
  GCData current_gc_data_;

  std::unique_ptr<IncrementalMarkingScheduler> incremental_marking_scheduler_;

  std::unique_ptr<CancelableTaskScheduler> marker_scheduler_;
  Vector<uint8_t> available_concurrent_marking_task_ids_;
  uint8_t active_markers_ = 0;
  base::Lock concurrent_marker_bootstrapping_lock_;
  size_t concurrently_marked_bytes_ = 0;

  std::unique_ptr<CancelableTaskScheduler> sweeper_scheduler_;

  friend class BlinkGCObserver;
  friend class incremental_marking_test::IncrementalMarkingScope;
  friend class IncrementalMarkingTestDriver;
  friend class HeapAllocator;
  template <typename T>
  friend class PreFinalizerRegistration;
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
