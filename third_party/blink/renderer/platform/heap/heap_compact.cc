// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_compact.h"

#include <memory>

#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

// The real worker behind heap compaction, recording references to movable
// objects ("slots".) When the objects end up being compacted and moved,
// relocate() will adjust the slots to point to the new location of the
// object along with handling fixups for interior pointers.
//
// The "fixups" object is created and maintained for the lifetime of one
// heap compaction-enhanced GC.
class HeapCompact::MovableObjectFixups final {
  USING_FAST_MALLOC(HeapCompact::MovableObjectFixups);

 public:
  explicit MovableObjectFixups(ThreadHeap* heap) : heap_(heap) {}
  ~MovableObjectFixups() = default;

  // For the arenas being compacted, record all pages belonging to them.
  // This is needed to handle interior pointers that reside on areas that are
  // compacted themselves.
  void AddCompactingPage(BasePage* page);

  // Adds a slot for compaction. Filters slots in dead objects.
  void AddOrFilter(MovableReference* slot);

  // Relocates a backing store |from| -> |to|.
  void Relocate(Address from, Address to);

  // Relocates interior slots in a backing store that is moved |from| -> |to|.
  void RelocateInteriorFixups(Address from, Address to, size_t size);

  // Updates the collection of callbacks from the item pushed the worklist by
  // marking visitors.
  void UpdateCallbacks();

#if DEBUG_HEAP_COMPACTION
  void dumpDebugStats() {
    LOG_HEAP_COMPACTION() << "Fixups: pages=" << relocatable_pages_.size()
                          << " objects=" << fixups_.size()
                          << " callbacks=" << fixup_callbacks_.size()
                          << " interior-size=" << interior_fixups_.size();
  }
#endif

 private:
  void VerifyUpdatedSlot(MovableReference* slot);

  ThreadHeap* const heap_;

  // Map from movable reference (value) to its slots. Upon moving an object its
  // slot pointing to it requires updating.
  HashMap<MovableReference, MovableReference*> fixups_;

  // Map from movable regions to callbacks that need to be invoked
  // when the region moves.
  HashMap<MovableReference, MovingObjectCallback> fixup_callbacks_;

  // Map of interior slots to their final location. Needs to be an ordered map
  // as it is used to walk through slots starting at a given memory address.
  // Requires log(n) lookup to make the early bailout reasonably fast. Currently
  // only std::map fullfills those requirements.
  //
  // - The initial value for a given key is nullptr.
  // - Upon moving a an object this value is adjusted accordingly.
  std::map<MovableReference*, Address> interior_fixups_;

  // All pages that are being compacted. The set keeps references to
  // BasePage instances. The void* type was selected to allow to check
  // arbitrary addresses.
  HashSet<void*> relocatable_pages_;

#if DCHECK_IS_ON()
  // The following two collections are used to allow refer back from a slot to
  // an already moved object.
  HashSet<void*> moved_objects_;
  HashMap<MovableReference*, MovableReference> interior_slot_to_object_;
#endif  // DCHECK_IS_ON()
};

void HeapCompact::MovableObjectFixups::AddCompactingPage(BasePage* page) {
  DCHECK(!page->IsLargeObjectPage());
  relocatable_pages_.insert(page);
}

void HeapCompact::MovableObjectFixups::AddOrFilter(MovableReference* slot) {
  MovableReference value = *slot;
  CHECK(value);

  // All slots and values are part of Oilpan's heap.
  // - Slots may be contained within dead objects if e.g. the write barrier
  //   registered the slot while the out backing itself has not been marked
  //   live in time. Slots in dead objects are filtered below.
  // - Values may only be contained in or point to live objects.

  // Slots handling.
  BasePage* const slot_page =
      heap_->LookupPageForAddress(reinterpret_cast<Address>(slot));
  CHECK(slot_page);
  HeapObjectHeader* const header =
      slot_page->IsLargeObjectPage()
          ? static_cast<LargeObjectPage*>(slot_page)->ObjectHeader()
          : static_cast<NormalPage*>(slot_page)->FindHeaderFromAddress(
                reinterpret_cast<Address>(slot));
  CHECK(header);
  // Filter the slot since the object that contains the slot is dead.
  if (!header->IsMarked())
    return;

  // Value handling.
  BasePage* const value_page =
      heap_->LookupPageForAddress(reinterpret_cast<Address>(value));
  CHECK(value_page);

  // The following cases are not compacted and do not require recording:
  // - Backings in large pages.
  // - Inline backings that are part of a non-backing arena.
  if (value_page->IsLargeObjectPage() ||
      !HeapCompact::IsCompactableArena(value_page->Arena()->ArenaIndex()))
    return;

  // Slots must reside in and values must point to live objects at this
  // point, with the exception of slots in eagerly swept arenas where objects
  // have already been processed. |value| usually points to a separate
  // backing store but can also point to inlined storage which is why the
  // dynamic header lookup is required.
  HeapObjectHeader* const value_header =
      static_cast<NormalPage*>(value_page)
          ->FindHeaderFromAddress(reinterpret_cast<Address>(value));
  CHECK(value_header);
  CHECK(value_header->IsMarked());

  // Slots may have been recorded already but must point to the same
  // value. Example: Ephemeron iterations may register slots multiple
  // times.
  auto fixup_it = fixups_.find(value);
  if (UNLIKELY(fixup_it != fixups_.end())) {
    CHECK_EQ(slot, fixup_it->value);
    return;
  }

  // Add regular fixup.
  fixups_.insert(value, slot);

  // Check whether the slot itself resides on a page that is compacted.
  if (LIKELY(!relocatable_pages_.Contains(slot_page)))
    return;

  auto interior_it = interior_fixups_.find(slot);
  CHECK(interior_fixups_.end() == interior_it);
  interior_fixups_.emplace(slot, nullptr);
#if DCHECK_IS_ON()
  interior_slot_to_object_.insert(slot, header->Payload());
#endif  // DCHECK_IS_ON()
  LOG_HEAP_COMPACTION() << "Interior slot: " << slot;
}

void HeapCompact::MovableObjectFixups::Relocate(Address from, Address to) {
#if DCHECK_IS_ON()
    moved_objects_.insert(from);
#endif  // DCHECK_IS_ON()

    const HeapObjectHeader* header = HeapObjectHeader::FromPayload(to);
    const size_t size = header->PayloadSize();

    // Interior slots always need to be processed for moved objects.
    // Consider an object A with slot A.x pointing to value B where A is
    // allocated on a movable page itself. When B is finally moved, it needs to
    // find the corresponding slot A.x. Object A may be moved already and the
    // memory may have been freed, which would result in a crash.
    if (!interior_fixups_.empty()) {
      RelocateInteriorFixups(from, to, size);
    }

    // Execute custom callback after interior fixups have been processed.
    auto callback = fixup_callbacks_.find(from);
    if (UNLIKELY(callback != fixup_callbacks_.end())) {
      callback->value(from, to, size);
    }

    auto it = fixups_.find(from);
    // This means that there is no corresponding slot for a live backing store.
    // This may happen because a mutator may change the slot to point to a
    // different backing store because e.g. incremental marking marked a backing
    // store as live that was later on replaced.
    if (it == fixups_.end()) {
      return;
    }

#if DCHECK_IS_ON()
    BasePage* from_page = PageFromObject(from);
    DCHECK(relocatable_pages_.Contains(from_page));
#endif

    // If the object is referenced by a slot that is contained on a compacted
    // area itself, check whether it can be updated already.
    MovableReference* slot = it->value;
    auto interior_it = interior_fixups_.find(slot);
    if (interior_it != interior_fixups_.end()) {
      MovableReference* slot_location =
          reinterpret_cast<MovableReference*>(interior_it->second);
      if (!slot_location) {
        interior_it->second = to;
#if DCHECK_IS_ON()
        // Check that the containing object has not been moved yet.
        auto reverse_it = interior_slot_to_object_.find(slot);
        DCHECK(interior_slot_to_object_.end() != reverse_it);
        DCHECK(moved_objects_.end() == moved_objects_.find(reverse_it->value));
#endif  // DCHECK_IS_ON()
      } else {
        LOG_HEAP_COMPACTION()
            << "Redirected slot: " << slot << " => " << slot_location;
        slot = slot_location;
      }
    }

    // If the slot has subsequently been updated, e.g. a destructor having
    // mutated and expanded/shrunk the collection, do not update and relocate
    // the slot -- |from| is no longer valid and referenced.
    if (UNLIKELY(*slot != from)) {
      LOG_HEAP_COMPACTION()
          << "No relocation: slot = " << slot << ", *slot = " << *slot
          << ", from = " << from << ", to = " << to;
      VerifyUpdatedSlot(slot);
      return;
    }

    // Update the slots new value.
    *slot = to;
}

void HeapCompact::MovableObjectFixups::RelocateInteriorFixups(Address from,
                                                              Address to,
                                                              size_t size) {
  // |from| is a valid address for a slot.
  auto interior_it =
      interior_fixups_.lower_bound(reinterpret_cast<MovableReference*>(from));
  if (interior_it == interior_fixups_.end())
    return;

  CHECK_GE(reinterpret_cast<Address>(interior_it->first), from);
  size_t offset = reinterpret_cast<Address>(interior_it->first) - from;
  while (offset < size) {
    if (!interior_it->second) {
      // Update the interior fixup value, so that when the object the slot is
      // pointing to is moved, it can re-use this value.
      Address fixup = to + offset;
      interior_it->second = fixup;

      // If the |slot|'s content is pointing into the region [from, from +
      // size) we are dealing with an interior pointer that does not point to
      // a valid HeapObjectHeader. Such references need to be fixed up
      // immediately.
      Address fixup_contents = *reinterpret_cast<Address*>(fixup);
      if (fixup_contents > from && fixup_contents < (from + size)) {
        *reinterpret_cast<Address*>(fixup) = fixup_contents - from + to;
      }
    }

    interior_it++;
    if (interior_it == interior_fixups_.end())
      return;
    offset = reinterpret_cast<Address>(interior_it->first) - from;
  }
}

void HeapCompact::MovableObjectFixups::UpdateCallbacks() {
  BackingStoreCallbackWorklist::View backing_store_callbacks(
      heap_->GetBackingStoreCallbackWorklist(), WorklistTaskId::MutatorThread);
  BackingStoreCallbackItem item;
  while (backing_store_callbacks.Pop(&item)) {
    fixup_callbacks_.insert(item.backing, item.callback);
  }
}

void HeapCompact::MovableObjectFixups::VerifyUpdatedSlot(
    MovableReference* slot) {
// Verify that the already updated slot is valid, meaning:
//  - has been cleared.
//  - has been updated & expanded with a large object backing store.
//  - has been updated with a larger, freshly allocated backing store.
//    (on a fresh page in a compactable arena that is not being
//    compacted.)
#if DCHECK_IS_ON()
  if (!*slot)
    return;
  BasePage* slot_page =
      heap_->LookupPageForAddress(reinterpret_cast<Address>(*slot));
  // ref_page is null if *slot is pointing to an off-heap region. This may
  // happy if *slot is pointing to an inline buffer of HeapVector with
  // inline capacity.
  if (!slot_page)
    return;
  DCHECK(slot_page->IsLargeObjectPage() ||
         (HeapCompact::IsCompactableArena(slot_page->Arena()->ArenaIndex()) &&
          !relocatable_pages_.Contains(slot_page)));
#endif  // DCHECK_IS_ON()
}

HeapCompact::HeapCompact(ThreadHeap* heap) : heap_(heap) {
  // The heap compaction implementation assumes the contiguous range,
  //
  //   [VectorArenaIndex, HashTableArenaIndex]
  //
  // in a few places. Use static asserts here to not have that assumption
  // be silently invalidated by ArenaIndices changes.
  static_assert(BlinkGC::kVectorArenaIndex + 1 == BlinkGC::kHashTableArenaIndex,
                "unexpected ArenaIndices ordering");
}

HeapCompact::~HeapCompact() = default;

HeapCompact::MovableObjectFixups& HeapCompact::Fixups() {
  if (!fixups_)
    fixups_ = std::make_unique<MovableObjectFixups>(heap_);
  return *fixups_;
}

bool HeapCompact::ShouldCompact(BlinkGC::StackState stack_state,
                                BlinkGC::MarkingType marking_type,
                                BlinkGC::GCReason reason) {
  if (marking_type == BlinkGC::MarkingType::kAtomicMarking &&
      stack_state == BlinkGC::StackState::kHeapPointersOnStack) {
    // The following check ensures that tests that want to test compaction are
    // not interrupted by garbage collections that cannot use compaction.
    CHECK(!force_for_next_gc_);
    return false;
  }

  UpdateHeapResidency();

  if (force_for_next_gc_) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(blink::features::kBlinkHeapCompaction)) {
    return false;
  }

  // Only enable compaction when in a memory reduction garbage collection as it
  // may significantly increase the final garbage collection pause.
  if (reason == BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC) {
    return free_list_size_ > kFreeListSizeThreshold;
  }

  return false;
}

void HeapCompact::Initialize(ThreadState* state) {
  CHECK(force_for_next_gc_ ||
        base::FeatureList::IsEnabled(blink::features::kBlinkHeapCompaction));
  CHECK(!do_compact_);
  CHECK(!fixups_);
  LOG_HEAP_COMPACTION() << "Compacting: free=" << free_list_size_;
  do_compact_ = true;
  gc_count_since_last_compaction_ = 0;
  force_for_next_gc_ = false;
}

bool HeapCompact::ShouldRegisterMovingAddress(Address address) {
  CHECK(heap_->LookupPageForAddress(reinterpret_cast<Address>(address)));

  return do_compact_;
}

void HeapCompact::UpdateHeapResidency() {
  size_t total_arena_size = 0;
  size_t total_free_list_size = 0;

  compactable_arenas_ = 0;
#if DEBUG_HEAP_FREELIST
  std::stringstream stream;
#endif
  for (int i = BlinkGC::kVectorArenaIndex; i <= BlinkGC::kHashTableArenaIndex;
       ++i) {
    NormalPageArena* arena = static_cast<NormalPageArena*>(heap_->Arena(i));
    size_t arena_size = arena->ArenaSize();
    size_t free_list_size = arena->FreeListSize();
    total_arena_size += arena_size;
    total_free_list_size += free_list_size;
#if DEBUG_HEAP_FREELIST
    stream << i << ": [" << arena_size << ", " << free_list_size << "], ";
#endif
    // TODO: be more discriminating and consider arena
    // load factor, effectiveness of past compactions etc.
    if (!arena_size)
      continue;
    // Mark the arena as compactable.
    compactable_arenas_ |= 0x1u << i;
  }
#if DEBUG_HEAP_FREELIST
  LOG_HEAP_FREELIST() << "Arena residencies: {" << stream.str() << "}";
  LOG_HEAP_FREELIST() << "Total = " << total_arena_size
                      << ", Free = " << total_free_list_size;
#endif

  // TODO(sof): consider smoothing the reported sizes.
  free_list_size_ = total_free_list_size;
}

void HeapCompact::FinishedArenaCompaction(NormalPageArena* arena,
                                          size_t freed_pages,
                                          size_t freed_size) {
  if (!do_compact_)
    return;

  heap_->stats_collector()->IncreaseCompactionFreedPages(freed_pages);
  heap_->stats_collector()->IncreaseCompactionFreedSize(freed_size);
}

void HeapCompact::Relocate(Address from, Address to) {
  Fixups().Relocate(from, to);
}

void HeapCompact::UpdateBackingStoreCallbacks() {
  if (!do_compact_)
    return;

  Fixups().UpdateCallbacks();
}

void HeapCompact::FilterNonLiveSlots() {
  if (!do_compact_)
    return;

  last_fixup_count_for_testing_ = 0;
  MovableReferenceWorklist::View traced_slots(
      heap_->GetMovableReferenceWorklist(), WorklistTaskId::MutatorThread);
  MovableReference* slot;
  while (traced_slots.Pop(&slot)) {
    if (*slot) {
      Fixups().AddOrFilter(slot);
      last_fixup_count_for_testing_++;
    }
  }
}

void HeapCompact::Finish() {
  if (!do_compact_)
    return;

#if DEBUG_HEAP_COMPACTION
  if (fixups_)
    fixups_->dumpDebugStats();
#endif
  do_compact_ = false;
  fixups_.reset();
}

void HeapCompact::Cancel() {
  if (!do_compact_)
    return;

  last_fixup_count_for_testing_ = 0;
  do_compact_ = false;
  heap_->GetMovableReferenceWorklist()->Clear();
  heap_->GetBackingStoreCallbackWorklist()->Clear();
  fixups_.reset();
}

void HeapCompact::AddCompactingPage(BasePage* page) {
  DCHECK(do_compact_);
  DCHECK(IsCompactingArena(page->Arena()->ArenaIndex()));
  Fixups().AddCompactingPage(page);
}

}  // namespace blink
