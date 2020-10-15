// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/marking_visitor.h"

#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

MarkingVisitorBase::MarkingVisitorBase(ThreadState* state,
                                       MarkingMode marking_mode,
                                       int task_id)
    : Visitor(state),
      marking_worklist_(Heap().GetMarkingWorklist(), task_id),
      write_barrier_worklist_(Heap().GetWriteBarrierWorklist(), task_id),
      not_fully_constructed_worklist_(Heap().GetNotFullyConstructedWorklist(),
                                      task_id),
      weak_callback_worklist_(Heap().GetWeakCallbackWorklist(), task_id),
      movable_reference_worklist_(Heap().GetMovableReferenceWorklist(),
                                  task_id),
      discovered_ephemeron_pairs_worklist_(
          Heap().GetDiscoveredEphemeronPairsWorklist(),
          task_id),
      ephemeron_pairs_to_process_worklist_(
          Heap().GetEphemeronPairsToProcessWorklist(),
          task_id),
      marking_mode_(marking_mode),
      task_id_(task_id) {}

void MarkingVisitorBase::FlushCompactionWorklists() {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  movable_reference_worklist_.FlushToGlobal();
}

void MarkingVisitorBase::RegisterWeakCallback(WeakCallback callback,
                                              const void* object) {
  weak_callback_worklist_.Push({callback, object});
}

void MarkingVisitorBase::RegisterMovableSlot(const void* const* slot) {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  if (Heap().ShouldRegisterMovingAddress()) {
    movable_reference_worklist_.Push(slot);
  }
}

void MarkingVisitorBase::VisitWeak(const void* object,
                                   const void* object_weak_ref,
                                   TraceDescriptor desc,
                                   WeakCallback callback) {
  HeapObjectHeader* header =
      HeapObjectHeader::FromPayload(desc.base_object_payload);
  if (header->IsInConstruction<HeapObjectHeader::AccessMode::kAtomic>()) {
    not_fully_constructed_worklist_.Push(desc.base_object_payload);
    return;
  }
  // Filter out already marked values. The write barrier for WeakMember
  // ensures that any newly set value after this point is kept alive and does
  // not require the callback.
  if (header->IsMarked<HeapObjectHeader::AccessMode::kAtomic>())
    return;
  RegisterWeakCallback(callback, object_weak_ref);
}

void MarkingVisitorBase::VisitEphemeron(const void* key,
                                        TraceDescriptor value_desc) {
  HeapObjectHeader* key_header = HeapObjectHeader::FromPayload(key);
  if (!key_header->IsInConstruction<HeapObjectHeader::AccessMode::kAtomic>() &&
      !key_header->IsMarked<HeapObjectHeader::AccessMode::kAtomic>()) {
    // In construction keys are considered as marked because they are
    // guaranteed to be marked by the end of GC (e.g. by write barrier
    // on insertion to HashTable).
    discovered_ephemeron_pairs_worklist_.Push({key, value_desc});
    return;
  }
  value_desc.callback(this, value_desc.base_object_payload);
}

void MarkingVisitorBase::VisitWeakContainer(
    const void* object,
    const void* const*,
    TraceDescriptor,
    TraceDescriptor weak_desc,
    WeakCallback weak_callback,
    const void* weak_callback_parameter) {
  // In case there's no object present, weakness processing is omitted. The GC
  // relies on the fact that in such cases touching the weak data structure will
  // strongify its references.
  if (!object)
    return;

  HeapObjectHeader* header = HeapObjectHeader::FromPayload(object);
  // We shouldn't trace an in-construction backing store of a weak container.
  // If this container is an ephemeron, we will try to iterate over it's
  // bucket which is unsafe when the backing store is in construction.
  if (header->IsInConstruction<HeapObjectHeader::AccessMode::kAtomic>()) {
    not_fully_constructed_worklist_.Push(object);
    return;
  }

  // Only trace the container initially. Its buckets will be processed after
  // marking. The interesting cases  are:
  // - The backing of the container is dropped using clear(): The backing can
  //   still be compacted but empty/deleted buckets will only be destroyed once
  //   the backing is reclaimed by the garbage collector on the next cycle.
  // - The container expands/shrinks: Buckets are moved to the new backing
  //   store and strongified, resulting in all buckets being alive. The old
  //   backing store is marked but only contains empty/deleted buckets as all
  //   non-empty/deleted buckets have been moved to the new backing store.
  MarkHeaderNoTracing(header);
  AccountMarkedBytes(header);

  // Register final weak processing of the backing store.
  RegisterWeakCallback(weak_callback, weak_callback_parameter);
  // Register ephemeron callbacks if necessary.
  if (weak_desc.callback)
    weak_desc.callback(this, weak_desc.base_object_payload);
}

void MarkingVisitorBase::DynamicallyMarkAddress(ConstAddress address) {
  constexpr HeapObjectHeader::AccessMode mode =
      HeapObjectHeader::AccessMode::kAtomic;
  HeapObjectHeader* const header =
      HeapObjectHeader::FromInnerAddress<mode>(address);
  DCHECK(header);
  DCHECK(!header->IsInConstruction<mode>());
  if (MarkHeaderNoTracing(header)) {
    marking_worklist_.Push({reinterpret_cast<void*>(header->Payload()),
                            GCInfo::From(header->GcInfoIndex<mode>()).trace});
  }
}

// static
bool MarkingVisitor::MarkValue(void* value,
                               BasePage* base_page,
                               ThreadState* thread_state) {
  HeapObjectHeader* header;
  if (LIKELY(!base_page->IsLargeObjectPage())) {
    header = reinterpret_cast<HeapObjectHeader*>(
        static_cast<NormalPage*>(base_page)->FindHeaderFromAddress(
            reinterpret_cast<Address>(value)));
  } else {
    LargeObjectPage* large_page = static_cast<LargeObjectPage*>(base_page);
    header = large_page->ObjectHeader();
  }

  if (!header->TryMark<HeapObjectHeader::AccessMode::kAtomic>())
    return false;

  MarkingVisitor* visitor = thread_state->CurrentVisitor();
  if (UNLIKELY(
          header->IsInConstruction<HeapObjectHeader::AccessMode::kAtomic>())) {
    // It is assumed that objects on not_fully_constructed_worklist_ are not
    // marked.
    header->Unmark<HeapObjectHeader::AccessMode::kAtomic>();
    visitor->not_fully_constructed_worklist_.Push(header->Payload());
    return true;
  }

  visitor->write_barrier_worklist_.Push(header);
  return true;
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

  return MarkValue(value, base_page, thread_state);
}

void MarkingVisitor::GenerationalBarrierSlow(Address slot,
                                             ThreadState* thread_state) {
  BasePage* slot_page = thread_state->Heap().LookupPageForAddress(slot);
  DCHECK(slot_page);

  if (UNLIKELY(slot_page->IsLargeObjectPage())) {
    auto* large_page = static_cast<LargeObjectPage*>(slot_page);
    if (UNLIKELY(large_page->ObjectHeader()->IsOld())) {
      large_page->SetRemembered(true);
    }
    return;
  }

  auto* normal_page = static_cast<NormalPage*>(slot_page);
  const HeapObjectHeader* source_header = reinterpret_cast<HeapObjectHeader*>(
      normal_page->object_start_bit_map()->FindHeader(slot));
  DCHECK_LT(0u, source_header->GcInfoIndex());
  DCHECK_GT(source_header->PayloadEnd(), slot);
  if (UNLIKELY(source_header->IsOld())) {
    normal_page->MarkCard(slot);
  }
}

void MarkingVisitor::TraceMarkedBackingStoreSlow(const void* value) {
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

  GCInfo::From(header->GcInfoIndex())
      .trace(thread_state->CurrentVisitor(), value);
}

MarkingVisitor::MarkingVisitor(ThreadState* state, MarkingMode marking_mode)
    : MarkingVisitorBase(state, marking_mode, WorklistTaskId::MutatorThread) {
  DCHECK(state->InAtomicMarkingPause());
  DCHECK(state->CheckThread());
}

void MarkingVisitor::ConservativelyMarkAddress(BasePage* page,
                                               ConstAddress address) {
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
  if (!header->IsInConstruction()) {
    MarkHeader(header,
               {header->Payload(), GCInfo::From(header->GcInfoIndex()).trace});
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

void MarkingVisitor::FlushMarkingWorklists() {
  marking_worklist_.FlushToGlobal();
  write_barrier_worklist_.FlushToGlobal();
}

ConcurrentMarkingVisitor::ConcurrentMarkingVisitor(ThreadState* state,
                                                   MarkingMode marking_mode,
                                                   int task_id)
    : MarkingVisitorBase(state, marking_mode, task_id),
      not_safe_to_concurrently_trace_worklist_(
          Heap().GetNotSafeToConcurrentlyTraceWorklist(),
          task_id),
      previously_not_fully_constructed_worklist_(
          Heap().GetPreviouslyNotFullyConstructedWorklist(),
          task_id) {
  DCHECK(!state->CheckThread());
  DCHECK_NE(WorklistTaskId::MutatorThread, task_id);
}

ConcurrentMarkingVisitor::~ConcurrentMarkingVisitor() {
  // ConcurrentMarkingVisitor should report all its marked_bytes before dying.
  DCHECK_EQ(marked_bytes_, last_marked_bytes_);
}

void ConcurrentMarkingVisitor::FlushWorklists() {
  // Flush marking worklists for further marking on the mutator thread.
  marking_worklist_.FlushToGlobal();
  write_barrier_worklist_.FlushToGlobal();
  not_fully_constructed_worklist_.FlushToGlobal();
  previously_not_fully_constructed_worklist_.FlushToGlobal();
  weak_callback_worklist_.FlushToGlobal();
  discovered_ephemeron_pairs_worklist_.FlushToGlobal();
  ephemeron_pairs_to_process_worklist_.FlushToGlobal();
  not_safe_to_concurrently_trace_worklist_.FlushToGlobal();
  // Flush compaction worklists.
  if (marking_mode_ == kGlobalMarkingWithCompaction) {
    movable_reference_worklist_.FlushToGlobal();
  }
}

}  // namespace blink
