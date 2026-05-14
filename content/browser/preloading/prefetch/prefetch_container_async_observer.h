// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_ASYNC_OBSERVER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_ASYNC_OBSERVER_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/preloading/prefetch/prefetch_container_observer.h"

namespace content {

// Wrapper class of `PrefetchContainerObserver` that notifies the underlying
// `PrefetchContainerObserver` asynchronously, to avoid complicated logic
// executed synchronously from `PrefetchContainer::NotifyObservers()`.
//
// Note: Currently `underlying_observer_` isn't notified of
// `OnWillBeDestroyed()`, to simplify the code and just because
// `OnWillBeDestroyed` is not used by the current use cases.
//
// Be careful of the state changes between the `PrefetchContainerLoadState`
// transitions and asynchronous `PrefetchContainerObserver` method calls.
// Namely,
// - The observer is notified only if the corresponding `PrefetchContainer` is
//   still alive at the time of async call dispatch.
// - Prefer accessing `PrefetchContainer` methods that return the same value
//   after the `PrefetchContainerLoadState` transitions, e.g.
//   `GetInitialEligibility()`.
class PrefetchContainerAsyncObserverBase : public PrefetchContainerObserver {
 public:
  ~PrefetchContainerAsyncObserverBase() override;

  virtual PrefetchContainerObserver& GetUnderlyingObserver() = 0;

  // PrefetchContainerObserver implementations
  void OnWillBeDestroyed(const PrefetchContainer& prefetch_container) override;
  void OnGotInitialEligibility(
      const PrefetchContainer& prefetch_container) override;
  void OnDeterminedHead(const PrefetchContainer& prefetch_container) override;
  void OnPrefetchCompletedOrFailed(
      const PrefetchContainer& prefetch_container) override;

 protected:
  PrefetchContainerAsyncObserverBase();

 private:
  void Dispatch(
      void (PrefetchContainerObserver::*method)(const PrefetchContainer&),
      base::WeakPtr<const PrefetchContainer> prefetch_container);

  base::WeakPtrFactory<PrefetchContainerAsyncObserverBase> weak_factory_{this};
};

template <typename UnderlyingObserver = PrefetchContainerObserver>
class PrefetchContainerAsyncObserver final
    : public PrefetchContainerAsyncObserverBase {
 public:
  // Returns null if the `underlying_observer` is null.
  static std::unique_ptr<PrefetchContainerAsyncObserver> Create(
      std::unique_ptr<UnderlyingObserver> underlying_observer) {
    if (!underlying_observer) {
      return nullptr;
    }
    return base::WrapUnique(
        new PrefetchContainerAsyncObserver(std::move(underlying_observer)));
  }

  UnderlyingObserver& GetUnderlyingObserver() override {
    return *underlying_observer_;
  }

 private:
  explicit PrefetchContainerAsyncObserver(
      std::unique_ptr<UnderlyingObserver> underlying_observer)
      : underlying_observer_(std::move(underlying_observer)) {
    CHECK(underlying_observer_);
  }

  std::unique_ptr<UnderlyingObserver> underlying_observer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_ASYNC_OBSERVER_H_
