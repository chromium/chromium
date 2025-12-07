// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_handle_impl.h"

#include "base/functional/callback.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"

namespace content {

PrefetchContainerObserver::PrefetchContainerObserver() = default;

PrefetchContainerObserver::~PrefetchContainerObserver() = default;

void PrefetchContainerObserver::SetOnPrefetchHeadReceivedCallback(
    base::RepeatingCallback<void(const network::mojom::URLResponseHead&)>
        on_prefetch_head_received) {
  on_prefetch_head_received_ = std::move(on_prefetch_head_received);
}

void PrefetchContainerObserver::SetOnPrefetchCompletedOrFailedCallback(
    base::RepeatingCallback<
        void(const network::URLLoaderCompletionStatus& completion_status,
             const std::optional<int>& response_code)>
        on_prefetch_completed_or_failed) {
  on_prefetch_completed_or_failed_ = std::move(on_prefetch_completed_or_failed);
}

void PrefetchContainerObserver::OnWillBeDestroyed(
    PrefetchContainer& prefetch_container) {}

void PrefetchContainerObserver::OnGotInitialEligibility(
    PrefetchContainer& prefetch_container,
    PreloadingEligibility eligibility) {}

void PrefetchContainerObserver::OnDeterminedHead(
    PrefetchContainer& prefetch_container) {
  if (on_prefetch_head_received_) {
    // This condition will be used in a callback provided in the future.
    // See
    // https://chromium-review.googlesource.com/c/chromium/src/+/6615559/comment/3f439d19_8c9cf99a
    //
    // TODO(crbug.com/400761083): Use the callback.
    if (prefetch_container.GetNonRedirectResponseReader() &&
        prefetch_container.GetNonRedirectResponseReader()->load_state() ==
            PrefetchResponseReader::LoadState::kResponseReceived) {
      on_prefetch_head_received_.Run(*prefetch_container.GetNonRedirectHead());
    }
  }
}

void PrefetchContainerObserver::OnPrefetchCompletedOrFailed(
    PrefetchContainer& prefetch_container,
    const network::URLLoaderCompletionStatus& completion_status,
    const std::optional<int>& response_code) {
  // `IsDecoy()` check is added to preserve the existing behavior.
  // https://crbug.com/400761083
  if (prefetch_container.IsDecoy()) {
    return;
  }
  if (on_prefetch_completed_or_failed_) {
    on_prefetch_completed_or_failed_.Run(completion_status, response_code);
  }
}

PrefetchHandleImpl::PrefetchHandleImpl(
    base::WeakPtr<PrefetchService> prefetch_service,
    base::WeakPtr<PrefetchContainer> prefetch_container)
    : prefetch_service_(std::move(prefetch_service)),
      prefetch_container_(std::move(prefetch_container)) {
  CHECK(prefetch_service_);
  // Note that `prefetch_container_` can be nullptr.

  if (prefetch_container_) {
    prefetch_container_->AddObserver(&prefetch_container_observer_);
  }
}

PrefetchHandleImpl::~PrefetchHandleImpl() {
  if (prefetch_container_) {
    prefetch_container_->RemoveObserver(&prefetch_container_observer_);
  }

  // Notify `PrefetchService` that the corresponding `PrefetchContainer` is no
  // longer needed. `PrefetchService` might release the container and its
  // corresponding resources by its decision with best-effort.
  if (prefetch_service_) {
    // TODO(crbug.com/390329781): Consider setting appropriate
    // PrefetchStatus/PreloadingAttempt.
    if (prefetch_status_on_release_started_prefetch_ && prefetch_container_) {
      switch (prefetch_container_->GetLoadState()) {
        case PrefetchContainer::LoadState::kNotStarted:
        case PrefetchContainer::LoadState::kEligible:
        case PrefetchContainer::LoadState::kFailedIneligible:
        case PrefetchContainer::LoadState::kFailedHeldback:
          break;
        case PrefetchContainer::LoadState::kStarted:
        case PrefetchContainer::LoadState::kDeterminedHead:
        case PrefetchContainer::LoadState::kFailedDeterminedHead:
        case PrefetchContainer::LoadState::kCompleted:
        case PrefetchContainer::LoadState::kFailed:
          prefetch_container_->SetPrefetchStatus(
              *prefetch_status_on_release_started_prefetch_);
          break;
      }
    }
    prefetch_service_->MayReleasePrefetch(prefetch_container_);
  }
}

void PrefetchHandleImpl::SetOnPrefetchHeadReceivedCallback(
    base::RepeatingCallback<void(const network::mojom::URLResponseHead&)>
        on_prefetch_head_received) {
  prefetch_container_observer_.SetOnPrefetchHeadReceivedCallback(
      std::move(on_prefetch_head_received));
}

void PrefetchHandleImpl::SetOnPrefetchCompletedOrFailedCallback(
    base::RepeatingCallback<
        void(const network::URLLoaderCompletionStatus& completion_status,
             const std::optional<int>& response_code)>
        on_prefetch_completed_or_failed) {
  prefetch_container_observer_.SetOnPrefetchCompletedOrFailedCallback(
      std::move(on_prefetch_completed_or_failed));
}

bool PrefetchHandleImpl::IsAlive() const {
  return static_cast<bool>(prefetch_container_);
}

void PrefetchHandleImpl::SetPrefetchStatusOnReleaseStartedPrefetch(
    PrefetchStatus prefetch_status_on_release_started_prefetch) {
  CHECK(!prefetch_status_on_release_started_prefetch_);
  prefetch_status_on_release_started_prefetch_ =
      prefetch_status_on_release_started_prefetch;
}

}  // namespace content
