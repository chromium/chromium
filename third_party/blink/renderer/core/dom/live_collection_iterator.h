// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_LIVE_COLLECTION_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_LIVE_COLLECTION_ITERATOR_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// LiveCollectionIterator<EntryType> provides live-iteration cursor logic over
// a HeapLinkedHashSet<Member<EntryType>> for collections that follow JS
// Set/Map semantics:
//
//  - Paused iterators see items added after the current position.
//  - Exhausted iterators (AdvanceAndGetNext returned nullptr/false) stay
//    permanently done.
//  - clear() resets the cursor but does NOT mark as exhausted; if items are
//    added before the next advance, the iterator sees them.
//  - delete() of the last-returned item moves the cursor to its predecessor.
template <typename EntryType>
class LiveCollectionIterator : public GarbageCollectedMixin {
  using SetType = HeapLinkedHashSet<Member<EntryType>>;

 public:
  // Advances the cursor past the last returned entry and returns the next
  // entry, or nullptr if the iterator is exhausted. On exhaustion, sets
  // finished_ and removes this iterator from |active_iterators|.
  EntryType* AdvanceAndGetNext(
      const SetType& collection,
      HeapHashSet<WeakMember<LiveCollectionIterator>>& active_iterators) {
    if (finished_) {
      return nullptr;
    }

    typename SetType::const_iterator it;
    if (!last_returned_) {
      it = collection.begin();
    } else {
      it = collection.find(last_returned_);
      CHECK(it != collection.end());
      ++it;
    }

    if (it == collection.end()) {
      finished_ = true;
      active_iterators.erase(this);
      return nullptr;
    }

    last_returned_ = *it;
    return last_returned_.Get();
  }

  // Called before |entry| is removed from the collection. If |entry| is the
  // last-returned item, moves the cursor to its predecessor so the next
  // AdvanceAndGetNext() steps past the removed entry correctly.
  void WillRemoveEntry(const EntryType* entry, const SetType& collection) {
    if (last_returned_.Get() == entry) {
      auto it = collection.find(last_returned_);
      CHECK(it != collection.end());
      if (it == collection.begin()) {
        last_returned_ = nullptr;
      } else {
        --it;
        last_returned_ = *it;
      }
    }
  }

  // Called before the collection is cleared. Resets the cursor without marking
  // as exhausted, so items added after clear() are still visible.
  void WillClear() { last_returned_ = nullptr; }

  void Trace(blink::Visitor* visitor) const override {
    visitor->Trace(last_returned_);
  }

 private:
  Member<EntryType> last_returned_;
  bool finished_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_LIVE_COLLECTION_ITERATOR_H_
