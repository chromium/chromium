// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_test_utils.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/constants.h"

namespace extensions {
namespace service_worker_test_utils {

content::ServiceWorkerContext* GetServiceWorkerContext(
    content::BrowserContext* browser_context) {
  return browser_context->GetDefaultStoragePartition()
      ->GetServiceWorkerContext();
}
// TestRegistrationObserver ----------------------------------------------------

TestRegistrationObserver::TestRegistrationObserver(
    content::BrowserContext* browser_context)
    : context_(browser_context->GetDefaultStoragePartition()
                   ->GetServiceWorkerContext()) {
  context_->AddObserver(this);
}

TestRegistrationObserver::~TestRegistrationObserver() {
  if (context_)
    context_->RemoveObserver(this);
}

void TestRegistrationObserver::WaitForRegistrationStored() {
  stored_run_loop_.Run();
}

void TestRegistrationObserver::WaitForWorkerStart() {
  started_run_loop_.Run();
}

void TestRegistrationObserver::WaitForWorkerActivated() {
  activated_run_loop_.Run();
}

int TestRegistrationObserver::GetCompletedCount(const GURL& scope) const {
  const auto it = registrations_completed_map_.find(scope);
  return it == registrations_completed_map_.end() ? 0 : it->second;
}

void TestRegistrationObserver::OnRegistrationCompleted(const GURL& scope) {
  ++registrations_completed_map_[scope];
}

void TestRegistrationObserver::OnRegistrationStored(int64_t registration_id,
                                                    const GURL& scope) {
  if (scope.SchemeIs(kExtensionScheme)) {
    stored_run_loop_.Quit();
  }
}

void TestRegistrationObserver::OnVersionStartedRunning(
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  running_version_id_ = version_id;
  started_run_loop_.Quit();
}

void TestRegistrationObserver::OnVersionActivated(int64_t version_id,
                                                  const GURL& scope) {
  activated_run_loop_.Quit();
}

void TestRegistrationObserver::OnDestruct(
    content::ServiceWorkerContext* context) {
  context_->RemoveObserver(this);
  context_ = nullptr;
}

// UnregisterWorkerObserver ----------------------------------------------------
UnregisterWorkerObserver::UnregisterWorkerObserver(
    ProcessManager* process_manager,
    const ExtensionId& extension_id)
    : extension_id_(extension_id) {
  observation_.Observe(process_manager);
}

UnregisterWorkerObserver::~UnregisterWorkerObserver() = default;

void UnregisterWorkerObserver::OnServiceWorkerUnregistered(
    const WorkerId& worker_id) {
  run_loop_.QuitWhenIdle();
}

void UnregisterWorkerObserver::WaitForUnregister() {
  run_loop_.Run();
}

}  // namespace service_worker_test_utils
}  // namespace extensions
