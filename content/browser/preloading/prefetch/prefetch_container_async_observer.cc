// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container_async_observer.h"

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"

namespace content {

// static
std::unique_ptr<PrefetchContainerAsyncObserver>
PrefetchContainerAsyncObserver::Create(
    std::unique_ptr<PrefetchContainerObserver> observer) {
  if (!observer) {
    return nullptr;
  }
  return base::WrapUnique(
      new PrefetchContainerAsyncObserver(std::move(observer)));
}

PrefetchContainerAsyncObserver::PrefetchContainerAsyncObserver(
    std::unique_ptr<PrefetchContainerObserver> observer)
    : observer_(std::move(observer)) {
  CHECK(observer_);
}

PrefetchContainerAsyncObserver::~PrefetchContainerAsyncObserver() = default;

void PrefetchContainerAsyncObserver::OnWillBeDestroyed(
    const PrefetchContainer& prefetch_container) {}

void PrefetchContainerAsyncObserver::OnGotInitialEligibility(
    const PrefetchContainer& prefetch_container) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrefetchContainerAsyncObserver::Dispatch,
                     weak_factory_.GetWeakPtr(),
                     &PrefetchContainerObserver::OnGotInitialEligibility,
                     prefetch_container.GetWeakPtr()));
}

void PrefetchContainerAsyncObserver::OnDeterminedHead(
    const PrefetchContainer& prefetch_container) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PrefetchContainerAsyncObserver::Dispatch,
                                weak_factory_.GetWeakPtr(),
                                &PrefetchContainerObserver::OnDeterminedHead,
                                prefetch_container.GetWeakPtr()));
}

void PrefetchContainerAsyncObserver::OnPrefetchCompletedOrFailed(
    const PrefetchContainer& prefetch_container) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrefetchContainerAsyncObserver::Dispatch,
                     weak_factory_.GetWeakPtr(),
                     &PrefetchContainerObserver::OnPrefetchCompletedOrFailed,
                     prefetch_container.GetWeakPtr()));
}

void PrefetchContainerAsyncObserver::Dispatch(
    void (PrefetchContainerObserver::*method)(const PrefetchContainer&),
    base::WeakPtr<const PrefetchContainer> prefetch_container) {
  if (!prefetch_container) {
    // `prefetch_container` has been destroyed during the async hop. Do nothing.
    return;
  }
  (observer_.get()->*method)(*prefetch_container);
}

}  // namespace content
