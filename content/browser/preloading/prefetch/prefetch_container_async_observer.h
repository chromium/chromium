// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_ASYNC_OBSERVER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_ASYNC_OBSERVER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/preloading/prefetch/prefetch_container_observer.h"

namespace content {

// Wrapper class of `PrefetchContainerObserver` that notifies the underlying
// `PrefetchContainerObserver` asynchronously, to avoid complicated logic
// executed synchronously from `PrefetchContainer::NotifyObservers()`.
//
// Note: Currently `observer_` isn't notified of `OnWillBeDestroyed()`, to
// simplify the code and just because `OnWillBeDestroyed` is not used by the
// current use cases.
//
// Be careful of the state changes between the `PrefetchContainerLoadState`
// transitions and asynchronous `PrefetchContainerObserver` method calls.
// Namely,
// - The observer is notified only if the corresponding `PrefetchContainer` is
//   still alive at the time of async call dispatch.
// - Prefer accessing `PrefetchContainer` methods that return the same value
//   after the `PrefetchContainerLoadState` transitions, e.g.
//   `GetInitialEligibility()`.
class PrefetchContainerAsyncObserver final : public PrefetchContainerObserver {
 public:
  // Returns null if the underlying `observer` is null.
  static std::unique_ptr<PrefetchContainerAsyncObserver> Create(
      std::unique_ptr<PrefetchContainerObserver> observer);

  ~PrefetchContainerAsyncObserver() override;

  // PrefetchContainerObserver implementations
  void OnWillBeDestroyed(const PrefetchContainer& prefetch_container) override;
  void OnGotInitialEligibility(
      const PrefetchContainer& prefetch_container) override;
  void OnDeterminedHead(const PrefetchContainer& prefetch_container) override;
  void OnPrefetchCompletedOrFailed(
      const PrefetchContainer& prefetch_container) override;

 private:
  explicit PrefetchContainerAsyncObserver(
      std::unique_ptr<PrefetchContainerObserver> observer);

  void Dispatch(
      void (PrefetchContainerObserver::*method)(const PrefetchContainer&),
      base::WeakPtr<const PrefetchContainer> prefetch_container);

  std::unique_ptr<PrefetchContainerObserver> observer_;

  base::WeakPtrFactory<PrefetchContainerAsyncObserver> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_ASYNC_OBSERVER_H_
