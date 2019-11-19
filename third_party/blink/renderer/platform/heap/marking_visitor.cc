// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/marking_visitor.h"

#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

namespace {

ALWAYS_INLINE bool IsHashTableDeleteValue(const void* value) {
  return value == reinterpret_cast<void*>(-1);
}

}  // namespace

MarkingVisitorCommon::MarkingVisitorCommon(ThreadState* state,
                                           MarkingMode marking_mode,
                                           int task_id)
    : Visitor(state),
      marking_worklist_(Heap().GetMarkingWorklist(), task_id),
      not_fully_constructed_worklist_(Heap().GetNotFullyConstructedWorklist(),
                                      task_id),
      weak_callback_worklist_(Heap().GetWeakCallbackWorklist(), task_id),
      movable_reference_worklist_(Heap().GetMovableReferenceWorklist(),
                                  task_id),
      weak_table_worklist_(Heap().GetWeakTableWorklist(), task_id),
      backing_store_callback_worklist_(Heap().GetBackingStoreCallbackWorklist(),
                                       task_id),
      marking_mode_(marking_mode),
      task_id_(task_id) {}

void MarkingVisitorCommon::FlushCompactionWorklists() {
  movable_reference_worklist_.FlushToGlobal();
  backing_store_callback_worklist_.FlushToGlobal();
}

void MarkingVisitorCommon::RegisterWeakCallback(WeakCallback callback,
                                                void* object) {
  weak_callback_worklist_.Push({callback, object});
}

void MarkingVisitorCommon::RegisterBackingStoreReference(void** slot) {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  MovableReference* movable_reference =
      reinterpret_cast<MovableReference*>(slot);
  if (Heap().ShouldRegisterMovingAddress(
          reinterpret_cast<Address>(movable_reference))) {
    movable_reference_worklist_.Push(movable_reference);
  }
}

void MarkingVisitorCommon::RegisterBackingStoreCallback(
    void* backing,
    MovingObjectCallback callback) {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  if (Heap().ShouldRegisterMovingAddress(reinterpret_cast<Address>(backing))) {
    backing_store_callback_worklist_.Push({backing, callback});
  }
}

void MarkingVisitorCommon::VisitWeak(void* object,
                                     void* object_weak_ref,
                                     TraceDescriptor desc,
                                     WeakCallback callback) {
  // Filter out already marked values. The write barrier for WeakMember
  // ensures that any newly set value after this point is kept alive and does
  // not require the callback.
  if (desc.base_object_payload != BlinkGC::kNotFullyConstructedObject &&
      HeapObjectHeader::FromPayload(desc.base_object_payload)
          ->IsMarked<HeapObjectHeader::AccessMode::kAtomic>())
    return;
  RegisterWeakCallback(callback, object_weak_ref);
}

void MarkingVisitorCommon::VisitBackingStoreStrongly(void* object,
                                                     void** object_slot,
                                                     TraceDescriptor desc) {
  RegisterBackingStoreReference(object_slot);
  if (!object)
    return;
  Visit(object, desc);
}

// All work is registered through RegisterWeakCallback.
void MarkingVisitorCommon::VisitBackingStoreWeakly(
    void* object,
    void** object_slot,
    TraceDescriptor strong_desc,
    TraceDescriptor weak_desc,
    WeakCallback weak_callback,
    void* weak_callback_parameter) {
  RegisterBackingStoreReference(object_slot);
  if (!object)
    return;
  RegisterWeakCallback(weak_callback, weak_callback_parameter);

  if (weak_desc.callback)
    weak_table_worklist_.Push(weak_desc);
}

bool MarkingVisitorCommon::VisitEphemeronKeyValuePair(
    void* key,
    void* value,
    EphemeronTracingCallback key_trace_callback,
    EphemeronTracingCallback value_trace_callback) {
  const bool key_is_dead = key_trace_callback(this, key);
  if (key_is_dead)
    return true;
  const bool value_is_dead = value_trace_callback(this, value);
  DCHECK(!value_is_dead);
  return false;
}

void MarkingVisitorCommon::VisitBackingStoreOnly(void* object,
                                                 void** object_slot) {
  RegisterBackingStoreReference(object_slot);
  if (!object)
    return;
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(object);
  MarkHeaderNoTracing(header);
  AccountMarkedBytes(header);
}

// static
bool MarkingVisitor::WriteBarrierSlow(void* value) {
  if (!value || IsHashTableDeleteValue(value))
    return false;

  // It is guaranteed that managed references point to either GarbageCollected
  // or GarbageCollectedMixin. Mixins are restricted to regular objects sizes.
  // It is thus possible to get to the page header by aligning properly.
  BasePage* base_page = PageFromObject(value);

  ThreadState* const thread_state = base_page->thread_state();
  if (!thread_state->IsIncrementalMarking())
    return false;

  HeapObjectHeader* header;
  if (LIKELY(!base_page->IsLargeObjectPage())) {
    header = reinterpret_cast<HeapObjectHeader*>(
        static_cast<NormalPage*>(base_page)
            ->FindHeaderFromAddress<HeapObjectHeader::AccessMode::kAtomic>(
                reinterpret_cast<Address>(value)));
  } else {
    LargeObjectPage* large_page = static_cast<LargeObjectPage*>(base_page);
    header = large_page->ObjectHeader();
  }

  if (!header->TryMark<HeapObjectHeader::AccessMode::kAtomic>())
    return false;

  MarkingVisitor* visitor = thread_state->CurrentVisitor();
  if (UNLIKELY(IsInConstruction(header))) {
    // It is assumed that objects on not_fully_constructed_worklist_ are not
    // marked.
    header->Unmark();
    visitor->not_fully_constructed_worklist_.Push(header->Payload());
    return true;
  }

  visitor->write_barrier_worklist_.Push(header);
  return true;
}

void MarkingVisitor::TraceMarkedBackingStoreSlow(void* value) {
  if (!value)
    return;

  ThreadState* const thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  // |value| is pointing to the start of a backing store.
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(value);
  CHECK(header->IsMarked());
  DCHECK(thread_state->CurrentVisitor());
  // No weak handling for write barriers. Modifying weakly reachable objects
  // strongifies them for the current cycle.
  GCInfoTable::Get()
      .GCInfoFromIndex(header->GcInfoIndex())
      ->trace(thread_state->CurrentVisitor(), value);
}

MarkingVisitor::MarkingVisitor(ThreadState* state, MarkingMode marking_mode)
    : MarkingVisitorBase(state, marking_mode, WorklistTaskId::MutatorThread),
      write_barrier_worklist_(Heap().GetWriteBarrierWorklist(),
                              WorklistTaskId::MutatorThread) {
  DCHECK(state->InAtomicMarkingPause());
  DCHECK(state->CheckThread());
}

void MarkingVisitor::DynamicallyMarkAddress(Address address) {
  HeapObjectHeader* const header = HeapObjectHeader::FromInnerAddress(address);
  DCHECK(header);
  DCHECK(!IsInConstruction(header));
  const GCInfo* gc_info =
      GCInfoTable::Get().GCInfoFromIndex(header->GcInfoIndex());
  if (MarkHeaderNoTracing(header)) {
    marking_worklist_.Push(
        {reinterpret_cast<void*>(header->Payload()), gc_info->trace});
  }
}

void MarkingVisitor::ConservativelyMarkAddress(BasePage* page,
                                               Address address) {
#if DCHECK_IS_ON()
  DCHECK(page->Contains(address));
#endif
  HeapObjectHeader* const header =
      page->IsLargeObjectPage()
          ? static_cast<LargeObjectPage*>(page)->ObjectHeader()
          : static_cast<NormalPage*>(page)->ConservativelyFindHeaderFromAddress(
                address);
  if (!header || header->IsMarked())
    return;

  // Simple case for fully constructed objects. This just adds the object to the
  // regular marking worklist.
  const GCInfo* gc_info =
      GCInfoTable::Get().GCInfoFromIndex(header->GcInfoIndex());
  if (!IsInConstruction(header)) {
    MarkHeader(header, {header->Payload(), gc_info->trace});
    return;
  }

  // This case is reached for not-fully-constructed objects with vtables.
  // We can differentiate multiple cases:
  // 1. No vtable set up. Example:
  //      class A : public GarbageCollected<A> { virtual void f() = 0; };
  //      class B : public A { B() : A(foo()) {}; };
  //    The vtable for A is not set up if foo() allocates and triggers a GC.
  //
  // 2. Vtables properly set up (non-mixin case).
  // 3. Vtables not properly set up (mixin) if GC is allowed during mixin
  //    construction.
  //
  // We use a simple conservative approach for these cases as they are not
  // performance critical.
  MarkHeaderNoTracing(header);
  Address* payload = reinterpret_cast<Address*>(header->Payload());
  const size_t payload_size = header->PayloadSize();
  for (size_t i = 0; i < (payload_size / sizeof(Address)); ++i) {
    Address maybe_ptr = payload[i];
#if defined(MEMORY_SANITIZER)
    // |payload| may be uninitialized by design or just contain padding bytes.
    // Copy into a local variable that is unpoisoned for conservative marking.
    // Copy into a temporary variable to maintain the original MSAN state.
    __msan_unpoison(&maybe_ptr, sizeof(maybe_ptr));
#endif
    if (maybe_ptr)
      Heap().CheckAndMarkPointer(this, maybe_ptr);
  }
  AccountMarkedBytes(header);
}

void MarkingVisitor::FlushMarkingWorklist() {
  marking_worklist_.FlushToGlobal();
}

ConcurrentMarkingVisitor::ConcurrentMarkingVisitor(ThreadState* state,
                                                   MarkingMode marking_mode,
                                                   int task_id)
    : MarkingVisitorBase(state, marking_mode, task_id) {
  DCHECK(!state->CheckThread());
  DCHECK_NE(WorklistTaskId::MutatorThread, task_id);
}

void ConcurrentMarkingVisitor::FlushWorklists() {
  // Flush marking worklists for further marking on the mutator thread.
  marking_worklist_.FlushToGlobal();
  not_fully_constructed_worklist_.FlushToGlobal();
  weak_callback_worklist_.FlushToGlobal();
  weak_table_worklist_.FlushToGlobal();
  // Flush compaction worklists.
  movable_reference_worklist_.FlushToGlobal();
  backing_store_callback_worklist_.FlushToGlobal();
}

}  // namespace blink
