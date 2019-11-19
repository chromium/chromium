// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PROCESS_HEAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PROCESS_HEAP_H_

#include <atomic>
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class CrossThreadPersistentRegion;

class PLATFORM_EXPORT ProcessHeap {
  STATIC_ONLY(ProcessHeap);

 public:
  static void Init();

  static CrossThreadPersistentRegion& GetCrossThreadPersistentRegion();
  static CrossThreadPersistentRegion& GetCrossThreadWeakPersistentRegion();

  // Access to the CrossThreadPersistentRegion from multiple threads has to be
  // prevented as allocation, freeing, and iteration of nodes may otherwise
  // cause data races.
  //
  // Examples include:
  // - Iteration of strong cross-thread Persistents.
  // - Iteration and processing of weak cross-thread Persistents. The lock
  //   needs to span both operations as iteration of weak persistents only
  //   registers memory regions that are then processed afterwards.
  // - Marking phase in garbage collection: The whole phase requires locking
  //   as CrossThreadWeakPersistents may be converted to CrossThreadPersistent
  //   which must observe GC as an atomic operation.
  static Mutex& CrossThreadPersistentMutex();

  static void IncreaseTotalAllocatedObjectSize(size_t delta) {
    total_allocated_object_size_.fetch_add(delta, std::memory_order_relaxed);
  }
  static void DecreaseTotalAllocatedObjectSize(size_t delta) {
    total_allocated_object_size_.fetch_sub(delta, std::memory_order_relaxed);
  }
  static size_t TotalAllocatedObjectSize() {
    return total_allocated_object_size_.load(std::memory_order_relaxed);
  }
  static void IncreaseTotalAllocatedSpace(size_t delta) {
    total_allocated_space_.fetch_add(delta, std::memory_order_relaxed);
  }
  static void DecreaseTotalAllocatedSpace(size_t delta) {
    total_allocated_space_.fetch_sub(delta, std::memory_order_relaxed);
  }
  static size_t TotalAllocatedSpace() {
    return total_allocated_space_.load(std::memory_order_relaxed);
  }
  static void ResetHeapCounters();

 private:
  static std::atomic_size_t total_allocated_space_;
  static std::atomic_size_t total_allocated_object_size_;

  friend class ThreadState;
};

}  // namespace blink

#endif
