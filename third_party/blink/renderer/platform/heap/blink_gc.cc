// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/blink_gc.h"

#include "base/logging.h"

namespace blink {

const char* BlinkGC::ToString(BlinkGC::GCReason reason) {
  switch (reason) {
    case BlinkGC::GCReason::kPreciseGC:
      return "PreciseGC";
    case BlinkGC::GCReason::kConservativeGC:
      return "ConservativeGC";
    case BlinkGC::GCReason::kForcedGCForTesting:
      return "ForcedGCForTesting";
    case BlinkGC::GCReason::kMemoryPressureGC:
      return "MemoryPressureGC";
    case BlinkGC::GCReason::kThreadTerminationGC:
      return "ThreadTerminationGC";
    case BlinkGC::GCReason::kIncrementalV8FollowupGC:
      return "IncrementalV8FollowupGC";
    case BlinkGC::GCReason::kUnifiedHeapGC:
      return "UnifiedHeapGC";
    case BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC:
      return "UnifiedHeapForMemoryReductionGC";
  }
  IMMEDIATE_CRASH();
}

const char* BlinkGC::ToString(BlinkGC::MarkingType type) {
  switch (type) {
    case BlinkGC::MarkingType::kAtomicMarking:
      return "AtomicMarking";
    case BlinkGC::MarkingType::kIncrementalAndConcurrentMarking:
      return "IncrementalAndConcurrentMarking";
  }
  IMMEDIATE_CRASH();
}

const char* BlinkGC::ToString(BlinkGC::SweepingType type) {
  switch (type) {
    case BlinkGC::SweepingType::kConcurrentAndLazySweeping:
      return "ConcurrentAndLazySweeping";
    case BlinkGC::SweepingType::kEagerSweeping:
      return "EagerSweeping";
  }
  IMMEDIATE_CRASH();
}

const char* BlinkGC::ToString(BlinkGC::StackState stack_state) {
  switch (stack_state) {
    case BlinkGC::kNoHeapPointersOnStack:
      return "NoHeapPointersOnStack";
    case BlinkGC::kHeapPointersOnStack:
      return "HeapPointersOnStack";
  }
  IMMEDIATE_CRASH();
}

const char* BlinkGC::ToString(BlinkGC::ArenaIndices arena_index) {
#define ArenaCase(name)     \
  case k##name##ArenaIndex: \
    return "" #name "Arena";

  switch (arena_index) {
    FOR_EACH_ARENA(ArenaCase)

    case BlinkGC::ArenaIndices::kNumberOfArenas:
      IMMEDIATE_CRASH();
  }
  IMMEDIATE_CRASH();

#undef ArenaCase
}

}  // namespace blink
