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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_H_

#include <limits>
#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/finalizer_traits.h"
#include "third_party/blink/renderer/platform/heap/gc_info.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state_statistics.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/heap/worklist.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"

namespace blink {

namespace incremental_marking_test {
class IncrementalMarkingScopeBase;
}  // namespace incremental_marking_test

class ConcurrentMarkingVisitor;
class ThreadHeapStatsCollector;
class PageBloomFilter;
class PagePool;
class ProcessHeapReporter;
class RegionTree;
class MarkingSchedulingOracle;

using MarkingItem = TraceDescriptor;
using NotFullyConstructedItem = const void*;

struct EphemeronPairItem {
  const void* key;
  const void* value;
  TraceCallback value_trace_callback;
};

struct CustomCallbackItem {
  WeakCallback callback;
  const void* parameter;
};

struct NotSafeToConcurrentlyTraceItem {
  TraceDescriptor desc;
  size_t bailout_size;
};

using V8Reference = const TraceWrapperV8Reference<v8::Value>*;

// Segment size of 512 entries necessary to avoid throughput regressions. Since
// the work list is currently a temporary object this is not a problem.
using MarkingWorklist = Worklist<MarkingItem, 512 /* local entries */>;
using WriteBarrierWorklist = Worklist<HeapObjectHeader*, 64>;
using NotFullyConstructedWorklist =
    Worklist<NotFullyConstructedItem, 16 /* local entries */>;
using WeakCallbackWorklist =
    Worklist<CustomCallbackItem, 64 /* local entries */>;
// Using large local segments here (sized 512 entries) to avoid throughput
// regressions.
using MovableReferenceWorklist =
    Worklist<const MovableReference*, 256 /* local entries */>;
using EphemeronPairsWorklist =
    Worklist<EphemeronPairItem, 64 /* local entries */>;
using V8ReferencesWorklist = Worklist<V8Reference, 16 /* local entries */>;
using NotSafeToConcurrentlyTraceWorklist =
    Worklist<NotSafeToConcurrentlyTraceItem, 64 /* local entries */>;

class PLATFORM_EXPORT HeapAllocHooks {
  STATIC_ONLY(HeapAllocHooks);

 public:
  // TODO(hajimehoshi): Pass a type name of the allocated object.
  typedef void AllocationHook(Address, size_t, const char*);
  typedef void FreeHook(Address);

  // Sets allocation hook. Only one hook is supported.
  static void SetAllocationHook(AllocationHook* hook) {
    CHECK(!allocation_hook_ || !hook);
    allocation_hook_ = hook;
  }

  // Sets free hook. Only one hook is supported.
  static void SetFreeHook(FreeHook* hook) {
    CHECK(!free_hook_ || !hook);
    free_hook_ = hook;
  }

  static void AllocationHookIfEnabled(Address address,
                                      size_t size,
                                      const char* type_name) {
    AllocationHook* allocation_hook = allocation_hook_;
    if (UNLIKELY(!!allocation_hook))
      allocation_hook(address, size, type_name);
  }

  static void FreeHookIfEnabled(Address address) {
    FreeHook* free_hook = free_hook_;
    if (UNLIKELY(!!free_hook))
      free_hook(address);
  }

 private:
  static AllocationHook* allocation_hook_;
  static FreeHook* free_hook_;
};

class HeapCompact;
template <typename T>
class Member;
template <typename T>
class WeakMember;
template <typename T>
class UntracedMember;

namespace internal {

class LivenessBrokerFactory;

template <typename T, bool = NeedsAdjustPointer<T>::value>
class ObjectAliveTrait;

template <typename T>
class ObjectAliveTrait<T, false> {
  STATIC_ONLY(ObjectAliveTrait);

 public:
  static bool IsHeapObjectAlive(const T* object) {
    static_assert(sizeof(T), "T must be fully defined");
    return HeapObjectHeader::FromPayload(object)->IsMarked();
  }
};

template <typename T>
class ObjectAliveTrait<T, true> {
  STATIC_ONLY(ObjectAliveTrait);

 public:
  NO_SANITIZE_ADDRESS
  static bool IsHeapObjectAlive(const T* object) {
    static_assert(sizeof(T), "T must be fully defined");
    const HeapObjectHeader* header = HeapObjectHeader::FromPayload(
        TraceTrait<T>::GetTraceDescriptor(object).base_object_payload);
    DCHECK(!header->IsInConstruction() || header->IsMarked());
    return header->IsMarked();
  }
};

template <typename T, typename = int>
struct IsGarbageCollectedContainer : std::false_type {};

template <typename T>
struct IsGarbageCollectedContainer<
    T,
    typename T::IsGarbageCollectedCollectionTypeMarker> : std::true_type {};

}  // namespace internal

class PLATFORM_EXPORT ThreadHeap {
  USING_FAST_MALLOC(ThreadHeap);

  using EphemeronProcessing = ThreadState::EphemeronProcessing;

 public:
  explicit ThreadHeap(ThreadState*);
  ~ThreadHeap();

  MarkingWorklist* GetMarkingWorklist() const {
    return marking_worklist_.get();
  }

  WriteBarrierWorklist* GetWriteBarrierWorklist() const {
    return write_barrier_worklist_.get();
  }

  NotFullyConstructedWorklist* GetNotFullyConstructedWorklist() const {
    return not_fully_constructed_worklist_.get();
  }

  NotFullyConstructedWorklist* GetPreviouslyNotFullyConstructedWorklist()
      const {
    return previously_not_fully_constructed_worklist_.get();
  }

  WeakCallbackWorklist* GetWeakCallbackWorklist() const {
    return weak_callback_worklist_.get();
  }

  MovableReferenceWorklist* GetMovableReferenceWorklist() const {
    return movable_reference_worklist_.get();
  }

  EphemeronPairsWorklist* GetDiscoveredEphemeronPairsWorklist() const {
    return discovered_ephemeron_pairs_worklist_.get();
  }

  EphemeronPairsWorklist* GetEphemeronPairsToProcessWorklist() const {
    return ephemeron_pairs_to_process_worklist_.get();
  }

  V8ReferencesWorklist* GetV8ReferencesWorklist() const {
    return v8_references_worklist_.get();
  }

  NotSafeToConcurrentlyTraceWorklist* GetNotSafeToConcurrentlyTraceWorklist()
      const {
    return not_safe_to_concurrently_trace_worklist_.get();
  }
  // Register an ephemeron table for fixed-point iteration.
  void RegisterWeakTable(void* container_object,
                         EphemeronCallback);

  // Heap compaction registration methods:

  // Checks whether we need to register |addr| as a backing store or a slot
  // containing reference to it.
  bool ShouldRegisterMovingAddress();

  RegionTree* GetRegionTree() { return region_tree_.get(); }

  static inline size_t AllocationSizeFromSize(size_t size) {
    // Add space for header.
    size_t allocation_size = size + sizeof(HeapObjectHeader);
    // The allocation size calculation can overflow for large sizes.
    CHECK_GT(allocation_size, size);
    // Align size with allocation granularity.
    allocation_size = (allocation_size + kAllocationMask) & ~kAllocationMask;
    return allocation_size;
  }
  Address AllocateOnArenaIndex(ThreadState*,
                               size_t,
                               int arena_index,
                               uint32_t gc_info_index,
                               const char* type_name);
  template <typename T>
  static Address Allocate(size_t);

  void WeakProcessing(MarkingVisitor*);

  // Moves not fully constructed objects to previously not fully constructed
  // objects. Such objects can be iterated using the Trace() method and do
  // not need to rely on conservative handling.
  void FlushNotFullyConstructedObjects();

  // Moves ephemeron pairs from |discovered_ephemeron_pairs_worklist_| to
  // |ephemeron_pairs_to_process_worklist_|
  void FlushEphemeronPairs(EphemeronProcessing);

  // Marks not fully constructed objects.
  void MarkNotFullyConstructedObjects(MarkingVisitor*);
  // Marks the transitive closure including ephemerons.
  bool AdvanceMarking(MarkingVisitor*, base::TimeTicks, EphemeronProcessing);
  void VerifyMarking();

  // Returns true if concurrent markers will have work to steal
  bool HasWorkForConcurrentMarking() const;
  // Returns the amount of work currently available for stealing (there could be
  // work remaining even if this is 0).
  size_t ConcurrentMarkingGlobalWorkSize() const;
  // Returns true if marker is done
  bool AdvanceConcurrentMarking(ConcurrentMarkingVisitor*,
                                base::JobDelegate*,
                                MarkingSchedulingOracle* marking_scheduler);

  // Conservatively checks whether an address is a pointer in any of the
  // thread heaps.  If so marks the object pointed to as live.
  Address CheckAndMarkPointer(MarkingVisitor*, Address);

  // Visits remembered sets.
  void VisitRememberedSets(MarkingVisitor*);

  size_t ObjectPayloadSizeForTesting();
  void ResetAllocationPointForTesting();

  PagePool* GetFreePagePool() { return free_page_pool_.get(); }

  // This look-up uses the region search tree and a negative contains cache to
  // provide an efficient mapping from arbitrary addresses to the containing
  // heap-page if one exists.
  BasePage* LookupPageForAddress(ConstAddress);

  HeapCompact* Compaction();

  // Get one of the heap structures for this thread.
  // The thread heap is split into multiple heap parts based on object types
  // and object sizes.
  BaseArena* Arena(int arena_index) const {
    DCHECK_LE(0, arena_index);
    DCHECK_LT(arena_index, BlinkGC::kNumberOfArenas);
    return arenas_[arena_index];
  }

  static bool IsVectorArenaIndex(int arena_index) {
    return BlinkGC::kVectorArenaIndex == arena_index;
  }
  static bool IsNormalArenaIndex(int);

  void MakeConsistentForGC();
  // MakeConsistentForMutator() drops marks from marked objects and rebuild
  // free lists. This is called after taking a snapshot and before resuming
  // the executions of mutators.
  void MakeConsistentForMutator();

  // Unmarks all objects in the entire heap. This is supposed to be called in
  // the beginning of major GC.
  void Unmark();

  void Compact();

  bool AdvanceLazySweep(base::TimeTicks deadline);
  bool AdvanceConcurrentSweep(base::JobDelegate*);

  void PrepareForSweep(BlinkGC::CollectionType);
  void RemoveAllPages();
  void InvokeFinalizersOnSweptPages();
  void CompleteSweep();

  void CollectStatistics(ThreadState::Statistics* statistics);

  ThreadHeapStatsCollector* stats_collector() const {
    return heap_stats_collector_.get();
  }

#if defined(ADDRESS_SANITIZER)
  void PoisonUnmarkedObjects();
#endif

#if DCHECK_IS_ON()
  // Infrastructure to determine if an address is within one of the
  // address ranges for the Blink heap. If the address is in the Blink
  // heap the containing heap page is returned.
  BasePage* FindPageFromAddress(Address);
  BasePage* FindPageFromAddress(const void* pointer) {
    return FindPageFromAddress(
        reinterpret_cast<Address>(const_cast<void*>(pointer)));
  }
#endif

  PageBloomFilter* page_bloom_filter() { return page_bloom_filter_.get(); }

  bool IsInLastAllocatedRegion(Address address) const;
  void SetLastAllocatedRegion(Address start, size_t length);

 private:
  struct LastAllocatedRegion {
    Address start = nullptr;
    size_t length = 0;
  };

  static int ArenaIndexForObjectSize(size_t);

  void SetupWorklists(bool);
  void DestroyMarkingWorklists(BlinkGC::StackState);
  void DestroyCompactionWorklists();

  bool InvokeEphemeronCallbacks(EphemeronProcessing,
                                MarkingVisitor*,
                                base::TimeTicks);

  bool FlushV8References(base::TimeTicks);

  ThreadState* thread_state_;
  std::unique_ptr<ThreadHeapStatsCollector> heap_stats_collector_;
  std::unique_ptr<RegionTree> region_tree_;
  std::unique_ptr<PageBloomFilter> page_bloom_filter_;
  std::unique_ptr<PagePool> free_page_pool_;
  std::unique_ptr<ProcessHeapReporter> process_heap_reporter_;

  // All objects on this worklist have been fully initialized and assigned a
  // trace callback for iterating the body of the object. This worklist should
  // contain almost all objects.
  std::unique_ptr<MarkingWorklist> marking_worklist_;

  // Objects on this worklist have been collected in the write barrier. The
  // worklist is different from |marking_worklist_| to minimize execution in the
  // path where a write barrier is executed.
  std::unique_ptr<WriteBarrierWorklist> write_barrier_worklist_;

  // Objects on this worklist were observed to be in construction (in their
  // constructor) and thus have been delayed for processing. They have not yet
  // been assigned a valid header and trace callback.
  std::unique_ptr<NotFullyConstructedWorklist> not_fully_constructed_worklist_;

  // Objects on this worklist were previously in construction but have been
  // moved here upon observing a safepoint, i.e., processing without stack. They
  // have not yet been assigned a valid header and trace callback but are fully
  // specified and can thus be iterated using the trace callback (which can be
  // looked up dynamically).
  std::unique_ptr<NotFullyConstructedWorklist>
      previously_not_fully_constructed_worklist_;

  // Worklist of weak callbacks accumulated for objects. Such callbacks are
  // processed after finishing marking objects.
  std::unique_ptr<WeakCallbackWorklist> weak_callback_worklist_;

  // The worklist is to remember slots that are traced during
  // marking phases. The mapping between the slots and the backing stores are
  // created at the atomic pause phase.
  std::unique_ptr<MovableReferenceWorklist> movable_reference_worklist_;

  // Worklist of ephemeron callbacks. Used to pass new callbacks from
  // MarkingVisitor to ThreadHeap.
  std::unique_ptr<EphemeronPairsWorklist> discovered_ephemeron_pairs_worklist_;
  std::unique_ptr<EphemeronPairsWorklist> ephemeron_pairs_to_process_worklist_;

  // Worklist for storing the V8 references until ThreadHeap can flush them
  // to V8.
  std::unique_ptr<V8ReferencesWorklist> v8_references_worklist_;

  std::unique_ptr<NotSafeToConcurrentlyTraceWorklist>
      not_safe_to_concurrently_trace_worklist_;

  std::unique_ptr<HeapCompact> compaction_;

  LastAllocatedRegion last_allocated_region_;

  BaseArena* arenas_[BlinkGC::kNumberOfArenas];

  static ThreadHeap* main_thread_heap_;

  static constexpr size_t kStepsBeforeEphemeronPairsFlush = 4u;
  size_t steps_since_last_ephemeron_pairs_flush_ = 0;
  static constexpr size_t kStepsBeforeEphemeronProcessing = 16u;
  size_t steps_since_last_ephemeron_processing_ = 0;

  friend class incremental_marking_test::IncrementalMarkingScopeBase;
  template <typename T>
  friend class Member;
  friend class ThreadState;
};

template <typename T>
class GarbageCollected {
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  using ParentMostGarbageCollectedType = T;

  // Must use MakeGarbageCollected.
  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
  // The garbage collector is taking care of reclaiming the object. Also,
  // virtual destructor requires an unambiguous, accessible 'operator delete'.
  void operator delete(void*) { NOTREACHED(); }
  void operator delete[](void*) = delete;

  template <typename Derived>
  static void* AllocateObject(size_t size) {
    return ThreadHeap::Allocate<GCInfoFoldedType<Derived>>(size);
  }

 protected:
  // This trait in theory can be moved to gc_info.h, but that would cause
  // significant memory bloat caused by huge number of ThreadHeap::Allocate<>
  // instantiations, which linker is not able to fold.
  template <typename Derived>
  class GCInfoFolded {
    static constexpr bool is_virtual_destructor_at_base =
        std::has_virtual_destructor<ParentMostGarbageCollectedType>::value;
    static constexpr bool both_trivially_destructible =
        std::is_trivially_destructible<ParentMostGarbageCollectedType>::value &&
        std::is_trivially_destructible<Derived>::value;
    static constexpr bool has_custom_dispatch_at_base =
        internal::HasFinalizeGarbageCollectedObject<
            ParentMostGarbageCollectedType>::value;

   public:
    using Type = std::conditional_t<is_virtual_destructor_at_base ||
                                        both_trivially_destructible ||
                                        has_custom_dispatch_at_base,
                                    ParentMostGarbageCollectedType,
                                    Derived>;
  };

  template <typename Derived>
  using GCInfoFoldedType = typename GCInfoFolded<Derived>::Type;

  GarbageCollected() = default;

  DISALLOW_COPY_AND_ASSIGN(GarbageCollected);
};

// Used for passing custom sizes to MakeGarbageCollected.
struct AdditionalBytes {
  explicit AdditionalBytes(size_t bytes) : value(bytes) {}
  const size_t value;
};

template <typename T>
struct MakeGarbageCollectedTrait {
  template <typename... Args>
  static T* Call(Args&&... args) {
    static_assert(WTF::IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    static_assert(
        std::is_trivially_destructible<T>::value ||
            std::has_virtual_destructor<T>::value || std::is_final<T>::value ||
            internal::IsGarbageCollectedContainer<T>::value ||
            internal::HasFinalizeGarbageCollectedObject<T>::value,
        "Finalized GarbageCollected class should either have a virtual "
        "destructor or be marked as final");
    static_assert(!IsGarbageCollectedMixin<T>::value ||
                      sizeof(T) <= kLargeObjectSizeThreshold,
                  "GarbageCollectedMixin may not be a large object");
    void* memory = T::template AllocateObject<T>(sizeof(T));
    HeapObjectHeader* header = HeapObjectHeader::FromPayload(memory);
    // Placement new as regular operator new() is deleted.
    T* object = ::new (memory) T(std::forward<Args>(args)...);
    header->MarkFullyConstructed<HeapObjectHeader::AccessMode::kAtomic>();
    return object;
  }

  template <typename... Args>
  static T* Call(AdditionalBytes additional_bytes, Args&&... args) {
    static_assert(WTF::IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    static_assert(
        std::is_trivially_destructible<T>::value ||
            std::has_virtual_destructor<T>::value || std::is_final<T>::value ||
            internal::IsGarbageCollectedContainer<T>::value ||
            internal::HasFinalizeGarbageCollectedObject<T>::value,
        "Finalized GarbageCollected class should either have a virtual "
        "destructor or be marked as final.");
    const size_t size = sizeof(T) + additional_bytes.value;
    if (IsGarbageCollectedMixin<T>::value) {
      // Ban large mixin so we can use PageFromObject() on them.
      CHECK_GE(kLargeObjectSizeThreshold, size)
          << "GarbageCollectedMixin may not be a large object";
    }
    void* memory = T::template AllocateObject<T>(size);
    HeapObjectHeader* header = HeapObjectHeader::FromPayload(memory);
    // Placement new as regular operator new() is deleted.
    T* object = ::new (memory) T(std::forward<Args>(args)...);
    header->MarkFullyConstructed<HeapObjectHeader::AccessMode::kAtomic>();
    return object;
  }
};

template <typename T, typename = void>
struct PostConstructionHookTrait {
  static void Call(T*) {}
};

// Default MakeGarbageCollected: Constructs an instance of T, which is a garbage
// collected type.
template <typename T, typename... Args>
T* MakeGarbageCollected(Args&&... args) {
  T* object = MakeGarbageCollectedTrait<T>::Call(std::forward<Args>(args)...);
  PostConstructionHookTrait<T>::Call(object);
  return object;
}

// Constructs an instance of T, which is a garbage collected type. This special
// version takes size which enables constructing inline objects.
template <typename T, typename... Args>
T* MakeGarbageCollected(AdditionalBytes additional_bytes, Args&&... args) {
  T* object = MakeGarbageCollectedTrait<T>::Call(additional_bytes,
                                                 std::forward<Args>(args)...);
  PostConstructionHookTrait<T>::Call(object);
  return object;
}

// Assigning class types to their arenas.
//
// We use sized arenas for most 'normal' objects to improve memory locality.
// It seems that the same type of objects are likely to be accessed together,
// which means that we want to group objects by type. That's one reason
// why we provide dedicated arenas for popular types (e.g., Node, CSSValue),
// but it's not practical to prepare dedicated arenas for all types.
// Thus we group objects by their sizes, hoping that this will approximately
// group objects by their types.
//

inline int ThreadHeap::ArenaIndexForObjectSize(size_t size) {
  if (size < 64) {
    if (size < 32)
      return BlinkGC::kNormalPage1ArenaIndex;
    return BlinkGC::kNormalPage2ArenaIndex;
  }
  if (size < 128)
    return BlinkGC::kNormalPage3ArenaIndex;
  return BlinkGC::kNormalPage4ArenaIndex;
}

inline bool ThreadHeap::IsNormalArenaIndex(int index) {
  return index >= BlinkGC::kNormalPage1ArenaIndex &&
         index <= BlinkGC::kNormalPage4ArenaIndex;
}

inline Address ThreadHeap::AllocateOnArenaIndex(ThreadState* state,
                                                size_t size,
                                                int arena_index,
                                                uint32_t gc_info_index,
                                                const char* type_name) {
  DCHECK(state->IsAllocationAllowed());
  DCHECK_NE(arena_index, BlinkGC::kLargeObjectArenaIndex);
  NormalPageArena* arena = static_cast<NormalPageArena*>(Arena(arena_index));
  Address address =
      arena->AllocateObject(AllocationSizeFromSize(size), gc_info_index);
  HeapAllocHooks::AllocationHookIfEnabled(address, size, type_name);
  return address;
}

template <typename T>
Address ThreadHeap::Allocate(size_t size) {
  ThreadState* state = ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
  const char* type_name = WTF_HEAP_PROFILER_TYPE_NAME(T);
  return state->Heap().AllocateOnArenaIndex(
      state, size, ThreadHeap::ArenaIndexForObjectSize(size),
      GCInfoTrait<T>::Index(), type_name);
}

inline bool ThreadHeap::IsInLastAllocatedRegion(Address address) const {
  return last_allocated_region_.start <= address &&
         address <
             (last_allocated_region_.start + last_allocated_region_.length);
}

inline void ThreadHeap::SetLastAllocatedRegion(Address start, size_t length) {
  last_allocated_region_.start = start;
  last_allocated_region_.length = length;
}

class PLATFORM_EXPORT LivenessBroker final {
 public:
  template <typename T>
  bool IsHeapObjectAlive(const T*) const;
  template <typename T>
  bool IsHeapObjectAlive(const WeakMember<T>&) const;
  template <typename T>
  bool IsHeapObjectAlive(const UntracedMember<T>&) const;

 private:
  LivenessBroker() = default;
  friend class internal::LivenessBrokerFactory;
};

template <typename T>
bool LivenessBroker::IsHeapObjectAlive(const T* object) const {
  static_assert(sizeof(T), "T must be fully defined");
  // The strongification of collections relies on the fact that once a
  // collection has been strongified, there is no way that it can contain
  // non-live entries, so no entries will be removed. Since you can't set
  // the mark bit on a null pointer, that means that null pointers are
  // always 'alive'.
  if (!object)
    return true;
  // TODO(keishi): some tests create CrossThreadPersistent on non attached
  // threads.
  if (!ThreadState::Current())
    return true;
  DCHECK(&ThreadState::Current()->Heap() ==
         &PageFromObject(object)->Arena()->GetThreadState()->Heap());
  return internal::ObjectAliveTrait<T>::IsHeapObjectAlive(object);
}

template <typename T>
bool LivenessBroker::IsHeapObjectAlive(const WeakMember<T>& weak_member) const {
  return IsHeapObjectAlive(weak_member.Get());
}

template <typename T>
bool LivenessBroker::IsHeapObjectAlive(
    const UntracedMember<T>& untraced_member) const {
  return IsHeapObjectAlive(untraced_member.Get());
}

template <typename T>
void Visitor::HandleWeakCell(const LivenessBroker& broker, const void* object) {
  WeakMember<T>* weak_member =
      reinterpret_cast<WeakMember<T>*>(const_cast<void*>(object));
  if (weak_member->Get()) {
    if (weak_member->IsHashTableDeletedValue()) {
      // This can happen when weak fields are deleted while incremental marking
      // is running. Deleted values need to be preserved to avoid reviving
      // objects in containers.
      return;
    }
    if (!broker.IsHeapObjectAlive(weak_member->Get()))
      weak_member->Clear();
  }
}

namespace internal {

class LivenessBrokerFactory final {
 public:
  static LivenessBroker Create() { return LivenessBroker(); }
};

}  // namespace internal

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_H_
