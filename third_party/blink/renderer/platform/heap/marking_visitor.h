// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VISITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VISITOR_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class BasePage;

// Base visitor used to mark Oilpan objects on any thread.
class PLATFORM_EXPORT MarkingVisitorCommon : public Visitor {
 public:
  enum MarkingMode {
    // Default visitor mode used for regular marking.
    kGlobalMarking,
    // Visitor mode recording slots for compaction during marking.
    kGlobalMarkingWithCompaction,
  };

  void VisitWeak(void*, void*, TraceDescriptor, WeakCallback) final;
  void VisitBackingStoreStrongly(void*, void**, TraceDescriptor) final;
  void VisitBackingStoreWeakly(void*,
                               void**,
                               TraceDescriptor,
                               TraceDescriptor,
                               WeakCallback,
                               void*) final;
  bool VisitEphemeronKeyValuePair(void*,
                                  void*,
                                  EphemeronTracingCallback,
                                  EphemeronTracingCallback) final;

  // Used to only mark the backing store when it has been registered for weak
  // processing. In this case, the contents are processed separately using
  // the corresponding traits but the backing store requires marking.
  void VisitBackingStoreOnly(void*, void**) final;

  // This callback mechanism is needed to account for backing store objects
  // containing intra-object pointers, all of which must be relocated/rebased
  // with respect to the moved-to location.
  //
  // For Blink, |HeapLinkedHashSet<>| is currently the only abstraction which
  // relies on this feature.
  void RegisterBackingStoreCallback(void*, MovingObjectCallback) final;
  void RegisterWeakCallback(WeakCallback, void*) final;

  // Flush private segments remaining in visitor's worklists to global pools.
  void FlushCompactionWorklists();

  size_t marked_bytes() const { return marked_bytes_; }

  int task_id() const { return task_id_; }

  // Account for object's live bytes. Should only be adjusted when
  // actually tracing through an already marked object. Logically, this means
  // accounting for the bytes when transitioning from grey to black.
  ALWAYS_INLINE void AccountMarkedBytes(HeapObjectHeader*);

 protected:
  MarkingVisitorCommon(ThreadState*, MarkingMode, int task_id);
  ~MarkingVisitorCommon() override = default;

  // Try to mark an object without tracing. Returns true when the object was not
  // marked upon calling.
  bool MarkHeaderNoTracing(HeapObjectHeader*);

  void RegisterBackingStoreReference(void** slot);

  MarkingWorklist::View marking_worklist_;
  NotFullyConstructedWorklist::View not_fully_constructed_worklist_;
  WeakCallbackWorklist::View weak_callback_worklist_;
  MovableReferenceWorklist::View movable_reference_worklist_;
  WeakTableWorklist::View weak_table_worklist_;
  BackingStoreCallbackWorklist::View backing_store_callback_worklist_;
  size_t marked_bytes_ = 0;
  const MarkingMode marking_mode_;
  int task_id_;
};

ALWAYS_INLINE void MarkingVisitorCommon::AccountMarkedBytes(
    HeapObjectHeader* header) {
  marked_bytes_ +=
      header->IsLargeObject()
          ? reinterpret_cast<LargeObjectPage*>(PageFromObject(header))->size()
          : header->size();
}

ALWAYS_INLINE bool MarkingVisitorCommon::MarkHeaderNoTracing(
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

// Base visitor used to mark Oilpan objects on any thread.
template <class Specialized>
class PLATFORM_EXPORT MarkingVisitorBase : public MarkingVisitorCommon {
 public:
  void Visit(void* object, TraceDescriptor desc) final;

  // Unused cross-component visit methods.
  void Visit(const TraceWrapperV8Reference<v8::Value>&) override {}

 protected:
  MarkingVisitorBase(ThreadState* state, MarkingMode marking_mode, int task_id)
      : MarkingVisitorCommon(state, marking_mode, task_id) {}
  ~MarkingVisitorBase() override = default;

  // Marks an object and adds a tracing callback for processing of the object.
  void MarkHeader(HeapObjectHeader*, const TraceDescriptor&);
};

template <class Specialized>
inline void MarkingVisitorBase<Specialized>::Visit(void* object,
                                                   TraceDescriptor desc) {
  DCHECK(object);
  if (desc.base_object_payload == BlinkGC::kNotFullyConstructedObject) {
    // This means that the objects are not-yet-fully-constructed. See comments
    // on GarbageCollectedMixin for how those objects are handled.
    not_fully_constructed_worklist_.Push(object);
    return;
  }
  MarkHeader(HeapObjectHeader::FromPayload(desc.base_object_payload), desc);
}

// Marks an object and adds a tracing callback for processing of the object.
template <class Specialized>
ALWAYS_INLINE void MarkingVisitorBase<Specialized>::MarkHeader(
    HeapObjectHeader* header,
    const TraceDescriptor& desc) {
  DCHECK(header);
  DCHECK(desc.callback);

  if (Specialized::IsInConstruction(header)) {
    not_fully_constructed_worklist_.Push(header->Payload());
  } else if (MarkHeaderNoTracing(header)) {
    marking_worklist_.Push(desc);
  }
}

// Visitor used to mark Oilpan objects on the main thread. Also implements
// various sorts of write barriers that should only be called from the main
// thread.
class PLATFORM_EXPORT MarkingVisitor
    : public MarkingVisitorBase<MarkingVisitor> {
 public:
  // Returns whether an object is in construction.
  static bool IsInConstruction(HeapObjectHeader* header);

  // Write barrier that adds |value| to the set of marked objects. The barrier
  // bails out if marking is off or the object is not yet marked. Returns true
  // if the object was marked on this call.
  static bool WriteBarrier(void* value);

  // Eagerly traces an already marked backing store ensuring that all its
  // children are discovered by the marker. The barrier bails out if marking
  // is off and on individual objects reachable if they are already marked. The
  // barrier uses the callback function through GcInfo, so it will not inline
  // any templated type-specific code.
  static void TraceMarkedBackingStore(void* value);

  MarkingVisitor(ThreadState*, MarkingMode);
  ~MarkingVisitor() override = default;

  // Conservatively marks an object if pointed to by Address. The object may
  // be in construction as the scan is conservative without relying on a
  // Trace method.
  void ConservativelyMarkAddress(BasePage*, Address);

  // Marks an object dynamically using any address within its body and adds a
  // tracing callback for processing of the object. The object is not allowed
  // to be in construction.
  void DynamicallyMarkAddress(Address);

  void FlushMarkingWorklist();

 private:
  // Exact version of the marking write barriers.
  static bool WriteBarrierSlow(void*);
  static void TraceMarkedBackingStoreSlow(void*);

  WriteBarrierWorklist::View write_barrier_worklist_;
};

// static
ALWAYS_INLINE bool MarkingVisitor::IsInConstruction(HeapObjectHeader* header) {
  // No need for atomics when operating on the mutator thread where
  // construction happens.
  return header->IsInConstruction<HeapObjectHeader::AccessMode::kNonAtomic>();
}

// static
ALWAYS_INLINE bool MarkingVisitor::WriteBarrier(void* value) {
  if (!ThreadState::IsAnyIncrementalMarking())
    return false;

  // Avoid any further checks and dispatch to a call at this point. Aggressive
  // inlining otherwise pollutes the regular execution paths.
  return WriteBarrierSlow(value);
}

// static
ALWAYS_INLINE void MarkingVisitor::TraceMarkedBackingStore(void* value) {
  if (!ThreadState::IsAnyIncrementalMarking())
    return;

  // Avoid any further checks and dispatch to a call at this point. Aggressive
  // inlining otherwise pollutes the regular execution paths.
  TraceMarkedBackingStoreSlow(value);
}

// Visitor used to mark Oilpan objects on concurrent threads.
class PLATFORM_EXPORT ConcurrentMarkingVisitor
    : public MarkingVisitorBase<ConcurrentMarkingVisitor> {
 public:
  // Returns whether an object is in construction.
  static bool IsInConstruction(HeapObjectHeader* header);

  ConcurrentMarkingVisitor(ThreadState*, MarkingMode, int);
  ~ConcurrentMarkingVisitor() override = default;

  virtual void FlushWorklists();
};

// static
ALWAYS_INLINE bool ConcurrentMarkingVisitor::IsInConstruction(
    HeapObjectHeader* header) {
  return header->IsInConstruction<HeapObjectHeader::AccessMode::kAtomic>();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VISITOR_H_
