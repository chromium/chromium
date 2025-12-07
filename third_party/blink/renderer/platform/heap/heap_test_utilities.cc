// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include <memory>

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/cppgc/platform.h"

namespace blink {

namespace {

bool IsGCInProgress() {
  return cppgc::subtle::HeapState::IsMarking(
             ThreadState::Current()->heap_handle()) ||
         cppgc::subtle::HeapState::IsSweeping(
             ThreadState::Current()->heap_handle());
}

}  // namespace

TestSupportingGC::~TestSupportingGC() {
  PreciselyCollectGarbage();
}

// static
void TestSupportingGC::PreciselyCollectGarbage() {
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
}

// static
void TestSupportingGC::ConservativelyCollectGarbage() {
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kMayContainHeapPointers);
}

// static
void TestSupportingGC::ClearOutOldGarbage() {
  PreciselyCollectGarbage();
  auto& cpp_heap = ThreadState::Current()->cpp_heap();
  size_t old_used = cpp_heap.CollectStatistics(cppgc::HeapStatistics::kDetailed)
                        .used_size_bytes;
  while (true) {
    PreciselyCollectGarbage();
    size_t used = cpp_heap.CollectStatistics(cppgc::HeapStatistics::kDetailed)
                      .used_size_bytes;
    if (used >= old_used)
      break;
    old_used = used;
  }
}

CompactionTestDriver::CompactionTestDriver(ThreadState* thread_state)
    : heap_(thread_state->heap_handle()) {}

void CompactionTestDriver::ForceCompactionForNextGC() {
  heap_.ForceCompactionForNextGarbageCollection();
}

IncrementalMarkingTestDriver::IncrementalMarkingTestDriver(
    ThreadState* thread_state)
    : heap_(thread_state->heap_handle()) {}

IncrementalMarkingTestDriver::~IncrementalMarkingTestDriver() {
  if (IsGCInProgress())
    heap_.FinalizeGarbageCollection(cppgc::EmbedderStackState::kNoHeapPointers);
}

void IncrementalMarkingTestDriver::StartGC() {
  heap_.StartGarbageCollection();
}

void IncrementalMarkingTestDriver::TriggerMarkingSteps() {
  CHECK(ThreadState::Current()->IsIncrementalMarking());
  while (!heap_.PerformMarkingStep(ThreadState::StackState::kNoHeapPointers)) {
  }
}

void IncrementalMarkingTestDriver::TriggerMarkingStepsWithStack() {
  CHECK(ThreadState::Current()->IsIncrementalMarking());
  while (!heap_.PerformMarkingStep(
      ThreadState::StackState::kMayContainHeapPointers)) {
  }
}

void IncrementalMarkingTestDriver::FinishGC() {
  CHECK(ThreadState::Current()->IsIncrementalMarking());
  heap_.FinalizeGarbageCollection(cppgc::EmbedderStackState::kNoHeapPointers);
  CHECK(!ThreadState::Current()->IsIncrementalMarking());
}

ConcurrentMarkingTestDriver::ConcurrentMarkingTestDriver(
    ThreadState* thread_state)
    : IncrementalMarkingTestDriver(thread_state) {}

void ConcurrentMarkingTestDriver::StartGC() {
  IncrementalMarkingTestDriver::StartGC();
  heap_.ToggleMainThreadMarking(false);
}

void ConcurrentMarkingTestDriver::TriggerMarkingSteps() {
  CHECK(ThreadState::Current()->IsIncrementalMarking());
  heap_.PerformMarkingStep(ThreadState::StackState::kNoHeapPointers);
}

void ConcurrentMarkingTestDriver::FinishGC() {
  heap_.ToggleMainThreadMarking(true);
  IncrementalMarkingTestDriver::FinishGC();
}

}  // namespace blink
