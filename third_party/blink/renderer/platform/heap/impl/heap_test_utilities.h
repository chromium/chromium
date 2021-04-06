/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_HEAP_TEST_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_HEAP_TEST_UTILITIES_H_

#include "base/callback.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class HeapPointersOnStackScope final {
  STACK_ALLOCATED();

 public:
  explicit HeapPointersOnStackScope(ThreadState* state) : state_(state) {
    DCHECK(!state_->heap_pointers_on_stack_forced_);
    state_->heap_pointers_on_stack_forced_ = true;
  }
  ~HeapPointersOnStackScope() {
    DCHECK(state_->heap_pointers_on_stack_forced_);
    state_->heap_pointers_on_stack_forced_ = false;
  }

 private:
  ThreadState* const state_;
};

class TestSupportingGC : public testing::Test {
 public:
  // Performs a precise garbage collection with eager sweeping.
  static void PreciselyCollectGarbage(
      BlinkGC::SweepingType sweeping_type = BlinkGC::kEagerSweeping);

  // Performs a conservative garbage collection.
  static void ConservativelyCollectGarbage(
      BlinkGC::SweepingType sweeping_type = BlinkGC::kEagerSweeping);

  ~TestSupportingGC() override;

  // Performs multiple rounds of garbage collections until no more memory can be
  // freed. This is useful to avoid other garbage collections having to deal
  // with stale memory.
  void ClearOutOldGarbage();

 protected:
  base::test::TaskEnvironment task_environment_;
};

// Test driver for compaction.
class CompactionTestDriver {
 public:
  explicit CompactionTestDriver(ThreadState* thread_state)
      : thread_state_(thread_state) {}

  void ForceCompactionForNextGC();

 protected:
  ThreadState* const thread_state_;
};

// Test driver for incremental marking. Assumes that no stack handling is
// required.
class IncrementalMarkingTestDriver {
 public:
  explicit IncrementalMarkingTestDriver(ThreadState* thread_state)
      : thread_state_(thread_state) {}
  ~IncrementalMarkingTestDriver();

  void StartGC();
  virtual void TriggerMarkingSteps(
      BlinkGC::StackState stack_state =
          BlinkGC::StackState::kNoHeapPointersOnStack);
  void FinishGC(bool complete_sweep = true);

  size_t GetHeapCompactLastFixupCount() const;

 protected:
  bool TriggerSingleMarkingStep(
      BlinkGC::StackState stack_state =
          BlinkGC::StackState::kNoHeapPointersOnStack);

  ThreadState* const thread_state_;
};

// Test driver for concurrent marking. Assumes that no stack handling is
// required.
class ConcurrentMarkingTestDriver : public IncrementalMarkingTestDriver {
 public:
  explicit ConcurrentMarkingTestDriver(ThreadState* thread_state)
      : IncrementalMarkingTestDriver(thread_state) {}

  void TriggerMarkingSteps(
      BlinkGC::StackState stack_state =
          BlinkGC::StackState::kNoHeapPointersOnStack) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_HEAP_TEST_UTILITIES_H_
