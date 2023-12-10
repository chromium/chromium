// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension_id.h"
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

}  // namespace service_worker_test_utils
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
