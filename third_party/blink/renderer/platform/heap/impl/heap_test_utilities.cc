/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/impl/heap_compact.h"

namespace blink {

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
  if (ThreadState::Current()->IsSweepingInProgress())
    ThreadState::Current()->CompleteSweep();
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

void CompactionTestDriver::ForceCompactionForNextGC() {
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
}

IncrementalMarkingTestDriver::~IncrementalMarkingTestDriver() {
  if (thread_state_->IsIncrementalMarking())
    FinishGC();
}

void IncrementalMarkingTestDriver::StartGC() {
  thread_state_->IncrementalMarkingStartForTesting();
}

bool IncrementalMarkingTestDriver::TriggerSingleMarkingStep(
    BlinkGC::StackState stack_state) {
  CHECK(thread_state_->IsIncrementalMarking());
  if (thread_state_->GetGCState() ==
      ThreadState::kIncrementalMarkingStepScheduled) {
    thread_state_->IncrementalMarkingStep(stack_state);
    return true;
  }
  return false;
}

void IncrementalMarkingTestDriver::TriggerMarkingSteps(
    BlinkGC::StackState stack_state) {
  CHECK(thread_state_->IsIncrementalMarking());
  while (TriggerSingleMarkingStep(stack_state)) {
  }
}

void IncrementalMarkingTestDriver::FinishGC(bool complete_sweep) {
  CHECK(thread_state_->IsIncrementalMarking());
  IncrementalMarkingTestDriver::TriggerMarkingSteps(
      BlinkGC::StackState::kNoHeapPointersOnStack);
  CHECK_EQ(ThreadState::kIncrementalMarkingFinalizeScheduled,
           thread_state_->GetGCState());
  thread_state_->ForceNoFollowupFullGCForTesting();
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

void ConcurrentMarkingTestDriver::TriggerMarkingSteps(
    BlinkGC::StackState stack_state) {
  if (thread_state_->GetGCState() ==
      ThreadState::kIncrementalMarkingStepScheduled) {
    thread_state_->SkipIncrementalMarkingForTesting();
    TriggerSingleMarkingStep();
  }
}

}  // namespace blink
