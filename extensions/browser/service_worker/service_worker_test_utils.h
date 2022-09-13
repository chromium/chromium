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

// An observer for service worker registration events.
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

  // Returns the number of completed registrations for |scope|.
  int GetCompletedCount(const GURL& scope) const;

 private:
  // ServiceWorkerContextObserver:
  void OnRegistrationCompleted(const GURL& scope) override;
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  RegistrationsMap registrations_completed_map_;
  base::RunLoop stored_run_loop_;
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
