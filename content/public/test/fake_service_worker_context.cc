// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_service_worker_context.h"

#include <utility>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

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
    const blink::StorageKey& key,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    StatusCodeCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::UnregisterServiceWorker(
    const GURL& scope,
    const blink::StorageKey& key,
    ResultCallback callback) {
  NOTREACHED();
}
ServiceWorkerExternalRequestResult
FakeServiceWorkerContext::StartingExternalRequest(
    int64_t service_worker_version_id,
    ServiceWorkerExternalRequestTimeoutType timeout_type,
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
size_t FakeServiceWorkerContext::CountExternalRequestsForTest(
    const blink::StorageKey& key) {
  NOTREACHED();
  return 0u;
}
bool FakeServiceWorkerContext::ExecuteScriptForTest(
    const std::string& script,
    int64_t version_id,
    ServiceWorkerScriptExecutionCallback callback) {
  NOTREACHED();
  return false;
}
bool FakeServiceWorkerContext::MaybeHasRegistrationForStorageKey(
    const blink::StorageKey& key) {
  return registered_storage_keys_.find(key) != registered_storage_keys_.end();
}
void FakeServiceWorkerContext::GetAllOriginsInfo(
    GetUsageInfoCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::DeleteForStorageKey(const blink::StorageKey& key,
                                                   ResultCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::CheckHasServiceWorker(
    const GURL& url,
    const blink::StorageKey& key,
    CheckHasServiceWorkerCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::CheckOfflineCapability(
    const GURL& url,
    const blink::StorageKey& key,
    const ServiceWorkerContext::CheckOfflineCapabilityCallback callback) {
  NOTREACHED();
}
void FakeServiceWorkerContext::ClearAllServiceWorkersForTest(
    base::OnceClosure) {
  NOTREACHED();
}
void FakeServiceWorkerContext::StartWorkerForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    ServiceWorkerContext::StartWorkerCallback info_callback,
    ServiceWorkerContext::StatusCodeCallback failure_callback) {
  NOTREACHED();
}

bool FakeServiceWorkerContext::IsLiveRunningServiceWorker(
    int64_t service_worker_version_id) {
  NOTREACHED();
  return false;
}

service_manager::InterfaceProvider&
FakeServiceWorkerContext::GetRemoteInterfaces(
    int64_t service_worker_version_id) {
  NOTREACHED();
  static service_manager::InterfaceProvider interface_provider(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  return interface_provider;
}

void FakeServiceWorkerContext::StartServiceWorkerForNavigationHint(
    const GURL& document_url,
    const blink::StorageKey& key,
    StartServiceWorkerForNavigationHintCallback callback) {
  start_service_worker_for_navigation_hint_called_ = true;
}

void FakeServiceWorkerContext::StartServiceWorkerAndDispatchMessage(
    const GURL& scope,
    const blink::StorageKey& key,
    blink::TransferableMessage message,
    ResultCallback result_callback) {
  start_service_worker_and_dispatch_message_calls_.push_back(
      std::make_tuple(scope, std::move(message), std::move(result_callback)));
}

void FakeServiceWorkerContext::StopAllServiceWorkersForStorageKey(
    const blink::StorageKey& key) {
  stop_all_service_workers_for_origin_calls_.push_back(key.origin());
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

void FakeServiceWorkerContext::AddRegistrationToRegisteredStorageKeys(
    const blink::StorageKey& key) {
  registered_storage_keys_.insert(key);
}

}  // namespace content
