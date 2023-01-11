// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_test_service_worker.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"

namespace content {

BackgroundFetchTestServiceWorker::BackgroundFetchTestServiceWorker(
    EmbeddedWorkerTestHelper* helper)
    : FakeServiceWorker(helper) {}

BackgroundFetchTestServiceWorker::~BackgroundFetchTestServiceWorker() {
  if (delayed_closure_)
    std::move(delayed_closure_).Run();
}

void BackgroundFetchTestServiceWorker::DispatchBackgroundFetchAbortEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback
        callback) {
  last_registration_ = std::move(registration);

  if (fail_abort_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED);
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  }

  if (abort_event_closure_)
    std::move(abort_event_closure_).Run();
}

void BackgroundFetchTestServiceWorker::DispatchBackgroundFetchClickEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback
        callback) {
  last_registration_ = std::move(registration);

  if (fail_click_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED);
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  }

  if (click_event_closure_)
    std::move(click_event_closure_).Run();
}

void BackgroundFetchTestServiceWorker::DispatchBackgroundFetchFailEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback
        callback) {
  last_registration_ = std::move(registration);

  if (delay_dispatch_) {
    delayed_closure_ = base::BindOnce(
        std::move(callback), blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  } else if (fail_fetch_fail_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED);
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  }

  if (fetch_fail_event_closure_)
    std::move(fetch_fail_event_closure_).Run();
}

void BackgroundFetchTestServiceWorker::DispatchBackgroundFetchSuccessEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
        callback) {
  last_registration_ = std::move(registration);

  if (delay_dispatch_) {
    delayed_closure_ = base::BindOnce(
        std::move(callback), blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  } else if (fail_fetched_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED);
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  }

  if (fetched_event_closure_)
    std::move(fetched_event_closure_).Run();
}

}  // namespace content
