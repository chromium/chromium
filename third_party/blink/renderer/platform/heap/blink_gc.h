// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_BLINK_GC_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_BLINK_GC_H_

// BlinkGC.h is a file that defines common things used by Blink GC.

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#define PRINT_HEAP_STATS 0  // Enable this macro to print heap stats to stderr.

namespace blink {

class WeakCallbackInfo;
class MarkingVisitor;
class Visitor;

using Address = uint8_t*;

using FinalizationCallback = void (*)(void*);
using VisitorCallback = void (*)(Visitor*, void*);
using MarkingVisitorCallback = void (*)(MarkingVisitor*, void*);
using TraceCallback = VisitorCallback;
using WeakCallback = void (*)(const WeakCallbackInfo&, void*);
using EphemeronCallback = VisitorCallback;

// Simple alias to avoid heap compaction type signatures turning into
// a sea of generic |void*|s.
using MovableReference = void*;

// Heap compaction supports registering callbacks that are to be invoked
// when an object is moved during compaction. This is to support internal
// location fixups that need to happen as a result.
//
// i.e., when the object residing at |from| is moved to |to| by the compaction
// pass, invoke the callback to adjust any internal references that now need
// to be |to|-relative.
using MovingObjectCallback = void (*)(MovableReference from,
                                      MovableReference to,
                                      size_t);

// List of all arenas. Includes typed arenas as well.
#define FOR_EACH_ARENA(H) \
  H(NormalPage1)          \
  H(NormalPage2)          \
  H(NormalPage3)          \
  H(NormalPage4)          \
  H(Vector)               \
  H(HashTable)            \
  H(Node)                 \
  H(CSSValue)             \
  H(LargeObject)

class PLATFORM_EXPORT WorklistTaskId {
 public:
  static constexpr int MutatorThread = 0;
  static constexpr int ConcurrentThreadBase = 1;
};

class PLATFORM_EXPORT BlinkGC final {
  STATIC_ONLY(BlinkGC);

 public:
  // When garbage collecting we need to know whether or not there
  // can be pointers to Blink GC managed objects on the stack for
  // each thread. When threads reach a safe point they record
  // whether or not they have pointers on the stack.
  enum StackState { kNoHeapPointersOnStack, kHeapPointersOnStack };

  enum MarkingType {
    // The marking completes synchronously.
    kAtomicMarking,
    // The marking task is split and executed in chunks (either on the mutator
    // thread or concurrently).
    kIncrementalAndConcurrentMarking
  };

  enum SweepingType {
    // The sweeping task is split into chunks and scheduled lazily and
    // concurrently.
    kConcurrentAndLazySweeping,
    // The sweeping task executes synchronously right after marking.
    kEagerSweeping,
  };

  // Commented out reasons have been used in the past but are not used any
  // longer. We keep them here as the corresponding UMA histograms cannot be
  // changed.
  enum class GCReason {
    // kIdleGC = 0,
    kPreciseGC = 1,
    kConservativeGC = 2,
    kForcedGCForTesting = 3,
    kMemoryPressureGC = 4,
    // kPageNavigationGC = 5,
    kThreadTerminationGC = 6,
    // kTesting = 7,
    // kIncrementalIdleGC = 8,
    kIncrementalV8FollowupGC = 9,
    kUnifiedHeapGC = 10,
    kUnifiedHeapForMemoryReductionGC = 11,
    kMaxValue = kUnifiedHeapForMemoryReductionGC,
  };

#define DeclareArenaIndex(name) k##name##ArenaIndex,
  enum ArenaIndices {
    FOR_EACH_ARENA(DeclareArenaIndex)
    // Values used for iteration of heap segments.
    kNumberOfArenas,
  };
#undef DeclareArenaIndex

  enum V8GCType {
    kV8MinorGC,
    kV8MajorGC,
  };

  // Sentinel used to mark not-fully-constructed during mixins.
  static constexpr void* kNotFullyConstructedObject = nullptr;

  static const char* ToString(GCReason);
  static const char* ToString(MarkingType);
  static const char* ToString(StackState);
  static const char* ToString(SweepingType);
  static const char* ToString(ArenaIndices);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_BLINK_GC_H_
