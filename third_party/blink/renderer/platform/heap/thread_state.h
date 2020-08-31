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

#include <atomic>
#include <memory>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/task/post_job.h"
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

class MarkingVisitor;
class MarkingSchedulingOracle;
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
#define USING_PRE_FINALIZER(Class, PreFinalizer)                             \
 public:                                                                     \
  static bool InvokePreFinalizer(const LivenessBroker& info, void* object) { \
    Class* self = reinterpret_cast<Class*>(object);                          \
    if (info.IsHeapObjectAlive(self))                                        \
      return false;                                                          \
    self->Class::PreFinalizer();                                             \
    return true;                                                             \
  }                                                                          \
                                                                             \
 private:                                                                    \
  ThreadState::PrefinalizerRegistration<Class> prefinalizer_dummy_{this};    \
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
  class PrefinalizerRegistration final {
    DISALLOW_NEW();

   public:
    PrefinalizerRegistration(T* self) {  // NOLINT
      static_assert(sizeof(&T::InvokePreFinalizer) > 0,
                    "USING_PRE_FINALIZER(T) must be defined.");
      ThreadState* state =
          ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
#if DCHECK_IS_ON()
      DCHECK(state->CheckThread());
#endif
      DCHECK(!state->SweepForbidden());
      DCHECK(std::find(state->ordered_pre_finalizers_.begin(),
                       state->ordered_pre_finalizers_.end(),
                       PreFinalizer(self, T::InvokePreFinalizer)) ==
             state->ordered_pre_finalizers_.end());
      state->ordered_pre_finalizers_.emplace_back(self, T::InvokePreFinalizer);
    }
  };

  // See setGCState() for possible state transitions.
  enum GCState {
    kNoGCScheduled,
    kIncrementalMarkingStepPaused,
    kIncrementalMarkingStepScheduled,
    kIncrementalMarkingFinalizeScheduled,
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

  enum class EphemeronProcessing {
    kPartialProcessing,  // Perofrm one ephemeron processing iteration every
                         // few step
    kFullProcessing  // Perofrm full fixed-point ephemeron processing on each
                     // step
  };

  class AtomicPauseScope;
  class GCForbiddenScope;
  class LsanDisabledScope;
  class NoAllocationScope;
  class StatisticsCollector;
  struct Statistics;
  class SweepForbiddenScope;
  class HeapPointersOnStackScope;

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
  void PerformConcurrentSweep(base::JobDelegate*);

  void ScheduleForcedGCForTesting();
  void ScheduleGCIfNeeded();
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
               BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC ||
           current_gc_data_.reason ==
               BlinkGC::GCReason::kUnifiedHeapForcedForTestingGC;
  }

  bool IsUnifiedHeapGC() const {
    return current_gc_data_.reason == BlinkGC::GCReason::kUnifiedHeapGC ||
           current_gc_data_.reason ==
               BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC ||
           current_gc_data_.reason ==
               BlinkGC::GCReason::kUnifiedHeapForcedForTestingGC;
  }

  bool FinishIncrementalMarkingIfRunning(BlinkGC::CollectionType,
                                         BlinkGC::StackState,
                                         BlinkGC::MarkingType,
                                         BlinkGC::SweepingType,
                                         BlinkGC::GCReason);

  void EnableIncrementalMarkingBarrier();
  void DisableIncrementalMarkingBarrier();

  void RestartIncrementalMarkingIfPaused();

  void CompleteSweep();

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

  // Returns |true| if |object| resides on this thread's heap.
  // It is well-defined to call this method on any heap allocated
  // reference, provided its associated heap hasn't been detached
  // and shut down. Its behavior is undefined for any other pointer
  // value.
  bool IsOnThreadHeap(const void* object) const {
    return &FromObject(object)->Heap() == &Heap();
  }

  ALWAYS_INLINE bool IsOnStack(Address address) const {
    return reinterpret_cast<Address>(start_of_stack_) >= address &&
           address >= (reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(
                          WTF::GetCurrentStackPosition())));
  }

  int GcAge() const { return gc_age_; }

  MarkingVisitor* CurrentVisitor() const {
    return current_gc_data_.visitor.get();
  }

  // Returns true if the marking verifier is enabled, false otherwise.
  bool IsVerifyMarkingEnabled() const;

  void SkipIncrementalMarkingForTesting() {
    skip_incremental_marking_for_testing_ = true;
  }

  // Performs stand-alone garbage collections considering only C++ objects for
  // testing.
  //
  // Since it only considers C++ objects this type of GC is mostly useful for
  // unit tests.
  void CollectGarbageForTesting(BlinkGC::CollectionType,
                                BlinkGC::StackState,
                                BlinkGC::MarkingType,
                                BlinkGC::SweepingType,
                                BlinkGC::GCReason);

  // Forced garbage collection for testing:
  // - Performs unified heap garbage collections if ThreadState is attached to a
  //   v8::Isolate using ThreadState::AttachToIsolate.
  // - Otherwise, performs stand-alone garbage collections.
  // - Collects garbage as long as live memory decreases (capped at 5).
  void CollectAllGarbageForTesting(
      BlinkGC::StackState stack_state =
          BlinkGC::StackState::kNoHeapPointersOnStack);

  // Enables compaction for next garbage collection.
  void EnableCompactionForNextGCForTesting();

  bool RequiresForcedGCForTesting() const {
    return current_gc_data_.stack_state ==
               BlinkGC::StackState::kHeapPointersOnStack &&
           !forced_scheduled_gc_for_testing_;
  }

  void EnterNoHeapVerificationScopeForTesting() {
    ++disable_heap_verification_scope_;
  }
  void LeaveNoHeapVerificationScopeForTesting() {
    --disable_heap_verification_scope_;
  }

 private:
  class IncrementalMarkingScheduler;

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

  static bool IsForcedGC(BlinkGC::GCReason reason) {
    return reason == BlinkGC::GCReason::kThreadTerminationGC ||
           reason == BlinkGC::GCReason::kForcedGCForTesting ||
           reason == BlinkGC::GCReason::kUnifiedHeapForcedForTestingGC;
  }

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

  // Performs stand-alone garbage collections considering only C++ objects.
  //
  // Use the public *ForTesting calls for calling GC in tests.
  void CollectGarbage(BlinkGC::CollectionType,
                      BlinkGC::StackState,
                      BlinkGC::MarkingType,
                      BlinkGC::SweepingType,
                      BlinkGC::GCReason);

  // The following methods are used to compose RunAtomicPause. Public users
  // should use the CollectGarbage entrypoint. Internal users should use these
  // methods to compose a full garbage collection.
  void AtomicPauseMarkPrologue(BlinkGC::CollectionType,
                               BlinkGC::StackState,
                               BlinkGC::MarkingType,
                               BlinkGC::GCReason);
  void AtomicPauseMarkRoots(BlinkGC::StackState,
                            BlinkGC::MarkingType,
                            BlinkGC::GCReason);
  void AtomicPauseMarkTransitiveClosure();
  void AtomicPauseMarkEpilogue(BlinkGC::MarkingType);
  void AtomicPauseSweepAndCompact(BlinkGC::CollectionType,
                                  BlinkGC::MarkingType marking_type,
                                  BlinkGC::SweepingType sweeping_type);
  void AtomicPauseEpilogue();

  // RunAtomicPause composes the final atomic pause that finishes a mark-compact
  // phase of a garbage collection. Depending on SweepingType it may also finish
  // sweeping or schedule lazy/concurrent sweeping.
  void RunAtomicPause(BlinkGC::CollectionType,
                      BlinkGC::StackState,
                      BlinkGC::MarkingType,
                      BlinkGC::SweepingType,
                      BlinkGC::GCReason);

  // The version is needed to be able to start incremental marking.
  void MarkPhasePrologue(BlinkGC::CollectionType,
                         BlinkGC::StackState,
                         BlinkGC::MarkingType,
                         BlinkGC::GCReason);
  void MarkPhaseEpilogue(BlinkGC::MarkingType);
  void MarkPhaseVisitRoots();
  void MarkPhaseVisitNotFullyConstructedObjects();
  bool MarkPhaseAdvanceMarkingBasedOnSchedule(base::TimeDelta,
                                              EphemeronProcessing);
  bool MarkPhaseAdvanceMarking(base::TimeDelta, EphemeronProcessing);
  void VerifyMarking(BlinkGC::MarkingType);

  // Visit the stack after pushing registers onto the stack.
  void PushRegistersAndVisitStack();

  // Visit local thread stack and trace all pointers conservatively. Never call
  // directly but always call through |PushRegistersAndVisitStack|.
  void VisitStackImpl(MarkingVisitor*, Address*, Address*);
  void VisitStack(MarkingVisitor*, Address*);
  void VisitUnsafeStack(MarkingVisitor*);

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

  // Visit card tables (remembered sets) containing inter-generational pointers.
  void VisitRememberedSets(MarkingVisitor*);

  // Incremental marking implementation functions.
  void IncrementalMarkingStartForTesting();
  void IncrementalMarkingStart(BlinkGC::GCReason);
  // Incremental marking step advance marking on the mutator thread. This method
  // also reschedules concurrent marking tasks if needed. The duration parameter
  // applies only to incremental marking steps on the mutator thread.
  void IncrementalMarkingStep(BlinkGC::StackState);
  void IncrementalMarkingFinalize();

  // Returns true if concurrent marking is finished (i.e. all current threads
  // terminated and the worklist is empty)
  bool ConcurrentMarkingStep();
  void ScheduleConcurrentMarking();
  void PerformConcurrentMark(base::JobDelegate* job);

  // Schedule helpers.
  void ScheduleIdleLazySweep();
  void ScheduleConcurrentAndLazySweep();

  void NotifySweepDone();
  void PostSweep();

  // See |DetachCurrentThread|.
  void RunTerminationGC();

  void RunScheduledGC(BlinkGC::StackState);

  void SynchronizeAndFinishConcurrentSweeping();

  void InvokePreFinalizers();

  // Adds the given observer to the ThreadState's observer list. This doesn't
  // take ownership of the argument. The argument must not be null. The argument
  // must not be registered before calling this.
  void AddObserver(BlinkGCObserver*);

  // Removes the given observer from the ThreadState's observer list. This
  // doesn't take ownership of the argument. The argument must not be null.
  // The argument must be registered before calling this.
  void RemoveObserver(BlinkGCObserver*);

  bool IsForcedGC() const { return IsForcedGC(current_gc_data_.reason); }

  // Returns whether stack scanning is forced. This is currently only used in
  // platform tests where non nested tasks can be run with heap pointers on
  // stack.
  bool HeapPointersOnStackForced() const {
    return heap_pointers_on_stack_forced_;
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
  bool heap_pointers_on_stack_forced_ = false;
  bool incremental_marking_ = false;
  bool should_optimize_for_load_time_ = false;
  bool forced_scheduled_gc_for_testing_ = false;
  size_t no_allocation_count_ = 0;
  size_t gc_forbidden_count_ = 0;
  size_t static_persistent_registration_disabled_count_ = 0;

  GCState gc_state_ = GCState::kNoGCScheduled;
  GCPhase gc_phase_ = GCPhase::kNone;
  BlinkGC::GCReason reason_for_scheduled_gc_ =
      BlinkGC::GCReason::kForcedGCForTesting;

  using PreFinalizerCallback = bool (*)(const LivenessBroker&, void*);
  using PreFinalizer = std::pair<void*, PreFinalizerCallback>;

  // Pre-finalizers are called in the reverse order in which they are
  // registered by the constructors (including constructors of Mixin objects)
  // for an object, by processing the ordered_pre_finalizers_ back-to-front.
  Deque<PreFinalizer> ordered_pre_finalizers_;

  v8::Isolate* isolate_ = nullptr;
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
    BlinkGC::CollectionType collection_type;
    BlinkGC::StackState stack_state;
    BlinkGC::MarkingType marking_type;
    BlinkGC::GCReason reason;
    std::unique_ptr<MarkingVisitor> visitor;
  };
  GCData current_gc_data_;

  std::unique_ptr<IncrementalMarkingScheduler> incremental_marking_scheduler_;
  std::unique_ptr<MarkingSchedulingOracle> marking_scheduling_;

  base::JobHandle marker_handle_;

  base::JobHandle sweeper_handle_;
  std::atomic_bool has_unswept_pages_{false};

  size_t disable_heap_verification_scope_ = 0;

  bool skip_incremental_marking_for_testing_ = false;

  size_t last_concurrently_marked_bytes_ = 0;
  base::TimeTicks last_concurrently_marked_bytes_update_;
  bool concurrent_marking_priority_increased_ = false;

  friend class BlinkGCObserver;
  friend class incremental_marking_test::IncrementalMarkingScope;
  friend class IncrementalMarkingTestDriver;
  friend class HeapAllocator;
  template <typename T>
  friend class PrefinalizerRegistration;
  friend class TestGCScope;
  friend class TestSupportingGC;
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
