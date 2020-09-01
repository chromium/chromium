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

class LivenessBroker;
class MarkingVisitor;
class Visitor;

using Address = uint8_t*;
using ConstAddress = const uint8_t*;

using VisitorCallback = void (*)(Visitor*, const void*);
using MarkingVisitorCallback = void (*)(MarkingVisitor*, const void*);
using TraceCallback = VisitorCallback;
using WeakCallback = void (*)(const LivenessBroker&, const void*);
using EphemeronCallback = VisitorCallback;

// Simple alias to avoid heap compaction type signatures turning into
// a sea of generic |void*|s.
using MovableReference = const void*;

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
  // CollectionType represents generational collection. kMinor collects objects
  // in the young generation (i.e. allocated since the previous collection
  // cycle, since we use sticky bits), kMajor collects the entire heap.
  enum class CollectionType { kMinor, kMajor };

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
    // kIdleGC = 0
    // kPreciseGC = 1
    // kConservativeGC = 2
    kForcedGCForTesting = 3,
    // kMemoryPressureGC = 4
    // kPageNavigationGC = 5
    kThreadTerminationGC = 6,
    // kTesting = 7
    // kIncrementalIdleGC = 8
    // kIncrementalV8FollowupGC = 9
    kUnifiedHeapGC = 10,
    kUnifiedHeapForMemoryReductionGC = 11,
    kUnifiedHeapForcedForTestingGC = 12,
    // Used by UMA_HISTOGRAM_ENUMERATION macro.
    kMaxValue = kUnifiedHeapForcedForTestingGC,
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

  static const char* ToString(GCReason);
  static const char* ToString(MarkingType);
  static const char* ToString(StackState);
  static const char* ToString(SweepingType);
  static const char* ToString(ArenaIndices);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_BLINK_GC_H_
