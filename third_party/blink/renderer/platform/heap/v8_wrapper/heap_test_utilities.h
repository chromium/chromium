// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_TEST_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_TEST_UTILITIES_H_

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8-cppgc.h"
#include "v8.h"
#include "v8/include/cppgc/testing.h"

namespace blink {

// Allows for overriding the stack state for the purpose of testing. Any garbage
// collection calls scoped with `HeapPointersOnStackScope` will perform
// conservative stack scanning, even if other (more local) hints indicate that
// there's no need for it.
class HeapPointersOnStackScope final {
  STACK_ALLOCATED();

 public:
  explicit HeapPointersOnStackScope(const ThreadState* state)
      : embedder_stack_state_(
            state->cpp_heap().GetHeapHandle(),
            cppgc::EmbedderStackState::kMayContainHeapPointers) {}

  HeapPointersOnStackScope(const HeapPointersOnStackScope&) = delete;
  HeapPointersOnStackScope& operator=(const HeapPointersOnStackScope&) = delete;

 private:
  cppgc::testing::OverrideEmbedderStackStateScope embedder_stack_state_;
};

class TestSupportingGC : public testing::Test {
 public:
  ~TestSupportingGC() override { CompleteGarbageCollectionIfNeeded(); }

  // Performs a precise garbage collection with eager sweeping.
  static void PreciselyCollectGarbage() {
    ThreadState::Current()->CollectAllGarbageForTesting(
        BlinkGC::kNoHeapPointersOnStack);
  }

  // Performs a conservative garbage collection with eager sweeping.
  static void ConservativelyCollectGarbage() {
    ThreadState::Current()->CollectAllGarbageForTesting(
        BlinkGC::kHeapPointersOnStack);
  }

  // Performs multiple rounds of garbage collections until no more memory can be
  // freed. This is useful to avoid other garbage collections having to deal
  // with stale memory.
  void ClearOutOldGarbage() {}

  // Completes GC if it is currently running.
  void CompleteGarbageCollectionIfNeeded() {}

  void ForceCompactionForNextGC() {}

 protected:
  base::test::TaskEnvironment task_environment_;
};

// Test driver for incremental marking. Assumes that no stack handling is
// required.
class IncrementalMarkingTestDriver {
 public:
  explicit IncrementalMarkingTestDriver(ThreadState* thread_state)
      : cpp_heap_(thread_state->cpp_heap()) {}
  ~IncrementalMarkingTestDriver();

  void StartGC() {}
  virtual void TriggerMarkingSteps(
      BlinkGC::StackState stack_state =
          BlinkGC::StackState::kNoHeapPointersOnStack) {}
  void FinishGC() {}

 protected:
  v8::CppHeap& cpp_heap_;
};

// Test driver for incremental marking. Assumes that no stack handling is
// required.
class ConcurrentMarkingTestDriver : public IncrementalMarkingTestDriver {
 public:
  explicit ConcurrentMarkingTestDriver(ThreadState* thread_state)
      : IncrementalMarkingTestDriver(thread_state) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_TEST_UTILITIES_H_
