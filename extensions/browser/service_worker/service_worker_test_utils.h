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

// A class for ServiceWorkerContextObserver events.
// Note: This class only works well when there is a *single* service worker
// being registered. We could extend this to track multiple workers.
class TestServiceWorkerContextObserver
    : public content::ServiceWorkerContextObserver {
 public:
  explicit TestServiceWorkerContextObserver(
      content::ServiceWorkerContext* context,
      std::optional<ExtensionId> extension_id = std::nullopt);
  explicit TestServiceWorkerContextObserver(
      content::BrowserContext* browser_context,
      std::optional<ExtensionId> extension_id = std::nullopt);
  ~TestServiceWorkerContextObserver() override;

  TestServiceWorkerContextObserver(const TestServiceWorkerContextObserver&) =
      delete;
  TestServiceWorkerContextObserver& operator=(
      const TestServiceWorkerContextObserver&) = delete;

  // Wait for the first service worker registration with an extension scheme
  // scope to be stored.
  void WaitForRegistrationStored();

  // Wait for OnVersionStartedRunning event is triggered, so that the observer
  // captures the running service worker version ID. Returns the version ID.
  int64_t WaitForWorkerStarted();

  // Wait for OnVersionStoppedRunning event is triggered, so that the observer
  // captures the stopped service worker version ID. Returns the version ID.
  int64_t WaitForWorkerStopped();

  // Waits for the OnVersionActivated() notification from the
  // ServiceWorkerContext. Returns the version ID.
  int64_t WaitForWorkerActivated();

  // Sets the ID of an already-running worker. This is handy so this observer
  // can be instantiated after the extension has already started.
  void SetRunningId(int64_t version_id) { running_version_id_ = version_id; }

  // Returns the number of completed registrations for |scope|.
  int GetCompletedCount(const GURL& scope) const;

 private:
  // ServiceWorkerContextObserver:
  void OnRegistrationCompleted(const GURL& scope) override;
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope) override;
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override;
  void OnVersionStoppedRunning(int64_t version_id) override;
  void OnVersionActivated(int64_t version_id, const GURL& scope) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  using RegistrationsMap = std::map<GURL, int>;

  RegistrationsMap registrations_completed_map_;

  // Multiple events may come in so we must wait for the specific event
  // to be triggered.
  base::OnceClosure activated_quit_closure_;
  base::OnceClosure started_quit_closure_;
  base::OnceClosure stored_quit_closure_;
  base::OnceClosure stopped_quit_closure_;

  const std::optional<GURL> extension_scope_;

  std::optional<bool> registration_stored_;
  std::optional<int64_t> activated_version_id_;
  std::optional<int64_t> running_version_id_;
  std::optional<int64_t> stopped_version_id_;

  raw_ptr<content::ServiceWorkerContext> context_ = nullptr;

  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
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
  void OnStoppedTrackingServiceWorkerInstance(
      const WorkerId& worker_id) override;

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
