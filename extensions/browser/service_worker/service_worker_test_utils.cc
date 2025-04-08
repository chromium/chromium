// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_test_utils.h"

#include <utility>

#include "base/containers/map_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"

namespace extensions {
namespace service_worker_test_utils {

content::ServiceWorkerContext* GetServiceWorkerContext(
    content::BrowserContext* browser_context) {
  return browser_context->GetDefaultStoragePartition()
      ->GetServiceWorkerContext();
}

namespace {

std::optional<GURL> GetScopeForExtensionID(
    std::optional<ExtensionId> extension_id) {
  if (!extension_id) {
    return std::nullopt;
  }

  return Extension::GetBaseURLFromExtensionId(*extension_id);
}

}  // namespace

// TestServiceWorkerContextObserver
// ----------------------------------------------------

TestServiceWorkerContextObserver::TestServiceWorkerContextObserver(
    content::ServiceWorkerContext* context,
    std::optional<ExtensionId> extension_id)
    : extension_scope_(GetScopeForExtensionID(std::move(extension_id))),
      context_(context) {
  scoped_observation_.Observe(context_);
}

TestServiceWorkerContextObserver::TestServiceWorkerContextObserver(
    content::BrowserContext* browser_context,
    std::optional<ExtensionId> extension_id)
    : extension_scope_(GetScopeForExtensionID(std::move(extension_id))),
      context_(GetServiceWorkerContext(browser_context)) {
  scoped_observation_.Observe(context_);
}

TestServiceWorkerContextObserver::~TestServiceWorkerContextObserver() = default;

void TestServiceWorkerContextObserver::WaitForRegistrationStored() {
  if (registration_stored_) {
    return;
  }

  base::RunLoop run_loop;
  stored_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

int64_t TestServiceWorkerContextObserver::WaitForWorkerStarted() {
  if (!running_version_id_) {
    base::RunLoop run_loop;
    started_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  return *running_version_id_;
}

int64_t TestServiceWorkerContextObserver::WaitForWorkerStopped() {
  if (!running_version_id_) {
    return blink::mojom::kInvalidServiceWorkerVersionId;
  } else if (stopped_version_id_) {
    return *stopped_version_id_;
  }

  base::RunLoop run_loop;
  stopped_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  return *stopped_version_id_;
}

int64_t TestServiceWorkerContextObserver::WaitForWorkerActivated() {
  if (!activated_version_id_) {
    base::RunLoop run_loop;
    activated_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  return *activated_version_id_;
}

int TestServiceWorkerContextObserver::GetCompletedCount(
    const GURL& scope) const {
  const auto it = registrations_completed_map_.find(scope);
  return it == registrations_completed_map_.end() ? 0 : it->second;
}

void TestServiceWorkerContextObserver::OnRegistrationCompleted(
    const GURL& scope) {
  ++registrations_completed_map_[scope];
}

void TestServiceWorkerContextObserver::OnRegistrationStored(
    int64_t registration_id,
    const GURL& scope) {
  if (scope.SchemeIs(kExtensionScheme)) {
    registration_stored_ = true;
    if (stored_quit_closure_) {
      std::move(stored_quit_closure_).Run();
    }
  }
}

void TestServiceWorkerContextObserver::OnVersionStartedRunning(
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  if (extension_scope_ && extension_scope_ != running_info.scope) {
    return;
  }

  running_version_id_ = version_id;
  if (started_quit_closure_) {
    std::move(started_quit_closure_).Run();
  }
}

void TestServiceWorkerContextObserver::OnVersionStoppedRunning(
    int64_t version_id) {
  if (running_version_id_ && running_version_id_ == version_id) {
    stopped_version_id_ = version_id;
    if (stopped_quit_closure_) {
      std::move(stopped_quit_closure_).Run();
    }
  }
}

void TestServiceWorkerContextObserver::OnVersionActivated(int64_t version_id,
                                                          const GURL& scope) {
  if (extension_scope_ && extension_scope_ != scope) {
    return;
  }

  activated_version_id_ = version_id;
  if (activated_quit_closure_) {
    std::move(activated_quit_closure_).Run();
  }
}

void TestServiceWorkerContextObserver::OnDestruct(
    content::ServiceWorkerContext* context) {
  scoped_observation_.Reset();
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

void UnregisterWorkerObserver::OnStoppedTrackingServiceWorkerInstance(
    const WorkerId& worker_id) {
  run_loop_.QuitWhenIdle();
}

void UnregisterWorkerObserver::WaitForUnregister() {
  run_loop_.Run();
}

TestServiceWorkerTaskQueueObserver::TestServiceWorkerTaskQueueObserver() {
  ServiceWorkerTaskQueue::SetObserverForTest(this);
}

TestServiceWorkerTaskQueueObserver::~TestServiceWorkerTaskQueueObserver() {
  ServiceWorkerTaskQueue::SetObserverForTest(nullptr);
}

void TestServiceWorkerTaskQueueObserver::WaitForWorkerStarted(
    const ExtensionId& extension_id) {
  if (started_set_.count(extension_id) != 0) {
    return;
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestServiceWorkerTaskQueueObserver::WaitForWorkerStopped(
    const ExtensionId& extension_id) {
  if (stopped_set_.count(extension_id) != 0) {
    return;
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestServiceWorkerTaskQueueObserver::WaitForWorkerContextInitialized(
    const ExtensionId& extension_id) {
  if (inited_set_.count(extension_id) != 0) {
    return;
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

TestServiceWorkerTaskQueueObserver::WorkerStartFailedData
TestServiceWorkerTaskQueueObserver::WaitForDidStartWorkerFail(
    const ExtensionId& extension_id) {
  const WorkerStartFailedData* const data =
      base::FindOrNull(failed_map_, extension_id);
  if (data) {
    return *data;
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  return failed_map_[extension_id];
}

void TestServiceWorkerTaskQueueObserver::WaitForOnActivateExtension(
    const ExtensionId& extension_id) {
  if (activated_map_.count(extension_id) == 1) {
    return;
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

bool TestServiceWorkerTaskQueueObserver::WaitForRegistrationMismatchMitigation(
    const ExtensionId& extension_id) {
  const bool* const value = base::FindOrNull(mitigated_map_, extension_id);
  if (value) {
    return *value;
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  return mitigated_map_[extension_id];
}

std::optional<bool>
TestServiceWorkerTaskQueueObserver::WillRegisterServiceWorker(
    const ExtensionId& extension_id) const {
  const bool* const value = base::FindOrNull(activated_map_, extension_id);
  if (value) {
    return *value;
  }
  return std::nullopt;
}

int TestServiceWorkerTaskQueueObserver::GetRequestedWorkerStartedCount(
    const ExtensionId& extension_id) const {
  const int* const value =
      base::FindOrNull(requested_worker_started_map_, extension_id);
  return value ? *value : 0;
}

void TestServiceWorkerTaskQueueObserver::DidStartWorker(
    const ExtensionId& extension_id) {
  started_set_.insert(extension_id);
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

void TestServiceWorkerTaskQueueObserver::DidStartWorkerFail(
    const ExtensionId& extension_id,
    size_t num_pending_tasks,
    blink::ServiceWorkerStatusCode status_code) {
  WorkerStartFailedData& data = failed_map_[extension_id];
  data.num_pending_tasks = num_pending_tasks;
  data.status_code = status_code;
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

void TestServiceWorkerTaskQueueObserver::DidInitializeServiceWorkerContext(
    const ExtensionId& extension_id) {
  inited_set_.insert(extension_id);
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

void TestServiceWorkerTaskQueueObserver::OnActivateExtension(
    const ExtensionId& extension_id,
    bool will_register_service_worker) {
  activated_map_[extension_id] = will_register_service_worker;
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

void TestServiceWorkerTaskQueueObserver::RegistrationMismatchMitigated(
    const ExtensionId& extension_id,
    bool success) {
  mitigated_map_[extension_id] = success;
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

void TestServiceWorkerTaskQueueObserver::RequestedWorkerStart(
    const ExtensionId& extension_id) {
  ++requested_worker_started_map_[extension_id];
}

void TestServiceWorkerTaskQueueObserver::DidStopServiceWorkerContext(
    const ExtensionId& extension_id) {
  stopped_set_.insert(extension_id);
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

}  // namespace service_worker_test_utils
}  // namespace extensions
