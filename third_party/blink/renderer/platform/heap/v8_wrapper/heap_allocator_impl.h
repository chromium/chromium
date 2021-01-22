// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_ALLOCATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_ALLOCATOR_IMPL_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_table_backing.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector_backing.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/heap.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/visitor.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT HeapAllocator {
  STATIC_ONLY(HeapAllocator);

 public:
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
  static T* AllocateVectorBacking(size_t) {
    // TODO(1056170): Implement.
    return nullptr;
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
  static T* AllocateHashTableBacking(size_t) {
    // TODO(1056170): Implement.
    return nullptr;
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
    // TODO(1056170): Implement.
    return true;
  }

  static bool IsIncrementalMarking() {
    // TODO(1056170): Implement.
    return false;
  }

  static void EnterGCForbiddenScope() {
    // TODO(1056170): Implement.
  }

  static void LeaveGCForbiddenScope() {
    // TODO(1056170): Implement.
  }

  template <typename T>
  static void BackingWriteBarrier(T**) {
    // TODO(1056170): Implement.
  }

  static void TraceBackingStoreIfMarked(const void* object) {
    // TODO(1056170): Implement.
  }

  template <typename T, typename Traits>
  static void NotifyNewObject(T*) {
    // TODO(1056170): Implement.
  }

  template <typename T, typename Traits>
  static void NotifyNewObjects(T*, size_t) {
    // TODO(1056170): Implement.
  }

  template <typename T, typename Traits>
  static void Trace(Visitor* visitor, const T& t) {
    // TODO(1056170): Forward to TraceInCollectionTrait.
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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_ALLOCATOR_IMPL_H_
