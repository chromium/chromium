// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OBSERVER_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OBSERVER_LIST_H_

#include <algorithm>

#include "base/auto_reset.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// A list of observers. Ensures that the list is not mutated while iterating.
// Observers are not retained by the list. The implementation favors performance
// of iteration over modifications via add and remove.
template <class ObserverType>
class PLATFORM_EXPORT HeapObserverList final {
  DISALLOW_NEW();

 public:
  // Adds an observer to this list. An observer must not be added to the same
  // list more than once. Adding an observer is O(n) but amortized constant.
  void AddObserver(ObserverType* observer) {
    CHECK(mutation_state_ & kAllowAddition);
    DCHECK(!HasObserver(observer));
    DCHECK_EQ(observers_to_indices_.size(), observers_.size());
    // `push_back()` may trigger GC which results in `observers_to_indices_`
    // missing the to-be-added observer. This is okay in general as `observer`
    // is alive and in particular for `CleanupDeadObservers()` which is aware of
    // this fact.
    observers_.push_back(observer);
    observers_to_indices_.insert(observer, observers_.size() - 1);
    if (check_capacity_) [[unlikely]] {
      // On first addition after GC, check whether we need to shrink to a
      // reasoanble capacity.
      observers_.ShrinkToReasonableCapacity();
      // observers_to_indices_ did already shrink if necessary due to insert().
      check_capacity_ = false;
    }
  }

  // Removes the given observer from this list. Does nothing if this observer is
  // not in this list. Removing an observer is O(n) but amortized constant.
  void RemoveObserver(ObserverType* observer) {
    CHECK(mutation_state_ & kAllowRemoval);
    DCHECK_EQ(observers_to_indices_.size(), observers_.size());

    const auto it = observers_to_indices_.find(observer);
    if (it == observers_to_indices_.end()) [[unlikely]] {
      return;
    }
    const wtf_size_t index = it->value;
    const wtf_size_t last_observer_index = observers_.size() - 1;
    CHECK_LT(index, observers_.size());
    if (index != last_observer_index) {
      auto* last_observer = observers_[last_observer_index].Get();
      observers_[index] = last_observer;
      observers_to_indices_.find(last_observer)->value = index;
    }
    // `pop_back()` cannot invoke GC.
    observers_.pop_back();

    // `erase()` may trigger GC which is not a problem at this point as
    // `observers_` is already clean. It may just mean that more observer may be
    // removed from the data structure.
    observers_to_indices_.erase(it);

    observers_.ShrinkToReasonableCapacity();
    // observers_to_indices_ did already shrink if necessary due to erase().
    check_capacity_ = false;
  }

  // Determine whether a particular observer is in the list. Checking
  // containment is O(1).
  bool HasObserver(ObserverType* observer) const {
    DCHECK(!IsIteratingOverObservers());
    return observers_to_indices_.Contains(observer);
  }

  // Returns true if the list is being iterated over.
  bool IsIteratingOverObservers() const { return mutation_state_ != kAllowAll; }

  // Removes all the observers from this list.
  void Clear() {
    CHECK(mutation_state_ & kAllowRemoval);
    observers_.clear();
    observers_to_indices_.clear();
  }

  // Iterate over the registered lifecycle observers in an unpredictable order.
  //
  // Adding or removing observers is not allowed during iteration. The callable
  // will be called synchronously inside `ForEachObserver()`.
  //
  // Sample usage:
  // ```
  // ForEachObserver([](ObserverType* observer) {
  //   observer->SomeMethod();
  // });
  // ```
  template <typename ForEachCallable>
  void ForEachObserver(const ForEachCallable& callable) const {
    base::AutoReset<MutationState> scope(&mutation_state_, kNoMutationAllowed);
    for (ObserverType* observer : observers_) {
      // See CleanupDeadObservers() for why we can receive nullptr here.
      if (observer) {
        callable(observer);
      }
    }
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(observers_);
    visitor->RegisterWeakCallbackMethod<
        HeapObserverList, &HeapObserverList::CleanupDeadObservers>(this);
    visitor->Trace(observers_to_indices_);
  }

 private:
  void CleanupDeadObservers(const LivenessBroker& broker) {
    // This method must not allocate.

    // The GC currently does not strongify UntracedMember, even during
    // iteration. This implies that we must not move around items as using the
    // erase/remove_if pattern may move a valid object in a slot where we just
    // retrieved a nullptr.
    if (mutation_state_ == kNoMutationAllowed) {
      for (auto& observer : observers_) {
        if (!broker.IsHeapObjectAlive(observer)) {
          observer.Clear();
        }
      }
    } else {
      // We are not iterating, so we we can prepare the backing store by moving
      // dead slots to the end. Backing store reallocations will happen during
      // addition and removal.
      observers_.erase(
          std::remove_if(observers_.begin(), observers_.end(),
                         [broker](auto& observer) {
                           // If there's no iteration allowed we just `Clear()`
                           // the entry which writes nullptr.
                           // `IsHeapObjectAlive()` then treats nullptr as live
                           // object for other reasons. Consequently, we require
                           // an explicit nullptr check here.
                           return !observer ||
                                  !broker.IsHeapObjectAlive(observer);
                         }),
          observers_.end());
      // We also need to fix up the indices map. This works because we only
      // query and update live indices.
      for (wtf_size_t i = 0; i < observers_.size(); ++i) {
        // The callback here may be executed while `observers_to_indices_`
        // misses out on the observer that is either added or removed.
        const auto it = observers_to_indices_.find(observers_[i]);
        if (it != observers_to_indices_.end()) [[likely]] {
          it->value = i;
        }
      }
      check_capacity_ = true;
    }
  }

  // TODO(keishi): Clean up iteration state once transition from
  // LifecycleObserver is complete.
  enum MutationState {
    kNoMutationAllowed = 0,
    kAllowAddition = 1,
    kAllowRemoval = 1 << 1,
    kAllowAll = kAllowAddition | kAllowRemoval,
  };

  // MutationState records whether mutations are allowed to the set of
  // observers. Iteration e.g. prohibits any mutations.
  mutable MutationState mutation_state_ = kAllowAll;
  bool check_capacity_ = false;
  // Compact list of observers used for iteration. HeapVector doesn't allow
  // WeakMember at this point. Instead, use UntracedMember with a custom weak
  // callback.
  HeapVector<UntracedMember<ObserverType>> observers_;
  // Mapping observers to indices in `observers_`. Observers may be missing in
  // here temporarily as modification treats `observers_` as source of truth.
  HeapHashMap<WeakMember<ObserverType>, wtf_size_t> observers_to_indices_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OBSERVER_LIST_H_
