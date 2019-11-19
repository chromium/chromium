// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_FAKE_SERVICE_WORKER_CONTEXT_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "content/public/browser/service_worker_context.h"

class GURL;

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
  ~FakeServiceWorkerContext() override;

  void AddObserver(ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(ServiceWorkerContextObserver* observer) override;
  void RegisterServiceWorker(
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      ResultCallback callback) override;
  void UnregisterServiceWorker(const GURL& scope,
                               ResultCallback callback) override;
  ServiceWorkerExternalRequestResult StartingExternalRequest(
      int64_t service_worker_version_id,
      const std::string& request_uuid) override;
  ServiceWorkerExternalRequestResult FinishedExternalRequest(
      int64_t service_worker_version_id,
      const std::string& request_uuid) override;
  void CountExternalRequestsForTest(
      const GURL& url,
      CountExternalRequestsCallback callback) override;
  void GetAllOriginsInfo(GetUsageInfoCallback callback) override;
  void DeleteForOrigin(const GURL& origin, ResultCallback callback) override;
  void PerformStorageCleanup(base::OnceClosure callback) override;
  void CheckHasServiceWorker(const GURL& url,
                             CheckHasServiceWorkerCallback callback) override;
  void ClearAllServiceWorkersForTest(base::OnceClosure) override;
  void StartWorkerForScope(
      const GURL& scope,
      ServiceWorkerContext::StartWorkerCallback info_callback,
      base::OnceClosure failure_callback) override;
  void StartServiceWorkerAndDispatchMessage(
      const GURL& scope,
      blink::TransferableMessage message,
      FakeServiceWorkerContext::ResultCallback result_callback) override;
  void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      StartServiceWorkerForNavigationHintCallback callback) override;
  void StopAllServiceWorkersForOrigin(const GURL& origin) override;
  void StopAllServiceWorkers(base::OnceClosure callback) override;
  const base::flat_map<int64_t, ServiceWorkerRunningInfo>&
  GetRunningServiceWorkerInfos() override;

  // Explicitly notify ServiceWorkerContextObservers added to this context.
  void NotifyObserversOnVersionActivated(int64_t version_id, const GURL& scope);
  void NotifyObserversOnVersionRedundant(int64_t version_id, const GURL& scope);
  void NotifyObserversOnNoControllees(int64_t version_id, const GURL& scope);

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

  const std::vector<GURL>& stop_all_service_workers_for_origin_calls() {
    return stop_all_service_workers_for_origin_calls_;
  }

 private:
  bool start_service_worker_for_navigation_hint_called_ = false;

  std::vector<StartServiceWorkerAndDispatchMessageArgs>
      start_service_worker_and_dispatch_message_calls_;

  std::vector<StartServiceWorkerAndDispatchMessageArgs>
      start_service_worker_and_dispatch_long_running_message_calls_;

  std::vector<GURL> stop_all_service_workers_for_origin_calls_;

  base::ObserverList<ServiceWorkerContextObserver, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeServiceWorkerContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_SERVICE_WORKER_CONTEXT_H_
