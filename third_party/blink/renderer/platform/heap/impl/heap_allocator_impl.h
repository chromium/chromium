// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_HEAP_ALLOCATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_HEAP_ALLOCATOR_IMPL_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_table_backing.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector_backing.h"
#include "third_party/blink/renderer/platform/heap/impl/heap.h"
#include "third_party/blink/renderer/platform/heap/impl/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/impl/trace_traits.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

// This is a static-only class used as a trait on collections to make them heap
// allocated.
class PLATFORM_EXPORT HeapAllocator {
  STATIC_ONLY(HeapAllocator);

 public:
  using LivenessBroker = blink::LivenessBroker;
  using Visitor = blink::Visitor;
  static constexpr bool kIsGarbageCollected = true;

  // See wtf/size_t.h for details.
  static constexpr size_t kMaxHeapObjectSizeLog2 = 27;
  static constexpr size_t kMaxHeapObjectSize = 1 << kMaxHeapObjectSizeLog2;

  template <typename T>
  static size_t MaxElementCountInBackingStore() {
    return kMaxHeapObjectSize / sizeof(T);
  }

  template <typename T>
  static size_t QuantizedSize(size_t count) {
    CHECK_LE(count, MaxElementCountInBackingStore<T>());
    // Oilpan's internal size is independent of MaxElementCountInBackingStore()
    // and the required size to match capacity needs.
    return count * sizeof(T);
  }

  template <typename T>
  static T* AllocateVectorBacking(size_t size) {
    return reinterpret_cast<T*>(
        MakeGarbageCollected<HeapVectorBacking<T>>(size / sizeof(T)));
  }
  static void FreeVectorBacking(void*);
  static bool ExpandVectorBacking(void*, size_t);
  static bool ShrinkVectorBacking(void* address,
                                  size_t quantized_current_size,
                                  size_t quantized_shrunk_size);

  template <typename T, typename HashTable>
  static T* AllocateHashTableBacking(size_t size) {
    static_assert(sizeof(T) == sizeof(typename HashTable::ValueType),
                  "T must match ValueType.");
    return reinterpret_cast<T*>(
        MakeGarbageCollected<HeapHashTableBacking<HashTable>>(size /
                                                              sizeof(T)));
  }
  template <typename T, typename HashTable>
  static T* AllocateZeroedHashTableBacking(size_t size) {
    return AllocateHashTableBacking<T, HashTable>(size);
  }
  static void FreeHashTableBacking(void* address);
  static bool ExpandHashTableBacking(void*, size_t);

  template <typename Traits>
  static bool CanReuseHashTableDeletedBucket() {
    if (Traits::kEmptyValueIsZero || !Traits::kCanTraceConcurrently)
      return true;
    return !ThreadState::Current()->IsMarkingInProgress();
  }

  static bool IsAllocationAllowed() {
    return ThreadState::Current()->IsAllocationAllowed();
  }

  static bool IsIncrementalMarking() {
    return ThreadState::IsAnyIncrementalMarking() &&
           ThreadState::Current()->IsIncrementalMarking();
  }

  static void EnterGCForbiddenScope() {
    ThreadState::Current()->EnterGCForbiddenScope();
  }

  static void LeaveGCForbiddenScope() {
    ThreadState::Current()->LeaveGCForbiddenScope();
  }

  template <typename T, typename Traits>
  static void Trace(Visitor* visitor, const T& t) {
    TraceCollectionIfEnabled<WTF::WeakHandlingTrait<T>::value, T,
                             Traits>::Trace(visitor, &t);
  }

  template <typename T>
  static void TraceVectorBacking(Visitor* visitor,
                                 const T* backing,
                                 const T* const* backing_slot) {
    visitor->TraceMovablePointer(backing_slot);
    visitor->Trace(reinterpret_cast<const HeapVectorBacking<T>*>(backing));
  }

  template <typename T, typename HashTable>
  static void TraceHashTableBackingStrongly(Visitor* visitor,
                                            const T* backing,
                                            const T* const* backing_slot) {
    visitor->TraceMovablePointer(backing_slot);
    visitor->Trace(
        reinterpret_cast<const HeapHashTableBacking<HashTable>*>(backing));
  }

  template <typename T, typename HashTable>
  static void TraceHashTableBackingWeakly(Visitor* visitor,
                                          const T* backing,
                                          const T* const* backing_slot,
                                          WeakCallback callback,
                                          const void* parameter) {
    visitor->TraceMovablePointer(backing_slot);
    visitor->TraceWeakContainer(
        reinterpret_cast<const HeapHashTableBacking<HashTable>*>(backing),
        reinterpret_cast<const HeapHashTableBacking<HashTable>* const*>(
            backing_slot),
        TraceTrait<HeapHashTableBacking<HashTable>>::GetTraceDescriptor(
            backing),
        TraceTrait<HeapHashTableBacking<HashTable>>::GetWeakTraceDescriptor(
            backing),
        callback, parameter);
  }

  template <typename T>
  static void BackingWriteBarrier(T** slot) {
    MarkingVisitor::WriteBarrier(reinterpret_cast<void**>(slot));
  }

  template <typename T>
  static void TraceBackingStoreIfMarked(T** slot) {
    MarkingVisitor::RetraceObject(*slot);
  }

  template <typename T, typename Traits>
  static void NotifyNewObject(T* object) {
    MarkingVisitor::WriteBarrier(
        []() { return ThreadState::Current(); }, object, sizeof(T), 1,
        TraceCollectionIfEnabled<WTF::kNoWeakHandling, T, Traits>::Trace);
  }

  template <typename T, typename Traits>
  static void NotifyNewObjects(T* array, size_t len) {
    MarkingVisitor::WriteBarrier(
        []() { return ThreadState::Current(); }, array, sizeof(T), len,
        TraceCollectionIfEnabled<WTF::kNoWeakHandling, T, Traits>::Trace);
  }

  static bool DeferTraceToMutatorThreadIfConcurrent(Visitor* visitor,
                                                    const void* object,
                                                    TraceCallback callback,
                                                    size_t deferred_size) {
    return visitor->DeferredTraceIfConcurrent({object, callback},
                                              deferred_size);
  }

 private:
  static void BackingFree(void*);
  static bool BackingExpand(void*, size_t);
  static bool BackingShrink(void*,
                            size_t quantized_current_size,
                            size_t quantized_shrunk_size);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_HEAP_ALLOCATOR_IMPL_H_
