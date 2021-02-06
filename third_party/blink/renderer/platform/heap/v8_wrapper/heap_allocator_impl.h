// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_ALLOCATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_ALLOCATOR_IMPL_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_table_backing.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector_backing.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/heap.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/visitor.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/cppgc/heap-consistency.h"

namespace blink {

class PLATFORM_EXPORT HeapAllocator {
  STATIC_ONLY(HeapAllocator);

 public:
  using HeapConsistency = cppgc::subtle::HeapConsistency;
  using LivenessBroker = blink::LivenessBroker;

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

  static void FreeVectorBacking(void*) {
    // TODO(1056170): Implement.
  }

  static bool ExpandVectorBacking(void*, size_t) {
    // TODO(1056170): Implement.
    return false;
  }

  static bool ShrinkVectorBacking(void*, size_t, size_t) {
    // TODO(1056170): Implement.
    return false;
  }

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

  static void FreeHashTableBacking(void*) {
    // TODO(1056170): Implement.
  }

  static bool ExpandHashTableBacking(void*, size_t) {
    // TODO(1056170): Implement.
    return false;
  }

  static bool IsAllocationAllowed() {
    return cppgc::subtle::DisallowGarbageCollectionScope::
        IsGarbageCollectionAllowed(
            ThreadState::Current()->cpp_heap().GetHeapHandle());
  }

  static bool IsIncrementalMarking() {
    auto& heap_handle = ThreadState::Current()->cpp_heap().GetHeapHandle();
    return cppgc::subtle::HeapState::IsMarking(heap_handle) &&
           !cppgc::subtle::HeapState::IsInAtomicPause(heap_handle);
  }

  static void EnterGCForbiddenScope() {
    cppgc::subtle::NoGarbageCollectionScope::Enter(
        ThreadState::Current()->cpp_heap().GetHeapHandle());
  }

  static void LeaveGCForbiddenScope() {
    cppgc::subtle::NoGarbageCollectionScope::Leave(
        ThreadState::Current()->cpp_heap().GetHeapHandle());
  }

  template <typename Traits>
  static bool CanReuseHashTableDeletedBucket() {
    if (Traits::kEmptyValueIsZero || !Traits::kCanTraceConcurrently)
      return true;
    return !IsIncrementalMarking();
  }

  template <typename T>
  static void BackingWriteBarrier(T** slot) {
    HeapConsistency::WriteBarrierParams params;
    switch (HeapConsistency::GetWriteBarrierType(slot, *slot, params)) {
      case HeapConsistency::WriteBarrierType::kMarking:
        HeapConsistency::DijkstraWriteBarrier(params, *slot);
        break;
      case HeapConsistency::WriteBarrierType::kGenerational:
        HeapConsistency::GenerationalBarrier(params, slot);
        break;
      case HeapConsistency::WriteBarrierType::kNone:
        break;
      default:
        break;  // TODO(1056170): Remove default case when API is stable.
    }
  }

  template <typename T>
  static void TraceBackingStoreIfMarked(T** slot) {
    HeapConsistency::WriteBarrierParams params;
    if (HeapConsistency::GetWriteBarrierType(slot, *slot, params) ==
        HeapConsistency::WriteBarrierType::kMarking) {
      HeapConsistency::SteeleWriteBarrier(params, *slot);
    }
  }

  template <typename T, typename Traits>
  static void NotifyNewObject(T* slot_in_backing) {
    HeapConsistency::WriteBarrierParams params;
    // `slot_in_backing` points into a backing store and T is not necessarily a
    // garbage collected type but may be kept inline.
    switch (HeapConsistency::GetWriteBarrierType(
        slot_in_backing, params, []() -> cppgc::HeapHandle& {
          return ThreadState::Current()->cpp_heap().GetHeapHandle();
        })) {
      case HeapConsistency::WriteBarrierType::kMarking:
        HeapConsistency::DijkstraWriteBarrierRange(
            params, slot_in_backing, sizeof(T), 1,
            TraceCollectionIfEnabled<WTF::kNoWeakHandling, T, Traits>::Trace);
        break;
      case HeapConsistency::WriteBarrierType::kGenerational:
        HeapConsistency::GenerationalBarrier(params, slot_in_backing);
        break;
      case HeapConsistency::WriteBarrierType::kNone:
        break;
      default:
        break;  // TODO(1056170): Remove default case when API is stable.
    }
  }

  template <typename T, typename Traits>
  static void NotifyNewObjects(T* first_element, size_t length) {
    HeapConsistency::WriteBarrierParams params;
    // `first_element` points into a backing store and T is not necessarily a
    // garbage collected type but may be kept inline.
    switch (HeapConsistency::GetWriteBarrierType(
        first_element, params, []() -> cppgc::HeapHandle& {
          return ThreadState::Current()->cpp_heap().GetHeapHandle();
        })) {
      case HeapConsistency::WriteBarrierType::kMarking:
        HeapConsistency::DijkstraWriteBarrierRange(
            params, first_element, sizeof(T), length,
            TraceCollectionIfEnabled<WTF::kNoWeakHandling, T, Traits>::Trace);
        break;
      case HeapConsistency::WriteBarrierType::kGenerational:
        HeapConsistency::GenerationalBarrier(params, first_element);
        break;
      case HeapConsistency::WriteBarrierType::kNone:
        break;
      default:
        break;  // TODO(1056170): Remove default case when API is stable.
    }
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
    visitor->RegisterMovableReference(const_cast<const HeapVectorBacking<T>**>(
        reinterpret_cast<const HeapVectorBacking<T>* const*>(backing_slot)));
    visitor->Trace(reinterpret_cast<const HeapVectorBacking<T>*>(backing));
  }

  template <typename T, typename HashTable>
  static void TraceHashTableBackingStrongly(Visitor* visitor,
                                            const T* backing,
                                            const T* const* backing_slot) {
    visitor->RegisterMovableReference(
        const_cast<const HeapHashTableBacking<HashTable>**>(
            reinterpret_cast<const HeapHashTableBacking<HashTable>* const*>(
                backing_slot)));
    visitor->Trace(
        reinterpret_cast<const HeapHashTableBacking<HashTable>*>(backing));
  }

  template <typename T, typename HashTable>
  static void TraceHashTableBackingWeakly(Visitor* visitor,
                                          const T* backing,
                                          const T* const* backing_slot,
                                          WeakCallback callback,
                                          const void* parameter) {
    visitor->RegisterMovableReference(
        const_cast<const HeapHashTableBacking<HashTable>**>(
            reinterpret_cast<const HeapHashTableBacking<HashTable>* const*>(
                backing_slot)));
    visitor->TraceWeakContainer(
        reinterpret_cast<const HeapHashTableBacking<HashTable>*>(backing),
        callback, parameter);
  }

  static bool DeferTraceToMutatorThreadIfConcurrent(Visitor* visitor,
                                                    const void* object,
                                                    TraceCallback callback,
                                                    size_t deferred_size) {
    return visitor->DeferTraceToMutatorThreadIfConcurrent(object, callback,
                                                          deferred_size);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_ALLOCATOR_IMPL_H_
