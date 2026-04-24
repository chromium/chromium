// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_OBSERVER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace content {

class PrefetchContainer;

// Observer interface to listen to lifecycle events of `PrefetchContainer`.
//
// ----------------------------------------------------------------
// Callback timing: Each callback
// - Is called synchronously and immediately AFTER the `PrefetchContainer`
//   transitioned to a corresponding state. At the time of the callback, all
//   relevant state changes on `PrefetchContainer` should be already done.
// - Is called at most once in the lifetime of a `PrefetchContainer`.
// - Isn't called during `PrefetchContainer` dtor, except for
//   `OnWillBeDestroyed()`.
// - Isn't called if the observer is added after reaching the corresponding
//   `PrefetchContainerLoadState`, including when added during the
//   `PrefetchContainerObserver` callback to another
//   `PrefetchContainerObserver`.
//
// Verified by: `AssertPrefetchContainerObserver` and
// `PrefetchContainerTest.ObserverAddedDuringNotification`.
//
// ----------------------------------------------------------------
// Allowed operations during `PrefetchContainerObserver` calls:
// - Accessing/creating `WeakPtr<PrefetchContainer>`. Observers can assume
//   `WeakPtr`s are not invalidated yet, even in `OnWillBeDestroyed()`.
// - Calling `PrefetchContainer::Add/RemoveObserver()`.
//   See the `base::ObserverList` semantics.
// - Posting tasks and other simple operations.
//
// ----------------------------------------------------------------
// Disallowed operations during `PrefetchContainerObserver` calls:
// - Don't trigger another `PrefetchContainerLoadState` state transitions,
//   because this would complicate the state management due to reentrancy.
//   - Don't call `PrefetchService::ResetPrefetchContainer()`.
//   - Don't destroy `PrefetchContainer`s.
//   - Don't cancel prefetching.
//   - Don't start a new prefetch.
// - Don't trigger logic that are complicated or not controlled by prefetch
//   stack. Namely:
//   - Don't unblock navigation.
//   - Don't trigger arbitrary external callbacks.
//   Because the `PrefetchContainerObserver` calls can be made during
//   complicated or uncontrolled-by-prefetch logic (e.g. navigation commit), we
//   should assume calling complicated or uncontrolled-by-prefetch logic from
//   `PrefetchContainerObserver`s can potentially cause reentrancy to prefetch
//   and navigation logic, which should be avoided.
// Verified by: `PrefetchContainer::during_observer_notification_`.
// The remaining known violations are:
// - TODO(crbug.com/404416345): `PrefetchMatchResolver` can unblock a
//   navigation synchronously.
// - TODO(crbug.com/480271813): `PrefetchContainerObserver` notifies callbacks
//   that can be set by the content public API.
class PrefetchContainerObserver : public base::CheckedObserver {
 public:
  // State: the `PrefetchContainer` is about to be destroyed, called at the
  // head of dtor.
  // No other `PrefetchContainerObserver` calls are made after
  // `OnWillBeDestroyed()`.
  // TODO(crbug.com/356314759): Call this just before dtor is called.
  virtual void OnWillBeDestroyed(
      const PrefetchContainer& prefetch_container) = 0;

  // State: `PrefetchContainerLoadState::kEligible` or
  // `PrefetchContainerLoadState::kFailedIneligible`.
  virtual void OnGotInitialEligibility(
      const PrefetchContainer& prefetch_container) = 0;

  // State: `PrefetchContainerLoadState::kDeterminedHead` or
  // `PrefetchContainerLoadState::kFailedDeterminedHead`.
  virtual void OnDeterminedHead(
      const PrefetchContainer& prefetch_container) = 0;

  // State: `PrefetchContainerLoadState::kCompleted` or
  // `PrefetchContainerLoadState::kFailed`.
  virtual void OnPrefetchCompletedOrFailed(
      const PrefetchContainer& prefetch_container) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_OBSERVER_H_
