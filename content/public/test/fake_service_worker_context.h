// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_FAKE_SERVICE_WORKER_CONTEXT_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "content/public/browser/service_worker_context.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

class GURL;

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class ServiceWorkerContextObserver;

// Fake implementation of ServiceWorkerContext.
//
// Currently it only implements StartServiceWorkerForNavigationHint. Add
// what you need.
class FakeServiceWorkerContext : public ServiceWorkerContext {
 public:
  using StartServiceWorkerAndDispatchMessageArgs =
      std::tuple<GURL, blink::TransferableMessage, ResultCallback>;

  FakeServiceWorkerContext();

  FakeServiceWorkerContext(const FakeServiceWorkerContext&) = delete;
  FakeServiceWorkerContext& operator=(const FakeServiceWorkerContext&) = delete;

  ~FakeServiceWorkerContext() override;

  void AddObserver(ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(ServiceWorkerContextObserver* observer) override;
  void RegisterServiceWorker(
      const GURL& script_url,
      const blink::StorageKey& key,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      StatusCodeCallback callback) override;
  void UnregisterServiceWorker(const GURL& scope,
                               const blink::StorageKey& key,
                               StatusCodeCallback callback) override;
  void UnregisterServiceWorkerImmediately(const GURL& scope,
                                          const blink::StorageKey& key,
                                          StatusCodeCallback callback) override;
  ServiceWorkerExternalRequestResult StartingExternalRequest(
      int64_t service_worker_version_id,
      content::ServiceWorkerExternalRequestTimeoutType timeout_type,
      const base::Uuid& request_uuid) override;
  ServiceWorkerExternalRequestResult FinishedExternalRequest(
      int64_t service_worker_version_id,
      const base::Uuid& request_uuid) override;
  size_t CountExternalRequestsForTest(const blink::StorageKey& key) override;
  bool ExecuteScriptForTest(
      const std::string& script,
      int64_t service_worker_version_id,
      ServiceWorkerScriptExecutionCallback callback) override;
  bool MaybeHasRegistrationForStorageKey(const blink::StorageKey& key) override;
  void GetAllStorageKeysInfo(GetUsageInfoCallback callback) override;
  void DeleteForStorageKey(const blink::StorageKey& key,
                           ResultCallback callback) override;
  void CheckHasServiceWorker(const GURL& url,
                             const blink::StorageKey& key,
                             CheckHasServiceWorkerCallback callback) override;
  void ClearAllServiceWorkersForTest(base::OnceClosure) override;
  void StartWorkerForScope(
      const GURL& scope,
      const blink::StorageKey& key,
      ServiceWorkerContext::StartWorkerCallback info_callback,
      ServiceWorkerContext::StatusCodeCallback failure_callback) override;
  bool IsLiveStartingServiceWorker(int64_t service_worker_version_id) override;
  bool IsLiveRunningServiceWorker(int64_t service_worker_version_id) override;
  service_manager::InterfaceProvider& GetRemoteInterfaces(
      int64_t service_worker_version_id) override;
  blink::AssociatedInterfaceProvider& GetRemoteAssociatedInterfaces(
      int64_t service_worker_version_id) override;
  void SetForceUpdateOnPageLoadForTesting(
      bool force_update_on_page_load) override;
  void StartServiceWorkerAndDispatchMessage(
      const GURL& scope,
      const blink::StorageKey& key,
      blink::TransferableMessage message,
      FakeServiceWorkerContext::ResultCallback result_callback) override;
  void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      const blink::StorageKey& key,
      StartServiceWorkerForNavigationHintCallback callback) override;
  void WarmUpServiceWorker(const GURL& document_url,
                           const blink::StorageKey& key,
                           WarmUpServiceWorkerCallback callback) override;
  void StopAllServiceWorkersForStorageKey(
      const blink::StorageKey& key) override;
  void StopAllServiceWorkers(base::OnceClosure callback) override;
  const base::flat_map<int64_t, ServiceWorkerRunningInfo>&
  GetRunningServiceWorkerInfos() override;

  // Explicitly notify ServiceWorkerContextObservers added to this context.
  void NotifyObserversOnVersionActivated(int64_t version_id, const GURL& scope);
  void NotifyObserversOnVersionRedundant(int64_t version_id, const GURL& scope);
  void NotifyObserversOnNoControllees(int64_t version_id, const GURL& scope);

  // Inserts `key` into `registered_storage_keys_` if it doesn't already exist.
  void AddRegistrationToRegisteredStorageKeys(const blink::StorageKey& key);

  bool start_service_worker_for_navigation_hint_called() {
    return start_service_worker_for_navigation_hint_called_;
  }

  std::vector<StartServiceWorkerAndDispatchMessageArgs>&
  start_service_worker_and_dispatch_message_calls() {
    return start_service_worker_and_dispatch_message_calls_;
  }

  std::vector<StartServiceWorkerAndDispatchMessageArgs>&
  start_service_worker_and_dispatch_long_running_message_calls() {
    return start_service_worker_and_dispatch_long_running_message_calls_;
  }

  const std::vector<url::Origin>& stop_all_service_workers_for_origin_calls() {
    return stop_all_service_workers_for_origin_calls_;
  }

 private:
  bool start_service_worker_for_navigation_hint_called_ = false;

  std::vector<StartServiceWorkerAndDispatchMessageArgs>
      start_service_worker_and_dispatch_message_calls_;

  std::vector<StartServiceWorkerAndDispatchMessageArgs>
      start_service_worker_and_dispatch_long_running_message_calls_;

  std::vector<url::Origin> stop_all_service_workers_for_origin_calls_;

  base::ObserverList<ServiceWorkerContextObserver, true>::Unchecked observers_;

  std::set<blink::StorageKey> registered_storage_keys_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_SERVICE_WORKER_CONTEXT_H_
