// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container_async_observer.h"

#include "base/task/sequenced_task_runner.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"

namespace content {

PrefetchContainerAsyncObserverBase::PrefetchContainerAsyncObserverBase() =
    default;

PrefetchContainerAsyncObserverBase::~PrefetchContainerAsyncObserverBase() =
    default;

void PrefetchContainerAsyncObserverBase::OnWillBeDestroyed(
    const PrefetchContainer& prefetch_container) {}

void PrefetchContainerAsyncObserverBase::OnGotInitialEligibility(
    const PrefetchContainer& prefetch_container) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrefetchContainerAsyncObserverBase::Dispatch,
                     weak_factory_.GetWeakPtr(),
                     &PrefetchContainerObserver::OnGotInitialEligibility,
                     prefetch_container.GetWeakPtr()));
}

void PrefetchContainerAsyncObserverBase::OnDeterminedHead(
    const PrefetchContainer& prefetch_container) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PrefetchContainerAsyncObserverBase::Dispatch,
                                weak_factory_.GetWeakPtr(),
                                &PrefetchContainerObserver::OnDeterminedHead,
                                prefetch_container.GetWeakPtr()));
}

void PrefetchContainerAsyncObserverBase::OnPrefetchCompletedOrFailed(
    const PrefetchContainer& prefetch_container) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrefetchContainerAsyncObserverBase::Dispatch,
                     weak_factory_.GetWeakPtr(),
                     &PrefetchContainerObserver::OnPrefetchCompletedOrFailed,
                     prefetch_container.GetWeakPtr()));
}

void PrefetchContainerAsyncObserverBase::Dispatch(
    void (PrefetchContainerObserver::*method)(const PrefetchContainer&),
    base::WeakPtr<const PrefetchContainer> prefetch_container) {
  if (!prefetch_container) {
    // `prefetch_container` has been destroyed during the async hop. Do nothing.
    return;
  }
  (GetUnderlyingObserver().*method)(*prefetch_container);
}

}  // namespace content
