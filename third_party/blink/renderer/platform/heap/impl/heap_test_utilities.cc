/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/impl/heap_compact.h"

namespace blink {

std::atomic_int IntegerObject::destructor_calls{0};

// static
void TestSupportingGC::PreciselyCollectGarbage(
    BlinkGC::SweepingType sweeping_type) {
  ThreadState::Current()->CollectGarbageForTesting(
      BlinkGC::CollectionType::kMajor, BlinkGC::kNoHeapPointersOnStack,
      BlinkGC::kAtomicMarking, sweeping_type,
      BlinkGC::GCReason::kForcedGCForTesting);
}

// static
void TestSupportingGC::ConservativelyCollectGarbage(
    BlinkGC::SweepingType sweeping_type) {
  ThreadState::Current()->CollectGarbageForTesting(
      BlinkGC::CollectionType::kMajor, BlinkGC::kHeapPointersOnStack,
      BlinkGC::kAtomicMarking, sweeping_type,
      BlinkGC::GCReason::kForcedGCForTesting);
}

TestSupportingGC::~TestSupportingGC() {
  // Complete sweeping before |task_environment_| is destroyed.
  CompleteSweepingIfNeeded();
}

void TestSupportingGC::ClearOutOldGarbage() {
  PreciselyCollectGarbage();
  ThreadHeap& heap = ThreadState::Current()->Heap();
  while (true) {
    size_t used = heap.ObjectPayloadSizeForTesting();
    PreciselyCollectGarbage();
    if (heap.ObjectPayloadSizeForTesting() >= used)
      break;
  }
}

void TestSupportingGC::CompleteSweepingIfNeeded() {
  if (ThreadState::Current()->IsSweepingInProgress())
    ThreadState::Current()->CompleteSweep();
}

IncrementalMarkingTestDriver::~IncrementalMarkingTestDriver() {
  if (thread_state_->IsIncrementalMarking())
    FinishGC();
}

void IncrementalMarkingTestDriver::Start() {
  thread_state_->IncrementalMarkingStartForTesting();
}

bool IncrementalMarkingTestDriver::SingleStep(BlinkGC::StackState stack_state) {
  CHECK(thread_state_->IsIncrementalMarking());
  if (thread_state_->GetGCState() ==
      ThreadState::kIncrementalMarkingStepScheduled) {
    thread_state_->IncrementalMarkingStep(stack_state);
    return true;
  }
  return false;
}

void IncrementalMarkingTestDriver::FinishSteps(
    BlinkGC::StackState stack_state) {
  CHECK(thread_state_->IsIncrementalMarking());
  while (SingleStep(stack_state)) {
  }
}

bool IncrementalMarkingTestDriver::SingleConcurrentStep(
    BlinkGC::StackState stack_state) {
  CHECK(thread_state_->IsIncrementalMarking());
  if (thread_state_->GetGCState() ==
      ThreadState::kIncrementalMarkingStepScheduled) {
    thread_state_->SkipIncrementalMarkingForTesting();
    thread_state_->IncrementalMarkingStep(stack_state);
    return true;
  }
  return false;
}

void IncrementalMarkingTestDriver::FinishConcurrentSteps(
    BlinkGC::StackState stack_state) {
  CHECK(thread_state_->IsIncrementalMarking());
  while (SingleConcurrentStep(stack_state)) {
  }
}

void IncrementalMarkingTestDriver::FinishGC(bool complete_sweep) {
  CHECK(thread_state_->IsIncrementalMarking());
  FinishSteps(BlinkGC::StackState::kNoHeapPointersOnStack);
  CHECK_EQ(ThreadState::kIncrementalMarkingFinalizeScheduled,
           thread_state_->GetGCState());
  thread_state_->IncrementalMarkingFinalize();
  CHECK(!thread_state_->IsIncrementalMarking());
  if (complete_sweep) {
    thread_state_->CompleteSweep();
  }
}

size_t IncrementalMarkingTestDriver::GetHeapCompactLastFixupCount() const {
  HeapCompact* compaction = ThreadState::Current()->Heap().Compaction();
  return compaction->LastFixupCountForTesting();
}

}  // namespace blink
