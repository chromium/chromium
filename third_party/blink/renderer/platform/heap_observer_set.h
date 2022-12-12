// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OBSERVER_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OBSERVER_SET_H_

#include "base/auto_reset.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// A set of observers. Ensures list is not mutated while iterating. Observers
// are not retained.
template <class ObserverType>
class PLATFORM_EXPORT HeapObserverSet {
  DISALLOW_NEW();

 public:
  // Add an observer to this list. An observer should not be added to the same
  // list more than once.
  void AddObserver(ObserverType* observer) {
    CHECK(iteration_state_ & kAllowingAddition);
    DCHECK(!HasObserver(observer));
    observers_.insert(observer);
  }

  // Removes the given observer from this list. Does nothing if this observer is
  // not in this list.
  void RemoveObserver(ObserverType* observer) {
    CHECK(iteration_state_ & kAllowingRemoval);
    observers_.erase(observer);
  }

  // Determine whether a particular observer is in the list.
  bool HasObserver(ObserverType* observer) const {
    DCHECK(!IsIteratingOverObservers());
    return observers_.Contains(observer);
  }

  // Returns true if the list is being iterated over.
  bool IsIteratingOverObservers() const {
    return iteration_state_ != kNotIterating;
  }

  // Removes all the observers from this list.
  void Clear() {
    CHECK(iteration_state_ & kAllowingRemoval);
    observers_.clear();
  }

  // Safely iterate over the registered lifecycle observers in an unpredictable
  // order.
  //
  // Adding or removing observers is not allowed during iteration. The callable
  // will only be called synchronously inside ForEachObserver().
  //
  // Sample usage:
  //     ForEachObserver([](ObserverType* observer) {
  //       observer->SomeMethod();
  //     });
  template <typename ForEachCallable>
  void ForEachObserver(const ForEachCallable& callable) const {
    base::AutoReset<IterationState> scope(&iteration_state_, kAllowingNone);
    for (ObserverType* observer : observers_) {
      callable(observer);
    }
  }

  void Trace(Visitor* visitor) const { visitor->Trace(observers_); }

 private:
  using ObserverSet = HeapHashSet<WeakMember<ObserverType>>;

  // TODO(keishi): Clean up iteration state once transition from
  // LifecycleObserver is complete.
  enum IterationState {
    kAllowingNone = 0,
    kAllowingAddition = 1,
    kAllowingRemoval = 1 << 1,
    kNotIterating = kAllowingAddition | kAllowingRemoval,
  };

  // Iteration state is recorded while iterating the observer set,
  // optionally barring add or remove mutations.
  mutable IterationState iteration_state_ = kNotIterating;
  ObserverSet observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OBSERVER_SET_H_
