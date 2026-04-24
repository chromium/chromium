// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_OBSERVER_FOR_PREFETCH_REQUEST_STATUS_LISTENER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_OBSERVER_FOR_PREFETCH_REQUEST_STATUS_LISTENER_H_

#include "content/browser/preloading/prefetch/prefetch_container_observer.h"
#include "content/public/browser/prefetch_request_status_listener.h"

namespace content {

// Wraps and notifies `PrefetchRequestStatusListener` via
// `PrefetchContainerObserver`.
class PrefetchContainerObserverForPrefetchRequestStatusListener final
    : public PrefetchContainerObserver {
 public:
  // Returns null if `listener` is null.
  static std::unique_ptr<
      PrefetchContainerObserverForPrefetchRequestStatusListener>
  Create(std::unique_ptr<PrefetchRequestStatusListener> listener);

  ~PrefetchContainerObserverForPrefetchRequestStatusListener() override;

  // PrefetchContainerObserver
  void OnWillBeDestroyed(const PrefetchContainer& prefetch_container) override;
  void OnGotInitialEligibility(
      const PrefetchContainer& prefetch_container) override;
  void OnDeterminedHead(const PrefetchContainer& prefetch_container) override;
  void OnPrefetchCompletedOrFailed(
      const PrefetchContainer& prefetch_container) override;

 private:
  explicit PrefetchContainerObserverForPrefetchRequestStatusListener(
      std::unique_ptr<PrefetchRequestStatusListener> listener);

  std::unique_ptr<PrefetchRequestStatusListener> listener_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_OBSERVER_FOR_PREFETCH_REQUEST_STATUS_LISTENER_H_
