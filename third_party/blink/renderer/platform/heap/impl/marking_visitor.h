// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_MARKING_VISITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_MARKING_VISITOR_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/impl/heap_page.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace internal {

ALWAYS_INLINE bool IsHashTableDeleteValue(const void* value) {
  return value == reinterpret_cast<void*>(-1);
}

}  // namespace internal

class BasePage;
enum class TracenessMemberConfiguration;
template <typename T, TracenessMemberConfiguration tracenessConfiguration>
class MemberBase;

// Base visitor used to mark Oilpan objects on any thread.
class PLATFORM_EXPORT MarkingVisitorBase : public Visitor {
 public:
  enum MarkingMode {
    // Default visitor mode used for regular marking.
    kGlobalMarking,
    // Visitor mode recording slots for compaction during marking.
    kGlobalMarkingWithCompaction,
  };

  void VisitWeakContainer(const void*,
                          const void* const*,
                          TraceDescriptor,
                          TraceDescriptor,
                          WeakCallback,
                          const void*) final;
  void VisitEphemeron(const void*, TraceDescriptor) final;

  // Marks an object dynamically using any address within its body and adds a
  // tracing callback for processing of the object. The object is not allowed
  // to be in construction.
  void DynamicallyMarkAddress(ConstAddress);

  void RegisterMovableSlot(const void* const*) final;

  void RegisterWeakCallback(WeakCallback, const void*) final;

  // Flush private segments remaining in visitor's worklists to global pools.
  void FlushCompactionWorklists();

  size_t marked_bytes() const { return marked_bytes_; }

  int task_id() const { return task_id_; }

  // Account for object's live bytes. Should only be adjusted when
  // actually tracing through an already marked object. Logically, this means
  // accounting for the bytes when transitioning from grey to black.
  ALWAYS_INLINE void AccountMarkedBytes(HeapObjectHeader*);
  ALWAYS_INLINE void AccountMarkedBytes(size_t);

 protected:
  MarkingVisitorBase(ThreadState*, MarkingMode, int task_id);
  ~MarkingVisitorBase() override = default;

  void Visit(const void* object, TraceDescriptor desc) final;
  void VisitWeak(const void*, const void*, TraceDescriptor, WeakCallback) final;

  // Marks an object and adds a tracing callback for processing of the object.
  void MarkHeader(HeapObjectHeader*, const TraceDescriptor&);
  // Try to mark an object without tracing. Returns true when the object was not
  // marked upon calling.
  bool MarkHeaderNoTracing(HeapObjectHeader*);

  MarkingWorklist::View marking_worklist_;
  WriteBarrierWorklist::View write_barrier_worklist_;
  NotFullyConstructedWorklist::View not_fully_constructed_worklist_;
  WeakCallbackWorklist::View weak_callback_worklist_;
  MovableReferenceWorklist::View movable_reference_worklist_;
  EphemeronPairsWorklist::View discovered_ephemeron_pairs_worklist_;
  EphemeronPairsWorklist::View ephemeron_pairs_to_process_worklist_;
  WeakContainersWorklist* const weak_containers_worklist_;
  size_t marked_bytes_ = 0;
  const MarkingMode marking_mode_;
  int task_id_;
};

ALWAYS_INLINE void MarkingVisitorBase::AccountMarkedBytes(
    HeapObjectHeader* header) {
  AccountMarkedBytes(
      header->IsLargeObject<HeapObjectHeader::AccessMode::kAtomic>()
          ? static_cast<LargeObjectPage*>(PageFromObject(header))->ObjectSize()
          : header->size<HeapObjectHeader::AccessMode::kAtomic>());
}

ALWAYS_INLINE void MarkingVisitorBase::AccountMarkedBytes(size_t marked_bytes) {
  marked_bytes_ += marked_bytes;
}

ALWAYS_INLINE bool MarkingVisitorBase::MarkHeaderNoTracing(
    HeapObjectHeader* header) {
  DCHECK(header);
  DCHECK(State()->IsIncrementalMarking() || State()->InAtomicMarkingPause());
  // A GC should only mark the objects that belong in its heap.
  DCHECK_EQ(State(),
            PageFromObject(header->Payload())->Arena()->GetThreadState());
  // Never mark free space objects. This would e.g. hint to marking a promptly
  // freed backing store.
  DCHECK(!header->IsFree());

  return header->TryMark<HeapObjectHeader::AccessMode::kAtomic>();
}

inline void MarkingVisitorBase::Visit(const void* object,
                                      TraceDescriptor desc) {
  DCHECK(object);
  MarkHeader(HeapObjectHeader::FromPayload(desc.base_object_payload), desc);
}

// Marks an object and adds a tracing callback for processing of the object.
ALWAYS_INLINE void MarkingVisitorBase::MarkHeader(HeapObjectHeader* header,
                                                  const TraceDescriptor& desc) {
  DCHECK(header);
  DCHECK(desc.callback);

  if (header->IsInConstruction<HeapObjectHeader::AccessMode::kAtomic>()) {
    not_fully_constructed_worklist_.Push(header->Payload());
  } else if (MarkHeaderNoTracing(header)) {
    marking_worklist_.Push(desc);
  }
}

// Visitor used to mark Oilpan objects on the main thread. Also implements
// various sorts of write barriers that should only be called from the main
// thread.
class PLATFORM_EXPORT MarkingVisitor : public MarkingVisitorBase {
 public:
  // Write barrier that adds a value the |slot| refers to to the set of marked
  // objects. The barrier bails out if marking is off or the object is not yet
  // marked. Returns true if the value has been marked on this call.
  ALWAYS_INLINE static bool WriteBarrier(void** slot);

  using ThreadStateCallback = ThreadState*();
  // Write barrier where for a range of |number_of_elements| elements of size
  // |element_size| starting at |first_element|. The |callback| will be invoked
  // for each element if necessary.
  ALWAYS_INLINE static void WriteBarrier(
      ThreadStateCallback thread_state_callback,
      void* first_element,
      size_t element_size,
      size_t number_of_elements,
      TraceCallback callback);

  // Eagerly traces an already marked |object| ensuring that all its children
  // are discovered by the marker. The barrier bails out if marking is off and
  // on individual objects reachable if they are already marked. The barrier
  // uses the callback function through GcInfo.
  //
  // Note: |object| must point to the beginning of the heap object.
  ALWAYS_INLINE static void RetraceObject(const void* object);

  MarkingVisitor(ThreadState*, MarkingMode);
  ~MarkingVisitor() override = default;

  // Conservatively marks an object if pointed to by Address. The object may
  // be in construction as the scan is conservative without relying on a
  // Trace method.
  void ConservativelyMarkAddress(BasePage*, ConstAddress);

  void FlushMarkingWorklists();

 private:
  ALWAYS_INLINE static void GenerationalBarrier(Address slot,
                                                ThreadState* state);

  // Exact version of the marking and generational write barriers.
  static bool WriteBarrierSlow(void*);
  static void GenerationalBarrierSlow(Address, ThreadState*);
  static bool MarkValue(void*, BasePage*, ThreadState*);
  static void RetraceObjectSlow(const void*);

  // Weak containers are strongly retraced during conservative stack scanning.
  // Stack scanning happens once per GC at the start of the atomic pause.
  // Because the visitor is not retained between GCs, there is no need to clear
  // the set at the end of GC.
  class RecentlyRetracedWeakContainers {
    static constexpr size_t kMaxCacheSize = 8;

   public:
    bool Contains(const HeapObjectHeader*);
    void Insert(const HeapObjectHeader*);

   private:
    std::vector<const HeapObjectHeader*> recently_retraced_cache_;
    size_t last_used_index_ = -1;
  } recently_retraced_weak_containers_;

  template <typename T, TracenessMemberConfiguration tracenessConfiguration>
  friend class MemberBase;
};

// static
bool MarkingVisitor::WriteBarrier(void** slot) {
#if BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
  void* value = *slot;
  if (!value || internal::IsHashTableDeleteValue(value))
    return false;

  // Dijkstra barrier if concurrent marking is in progress.
  BasePage* value_page = PageFromObject(value);
  ThreadState* thread_state = value_page->thread_state();

  if (UNLIKELY(thread_state->IsIncrementalMarking()))
    return MarkValue(value, value_page, thread_state);

  GenerationalBarrier(reinterpret_cast<Address>(slot), thread_state);
  return false;
#else
  if (!ThreadState::IsAnyIncrementalMarking())
    return false;

  // Avoid any further checks and dispatch to a call at this point. Aggressive
  // inlining otherwise pollutes the regular execution paths.
  return WriteBarrierSlow(*slot);
#endif
}

// static
void MarkingVisitor::WriteBarrier(ThreadStateCallback thread_state_callback,
                                  void* first_element,
                                  size_t element_size,
                                  size_t number_of_elements,
                                  TraceCallback callback) {
#if BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
  ThreadState* const thread_state = thread_state_callback();
  if (!thread_state->IsIncrementalMarking()) {
    MarkingVisitor::GenerationalBarrier(
        reinterpret_cast<Address>(first_element), thread_state);
    return;
  }
#else   // !BLINK_HEAP_YOUNG_GENERATION
  if (!ThreadState::IsAnyIncrementalMarking())
    return;
  // The object may have been in-place constructed as part of a large object.
  // It is not safe to retrieve the page from the object here.
  ThreadState* const thread_state = thread_state_callback();
  if (!thread_state->IsIncrementalMarking()) {
    return;
  }
#endif  // !BLINK_HEAP_YOUNG_GENERATION
  ThreadState::NoAllocationScope no_allocation_scope(thread_state);
  DCHECK(thread_state->CurrentVisitor());
  // No weak handling for write barriers. Modifying weakly reachable objects
  // strongifies them for the current cycle.
  char* array = static_cast<char*>(first_element);
  while (number_of_elements-- > 0) {
    callback(thread_state->CurrentVisitor(), array);
    array += element_size;
  }
}

// static
void MarkingVisitor::GenerationalBarrier(Address slot, ThreadState* state) {
  // First, check if the source object is in the last allocated region of heap.
  if (LIKELY(state->Heap().IsInLastAllocatedRegion(slot)))
    return;
  if (UNLIKELY(state->IsOnStack(slot)))
    return;
  GenerationalBarrierSlow(slot, state);
}

// static
void MarkingVisitor::RetraceObject(const void* object) {
  if (!ThreadState::IsAnyIncrementalMarking())
    return;

  RetraceObjectSlow(object);
}

// Visitor used to mark Oilpan objects on concurrent threads.
class PLATFORM_EXPORT ConcurrentMarkingVisitor : public MarkingVisitorBase {
 public:
  ConcurrentMarkingVisitor(ThreadState*, MarkingMode, int);
  ~ConcurrentMarkingVisitor() override;

  virtual void FlushWorklists();

  bool IsConcurrent() const override { return true; }

  bool DeferredTraceIfConcurrent(TraceDescriptor desc,
                                 size_t bailout_size) override {
    not_safe_to_concurrently_trace_worklist_.Push({desc, bailout_size});
    // The object is bailed out from concurrent marking, so updating
    // marked_bytes_ to reflect how many bytes were actually traced.
    // This deducted bytes will be added to the mutator thread marking
    // visitor's marked_bytes_ count when the object is popped from
    // the bailout worklist.
    marked_bytes_ -= bailout_size;
    return true;
  }

  size_t RecentlyMarkedBytes() {
    return marked_bytes_ - std::exchange(last_marked_bytes_, marked_bytes_);
  }

 private:
  NotSafeToConcurrentlyTraceWorklist::View
      not_safe_to_concurrently_trace_worklist_;
  NotFullyConstructedWorklist::View previously_not_fully_constructed_worklist_;
  size_t last_marked_bytes_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_MARKING_VISITOR_H_
