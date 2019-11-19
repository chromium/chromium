// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_service_worker_context.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content {

FakeServiceWorkerContext::FakeServiceWorkerContext() {}
FakeServiceWorkerContext::~FakeServiceWorkerContext() {}

void FakeServiceWorkerContext::AddObserver(
    ServiceWorkerContextObserver* observer) {
  observers_.AddObserver(observer);
}
void FakeServiceWorkerContext::RemoveObserver(
    ServiceWorkerContextObserver* observer) {
  observers_.RemoveObserver(observer);
}
void FakeServiceWorkerContext::RegisterServiceWorker(
    const GURL& script_url,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    ResultCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::UnregisterServiceWorker(
    const GURL& scope,
    ResultCallback callback) {
  NOTREACHED();
}
ServiceWorkerExternalRequestResult
FakeServiceWorkerContext::StartingExternalRequest(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  NOTREACHED();
  return ServiceWorkerExternalRequestResult::kWorkerNotFound;
}
ServiceWorkerExternalRequestResult
FakeServiceWorkerContext::FinishedExternalRequest(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  NOTREACHED();
  return ServiceWorkerExternalRequestResult::kWorkerNotFound;
}
void FakeServiceWorkerContext::CountExternalRequestsForTest(
    const GURL& url,
    CountExternalRequestsCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::GetAllOriginsInfo(
    GetUsageInfoCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::DeleteForOrigin(const GURL& origin,
                                               ResultCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::PerformStorageCleanup(
    base::OnceClosure callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::CheckHasServiceWorker(
    const GURL& url,
    CheckHasServiceWorkerCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::ClearAllServiceWorkersForTest(
    base::OnceClosure) {
  NOTREACHED();
}
void FakeServiceWorkerContext::StartWorkerForScope(
    const GURL& scope,
    ServiceWorkerContext::StartWorkerCallback info_callback,
    base::OnceClosure failure_callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::StartServiceWorkerForNavigationHint(
    const GURL& document_url,
    StartServiceWorkerForNavigationHintCallback callback) {
  start_service_worker_for_navigation_hint_called_ = true;
}

void FakeServiceWorkerContext::StartServiceWorkerAndDispatchMessage(
    const GURL& scope,
    blink::TransferableMessage message,
    ResultCallback result_callback) {
  start_service_worker_and_dispatch_message_calls_.push_back(
      std::make_tuple(scope, std::move(message), std::move(result_callback)));
}

void FakeServiceWorkerContext::StopAllServiceWorkersForOrigin(
    const GURL& origin) {
  stop_all_service_workers_for_origin_calls_.push_back(origin);
}

void FakeServiceWorkerContext::StopAllServiceWorkers(base::OnceClosure) {
  NOTREACHED();
}

const base::flat_map<int64_t, ServiceWorkerRunningInfo>&
FakeServiceWorkerContext::GetRunningServiceWorkerInfos() {
  NOTREACHED();
  static const base::NoDestructor<
      base::flat_map<int64_t, ServiceWorkerRunningInfo>>
      empty_running_workers;
  return *empty_running_workers;
}

void FakeServiceWorkerContext::NotifyObserversOnVersionActivated(
    int64_t version_id,
    const GURL& scope) {
  for (auto& observer : observers_)
    observer.OnVersionActivated(version_id, scope);
}

void FakeServiceWorkerContext::NotifyObserversOnVersionRedundant(
    int64_t version_id,
    const GURL& scope) {
  for (auto& observer : observers_)
    observer.OnVersionRedundant(version_id, scope);
}

void FakeServiceWorkerContext::NotifyObserversOnNoControllees(
    int64_t version_id,
    const GURL& scope) {
  for (auto& observer : observers_)
    observer.OnNoControllees(version_id, scope);
}

}  // namespace content
