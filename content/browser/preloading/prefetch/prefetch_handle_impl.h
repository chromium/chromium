// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_HANDLE_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_HANDLE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/public/browser/prefetch_handle.h"

namespace content {

class PrefetchService;
enum class PrefetchStatus;

// Currently, it must be visible from `PrefetchResponseReader` for `friend`.
//
// TODO(crbug.com/400761083): Put it into `namespace prefetch_handle`.
class PrefetchContainerObserver final : public PrefetchContainer::Observer {
 public:
  PrefetchContainerObserver();
  ~PrefetchContainerObserver() override;

  // Not movable nor copyable.
  PrefetchContainerObserver(PrefetchContainerObserver&& other) = delete;
  PrefetchContainerObserver& operator=(PrefetchContainerObserver&& other) =
      delete;
  PrefetchContainerObserver(const PrefetchContainerObserver&) = delete;
  PrefetchContainerObserver& operator=(const PrefetchContainerObserver&) =
      delete;

  void SetOnPrefetchHeadReceivedCallback(
      base::RepeatingCallback<void(const network::mojom::URLResponseHead&)>
          on_prefetch_head_received);
  void SetOnPrefetchCompletedOrFailedCallback(
      base::RepeatingCallback<
          void(const network::URLLoaderCompletionStatus& completion_status,
               const std::optional<int>& response_code)>
          on_prefetch_completed_or_failed);

  // Implements `PrefetchContainer::Observer`.
  void OnWillBeDestroyed(PrefetchContainer& prefetch_container) override;
  void OnGotInitialEligibility(PrefetchContainer& prefetch_container,
                               PreloadingEligibility eligibility) override;
  void OnDeterminedHead(PrefetchContainer& prefetch_container) override;
  void OnPrefetchCompletedOrFailed(
      PrefetchContainer& prefetch_container,
      const network::URLLoaderCompletionStatus& completion_status,
      const std::optional<int>& response_code) override;

 private:
  base::RepeatingCallback<void(const network::mojom::URLResponseHead&)>
      on_prefetch_head_received_;
  base::RepeatingCallback<void(
      const network::URLLoaderCompletionStatus& completion_status,
      const std::optional<int>& response_code)>
      on_prefetch_completed_or_failed_;
};

class PrefetchHandleImpl final : public PrefetchHandle {
 public:
  PrefetchHandleImpl(base::WeakPtr<PrefetchService> prefetch_service,
                     base::WeakPtr<PrefetchContainer> prefetch_container);
  ~PrefetchHandleImpl() override;

  // `PrefetchHandle` implementations
  void SetOnPrefetchHeadReceivedCallback(
      base::RepeatingCallback<void(const network::mojom::URLResponseHead&)>
          on_prefetch_head_received) override;
  void SetOnPrefetchCompletedOrFailedCallback(
      base::RepeatingCallback<
          void(const network::URLLoaderCompletionStatus& completion_status,
               const std::optional<int>& response_code)>
          on_prefetch_completed_or_failed) override;
  bool IsAlive() const override;

  // TODO(crbug.com/390329781): The following methods are tentative interface
  // for incrementally migrating `//content/` internal code to `PrefetchHandle`.
  // Revisit the semantics when exposing for the public API.

  // Sets the `PrefetchStatus` that will be set to the `PrefetchContainer` when
  // this handle is destroyed and the `PrefetchContainer`'s `LoadState` is
  // `kStarted` at that time.
  void SetPrefetchStatusOnReleaseStartedPrefetch(
      PrefetchStatus prefetch_status_on_release_started_prefetch);

 private:
  base::WeakPtr<PrefetchService> prefetch_service_;
  base::WeakPtr<PrefetchContainer> prefetch_container_;
  PrefetchContainerObserver prefetch_container_observer_;
  std::optional<PrefetchStatus> prefetch_status_on_release_started_prefetch_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_HANDLE_IMPL_H_
