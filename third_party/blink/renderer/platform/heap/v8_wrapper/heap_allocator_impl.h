// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_ALLOCATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_ALLOCATOR_IMPL_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT HeapAllocator {
  STATIC_ONLY(HeapAllocator);

 public:
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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_ALLOCATOR_IMPL_H_
