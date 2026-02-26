// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_ASSERT_PREFETCH_CONTAINER_OBSERVER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_ASSERT_PREFETCH_CONTAINER_OBSERVER_H_

#include "content/browser/preloading/prefetch/prefetch_container.h"

namespace content {

// A `PrefetchContainer::Observer` that verifies the observer is called at the
// right sequence and with aligned `PrefetchContainerLoadState`.
class AssertPrefetchContainerObserver final
    : public PrefetchContainer::Observer {
 public:
  // This should be constructed upon `PrefetchContainer` construction.
  explicit AssertPrefetchContainerObserver(
      PrefetchContainer& prefetch_container);
  ~AssertPrefetchContainerObserver() override;

 private:
  // PrefetchContainer::Observer overrides:
  void OnWillBeDestroyed(const PrefetchContainer& prefetch_container) override;
  void OnGotInitialEligibility(const PrefetchContainer& prefetch_container,
                               PreloadingEligibility eligibility) override;
  void OnDeterminedHead(const PrefetchContainer& prefetch_container) override;
  void OnPrefetchCompletedOrFailed(
      const PrefetchContainer& prefetch_container,
      const network::URLLoaderCompletionStatus& completion_status,
      const std::optional<int>& response_code) override;

  void UpdateObservedLoadState();

  // `base::WeakPtr` is used here (instead of `raw_ref`) to test the
  // `base::WeakPtr` behavior, especially to check that it isn't invalidated at
  // the time of `OnWillBeDestroyed()`.
  base::WeakPtr<PrefetchContainer> prefetch_container_;

  // The `PrefetchContainerLoadState` observed at the last
  // `PrefetchContainer::Observer` callback.
  PrefetchContainerLoadState observed_load_state_ =
      PrefetchContainerLoadState::kNotStarted;

  // Currently `AssertPrefetchContainerObserver` doesn't outlive
  // `PrefetchContainer` so this doesn't check notifications AFTER
  // `PrefetchContainer` is completely destroyed.
  // Still this flag is used to check notifications DURING `PrefetchContainer`
  // dtor, as there can be a bunch of things executed between
  // `OnWillBeDestroyed()` and the end of `PrefetchContainer` dtor.
  bool on_will_be_destroyed_called_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_ASSERT_PREFETCH_CONTAINER_OBSERVER_H_
