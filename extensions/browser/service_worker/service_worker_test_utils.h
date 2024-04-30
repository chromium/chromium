// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_

#include <stddef.h>

#include <map>
#include <optional>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {
class ServiceWorkerContext;
class BrowserContext;
}

namespace extensions {
namespace service_worker_test_utils {

// Get the ServiceWorkerContext for the `browser_context`.
content::ServiceWorkerContext* GetServiceWorkerContext(
    content::BrowserContext* browser_context);

// An observer for service worker registration events.
// Note: This class only works well when there is a *single* service worker
// being registered. We could extend this to track multiple workers.
class TestRegistrationObserver : public content::ServiceWorkerContextObserver {
 public:
  using RegistrationsMap = std::map<GURL, int>;

  explicit TestRegistrationObserver(content::BrowserContext* browser_context);
  ~TestRegistrationObserver() override;

  TestRegistrationObserver(const TestRegistrationObserver&) = delete;
  TestRegistrationObserver& operator=(const TestRegistrationObserver&) = delete;

  // Wait for the first service worker registration with an extension scheme
  // scope to be stored.
  void WaitForRegistrationStored();

  // Wait for OnVersionStartedRunning event is triggered, so that the observer
  // captures the running service worker version id.
  void WaitForWorkerStart();

  // Waits for the OnVersionActivated() notification from the
  // ServiceWorkerContext.
  void WaitForWorkerActivated();

  // Returns the number of completed registrations for |scope|.
  int GetCompletedCount(const GURL& scope) const;

  // Get the running service worker version id.
  // This method must be called after WaitForWorkerStart().
  int64_t GetServiceWorkerVersionId() const {
    CHECK(running_version_id_);
    return running_version_id_.value();
  }

 private:
  // ServiceWorkerContextObserver:
  void OnRegistrationCompleted(const GURL& scope) override;
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope) override;
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override;
  void OnVersionActivated(int64_t version_id, const GURL& scope) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  RegistrationsMap registrations_completed_map_;
  base::RunLoop stored_run_loop_;
  base::RunLoop started_run_loop_;
  base::RunLoop activated_run_loop_;
  std::optional<int64_t> running_version_id_;
  raw_ptr<content::ServiceWorkerContext> context_ = nullptr;
};

// Observes ProcessManager::UnregisterServiceWorker.
class UnregisterWorkerObserver : public ProcessManagerObserver {
 public:
  UnregisterWorkerObserver(ProcessManager* process_manager,
                           const ExtensionId& extension_id);
  ~UnregisterWorkerObserver() override;

  UnregisterWorkerObserver(const UnregisterWorkerObserver&) = delete;
  UnregisterWorkerObserver& operator=(const UnregisterWorkerObserver&) = delete;

  // ProcessManagerObserver:
  void OnServiceWorkerUnregistered(const WorkerId& worker_id) override;

  // Waits for ProcessManager::UnregisterServiceWorker for |extension_id_|.
  void WaitForUnregister();

 private:
  ExtensionId extension_id_;
  base::ScopedObservation<ProcessManager, ProcessManagerObserver> observation_{
      this};
  base::RunLoop run_loop_;
};

class TestServiceWorkerTaskQueueObserver
    : public ServiceWorkerTaskQueue::TestObserver {
 public:
  TestServiceWorkerTaskQueueObserver();
  ~TestServiceWorkerTaskQueueObserver() override;

  TestServiceWorkerTaskQueueObserver(
      const TestServiceWorkerTaskQueueObserver&) = delete;
  TestServiceWorkerTaskQueueObserver& operator=(
      const TestServiceWorkerTaskQueueObserver&) = delete;

  struct WorkerStartFailedData {
    size_t num_pending_tasks = 0;
    blink::ServiceWorkerStatusCode status_code =
        blink::ServiceWorkerStatusCode::kOk;
  };

  void WaitForWorkerStarted(const ExtensionId& extension_id);
  void WaitForWorkerStopped(const ExtensionId& extension_id);
  void WaitForWorkerContextInitialized(const ExtensionId& extension_id);
  WorkerStartFailedData WaitForDidStartWorkerFail(
      const ExtensionId& extension_id);
  void WaitForOnActivateExtension(const ExtensionId& extension_id);
  bool WaitForRegistrationMismatchMitigation(const ExtensionId& extension_id);

  std::optional<bool> WillRegisterServiceWorker(
      const ExtensionId& extension_id) const;

  int GetRequestedWorkerStartedCount(const ExtensionId& extension_id) const;

  // ServiceWorkerTaskQueue::TestObserver
  void DidStartWorker(const ExtensionId& extension_id) override;
  void DidInitializeServiceWorkerContext(
      const ExtensionId& extension_id) override;
  void DidStartWorkerFail(const ExtensionId& extension_id,
                          size_t num_pending_tasks,
                          blink::ServiceWorkerStatusCode status_code) override;
  void OnActivateExtension(const ExtensionId& extension_id,
                           bool will_register_service_worker) override;
  void RegistrationMismatchMitigated(const ExtensionId& extension_id,
                                     bool success) override;
  void RequestedWorkerStart(const ExtensionId& extension_id) override;
  void DidStopServiceWorkerContext(const ExtensionId& extension_id) override;

 private:
  std::map<ExtensionId, bool> activated_map_;

  std::map<ExtensionId, WorkerStartFailedData> failed_map_;

  std::set<ExtensionId> inited_set_;

  std::map<ExtensionId, bool> mitigated_map_;

  std::map<ExtensionId, int> requested_worker_started_map_;

  std::set<ExtensionId> started_set_;

  std::set<ExtensionId> stopped_set_;

  base::OnceClosure quit_closure_;
};

}  // namespace service_worker_test_utils
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
