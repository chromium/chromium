// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container_observer_for_prefetch_request_status_listener.h"

#include "base/memory/ptr_util.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"

namespace content {

// static
std::unique_ptr<PrefetchContainerObserverForPrefetchRequestStatusListener>
PrefetchContainerObserverForPrefetchRequestStatusListener::Create(
    std::unique_ptr<PrefetchRequestStatusListener> listener) {
  if (!listener) {
    return nullptr;
  }
  return base::WrapUnique(
      new PrefetchContainerObserverForPrefetchRequestStatusListener(
          std::move(listener)));
}

PrefetchContainerObserverForPrefetchRequestStatusListener::
    PrefetchContainerObserverForPrefetchRequestStatusListener(
        std::unique_ptr<PrefetchRequestStatusListener> listener)
    : listener_(std::move(listener)) {
  CHECK(listener_);
}

PrefetchContainerObserverForPrefetchRequestStatusListener::
    ~PrefetchContainerObserverForPrefetchRequestStatusListener() = default;

void PrefetchContainerObserverForPrefetchRequestStatusListener::
    OnWillBeDestroyed(const PrefetchContainer& prefetch_container) {}

void PrefetchContainerObserverForPrefetchRequestStatusListener::
    OnGotInitialEligibility(const PrefetchContainer& prefetch_container) {
  if (prefetch_container.GetInitialEligibility() !=
      PreloadingEligibility::kEligible) {
    listener_->OnPrefetchStartFailedGeneric();
  }
}

void PrefetchContainerObserverForPrefetchRequestStatusListener::
    OnDeterminedHead(const PrefetchContainer& prefetch_container) {}

void PrefetchContainerObserverForPrefetchRequestStatusListener::
    OnPrefetchCompletedOrFailed(const PrefetchContainer& prefetch_container) {
  switch (prefetch_container.GetPrefetchStatus()) {
    case PrefetchStatus::kPrefetchSuccessful:
    case PrefetchStatus::kPrefetchResponseUsed:
      listener_->OnPrefetchResponseCompleted();
      break;
    case PrefetchStatus::kPrefetchFailedNon2XX:
      listener_->OnPrefetchResponseServerError(
          prefetch_container.GetResponseCode().value_or(0));
      break;
    default:
      listener_->OnPrefetchResponseError();
      break;
  }
}

}  // namespace content
