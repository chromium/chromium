// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_service_worker_context.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
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
  NOTREACHED_IN_MIGRATION();
}
void FakeServiceWorkerContext::UnregisterServiceWorker(
    const GURL& scope,
    const blink::StorageKey& key,
    StatusCodeCallback callback) {
  NOTREACHED_IN_MIGRATION();
}
void FakeServiceWorkerContext::UnregisterServiceWorkerImmediately(
    const GURL& scope,
    const blink::StorageKey& key,
    StatusCodeCallback callback) {
  NOTREACHED_IN_MIGRATION();
}
ServiceWorkerExternalRequestResult
FakeServiceWorkerContext::StartingExternalRequest(
    int64_t service_worker_version_id,
    ServiceWorkerExternalRequestTimeoutType timeout_type,
    const base::Uuid& request_uuid) {
  NOTREACHED_IN_MIGRATION();
  return ServiceWorkerExternalRequestResult::kWorkerNotFound;
}
ServiceWorkerExternalRequestResult
FakeServiceWorkerContext::FinishedExternalRequest(
    int64_t service_worker_version_id,
    const base::Uuid& request_uuid) {
  NOTREACHED_IN_MIGRATION();
  return ServiceWorkerExternalRequestResult::kWorkerNotFound;
}
size_t FakeServiceWorkerContext::CountExternalRequestsForTest(
    const blink::StorageKey& key) {
  NOTREACHED_IN_MIGRATION();
  return 0u;
}
bool FakeServiceWorkerContext::ExecuteScriptForTest(
    const std::string& script,
    int64_t version_id,
    ServiceWorkerScriptExecutionCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return false;
}
bool FakeServiceWorkerContext::MaybeHasRegistrationForStorageKey(
    const blink::StorageKey& key) {
  return base::Contains(registered_storage_keys_, key);
}
void FakeServiceWorkerContext::GetAllStorageKeysInfo(
    GetUsageInfoCallback callback) {
  NOTREACHED_IN_MIGRATION();
}
void FakeServiceWorkerContext::DeleteForStorageKey(const blink::StorageKey& key,
                                                   ResultCallback callback) {
  NOTREACHED_IN_MIGRATION();
}
void FakeServiceWorkerContext::CheckHasServiceWorker(
    const GURL& url,
    const blink::StorageKey& key,
    CheckHasServiceWorkerCallback callback) {
  NOTREACHED_IN_MIGRATION();
}
void FakeServiceWorkerContext::ClearAllServiceWorkersForTest(
    base::OnceClosure) {
  NOTREACHED_IN_MIGRATION();
}
void FakeServiceWorkerContext::StartWorkerForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    ServiceWorkerContext::StartWorkerCallback info_callback,
    ServiceWorkerContext::StatusCodeCallback failure_callback) {
  NOTREACHED_IN_MIGRATION();
}

bool FakeServiceWorkerContext::IsLiveStartingServiceWorker(
    int64_t service_worker_version_id) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool FakeServiceWorkerContext::IsLiveRunningServiceWorker(
    int64_t service_worker_version_id) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

service_manager::InterfaceProvider&
FakeServiceWorkerContext::GetRemoteInterfaces(
    int64_t service_worker_version_id) {
  NOTREACHED();
}

blink::AssociatedInterfaceProvider&
FakeServiceWorkerContext::GetRemoteAssociatedInterfaces(
    int64_t service_worker_version_id) {
  NOTREACHED();
}

void FakeServiceWorkerContext::SetForceUpdateOnPageLoadForTesting(
    bool force_update_on_page_load) {
  NOTREACHED();
}

void FakeServiceWorkerContext::StartServiceWorkerForNavigationHint(
    const GURL& document_url,
    const blink::StorageKey& key,
    StartServiceWorkerForNavigationHintCallback callback) {
  start_service_worker_for_navigation_hint_called_ = true;
}

void FakeServiceWorkerContext::WarmUpServiceWorker(
    const GURL& document_url,
    const blink::StorageKey& key,
    WarmUpServiceWorkerCallback callback) {
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
}

const base::flat_map<int64_t, ServiceWorkerRunningInfo>&
FakeServiceWorkerContext::GetRunningServiceWorkerInfos() {
  NOTREACHED_IN_MIGRATION();
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
