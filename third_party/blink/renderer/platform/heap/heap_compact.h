// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_COMPACT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_COMPACT_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

#include <bitset>
#include <utility>

// Compaction-specific debug switches:

// Emit debug info during compaction.
#define DEBUG_HEAP_COMPACTION 0

// Emit stats on freelist occupancy.
// 0 - disabled, 1 - minimal, 2 - verbose.
#define DEBUG_HEAP_FREELIST 0

namespace blink {

class NormalPageArena;
class BasePage;
class ThreadState;
class ThreadHeap;

class PLATFORM_EXPORT HeapCompact final {
 public:
  // Returns |true| if the ongoing GC may compact the given arena/sub-heap.
  static bool IsCompactableArena(int arena_index) {
    return arena_index >= BlinkGC::kVectorArenaIndex &&
           arena_index <= BlinkGC::kHashTableArenaIndex;
  }

  explicit HeapCompact(ThreadHeap*);
  ~HeapCompact();

  // Returns true if compaction can and should be used for the provided
  // parameters.
  bool ShouldCompact(BlinkGC::StackState,
                     BlinkGC::MarkingType,
                     BlinkGC::GCReason);

  // Compaction should be performed as part of the ongoing GC, initialize
  // the heap compaction pass.
  void Initialize(ThreadState*);

  // Returns true if the ongoing GC will perform compaction.
  bool IsCompacting() const { return do_compact_; }

  // Returns true if the ongoing GC will perform compaction for the given
  // heap arena.
  bool IsCompactingArena(int arena_index) const {
    return do_compact_ && (compactable_arenas_ & (0x1u << arena_index));
  }

  // See |Heap::ShouldRegisterMovingAddress()| documentation.
  bool ShouldRegisterMovingAddress();

  // Slots that are not contained within live objects are filtered. This can
  // happen when the write barrier for in-payload objects triggers but the outer
  // backing store does not survive the marking phase because all its referents
  // die before being reached by the marker.
  void FilterNonLiveSlots();

  // Finishes compaction and clears internal state.
  void Finish();

  // Cancels compaction after slots may have been recorded already.
  void Cancel();

  // Perform any relocation post-processing after having completed compacting
  // the given arena. The number of pages that were freed together with the
  // total size (in bytes) of freed heap storage, are passed in as arguments.
  void FinishedArenaCompaction(NormalPageArena*,
                               size_t freed_pages,
                               size_t freed_size);

  // Register the heap page as containing live objects that will all be
  // compacted. Registration happens as part of making the arenas ready
  // for a GC.
  void AddCompactingPage(BasePage*);

  // Notify heap compaction that object at |from| has been relocated to.. |to|.
  // (Called by the sweep compaction pass.)
  void Relocate(Address from, Address to);

  // Enables compaction for the next garbage collection if technically possible.
  void EnableCompactionForNextGCForTesting() { force_for_next_gc_ = true; }

  // Returns true if one or more vector arenas are being compacted.
  bool IsCompactingVectorArenasForTesting() const {
    return IsCompactingArena(BlinkGC::kVectorArenaIndex);
  }

  size_t LastFixupCountForTesting() const {
    return last_fixup_count_for_testing_;
  }

 private:
  class MovableObjectFixups;

  // Freelist size threshold that must be exceeded before compaction
  // should be considered.
  static const size_t kFreeListSizeThreshold = 512 * 1024;

  // Sample the amount of fragmentation and heap memory currently residing
  // on the freelists of the arenas we're able to compact. The computed
  // numbers will be subsequently used to determine if a heap compaction
  // is on order (shouldCompact().)
  void UpdateHeapResidency();

  MovableObjectFixups& Fixups();

  ThreadHeap* const heap_;
  std::unique_ptr<MovableObjectFixups> fixups_;

  // Set to |true| when a compacting sweep will go ahead.
  bool do_compact_ = false;
  size_t gc_count_since_last_compaction_ = 0;

  // Last reported freelist size, across all compactable arenas.
  size_t free_list_size_ = 0;

  // If compacting, i'th heap arena will be compacted if corresponding bit is
  // set. Indexes are in the range of BlinkGC::ArenaIndices.
  unsigned compactable_arenas_ = 0u;

  size_t last_fixup_count_for_testing_ = 0;

  bool force_for_next_gc_ = false;
};

}  // namespace blink

// Logging macros activated by debug switches.

#define LOG_HEAP_COMPACTION_INTERNAL() DLOG(INFO)

#if DEBUG_HEAP_COMPACTION
#define LOG_HEAP_COMPACTION() LOG_HEAP_COMPACTION_INTERNAL()
#else
#define LOG_HEAP_COMPACTION() EAT_STREAM_PARAMETERS
#endif

#if DEBUG_HEAP_FREELIST
#define LOG_HEAP_FREELIST() LOG_HEAP_COMPACTION_INTERNAL()
#else
#define LOG_HEAP_FREELIST() EAT_STREAM_PARAMETERS
#endif

#if DEBUG_HEAP_FREELIST == 2
#define LOG_HEAP_FREELIST_VERBOSE() LOG_HEAP_COMPACTION_INTERNAL()
#else
#define LOG_HEAP_FREELIST_VERBOSE() EAT_STREAM_PARAMETERS
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_COMPACT_H_
