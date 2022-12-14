// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/test_service_worker_observer.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"

namespace content {

TestServiceWorkerObserver::TestServiceWorkerObserver(
    scoped_refptr<ServiceWorkerContextWrapper> wrapper)
    : wrapper_(std::move(wrapper)) {
  wrapper_->AddObserver(this);
}

TestServiceWorkerObserver::~TestServiceWorkerObserver() {
  wrapper_->RemoveObserver(this);
}

void TestServiceWorkerObserver::RunUntilStatusChange(
    ServiceWorkerVersion* version,
    ServiceWorkerVersion::Status status) {
  if (version->status() == status)
    return;

  base::RunLoop loop;
  version_id_for_status_change_ = version->version_id();
  status_for_status_change_ = status;
  DCHECK(!quit_closure_for_status_change_);
  quit_closure_for_status_change_ = loop.QuitClosure();
  loop.Run();
}

void TestServiceWorkerObserver::RunUntilActivated(
    ServiceWorkerVersion* version,
    scoped_refptr<base::TestSimpleTaskRunner> runner) {
  if (version->status() == ServiceWorkerVersion::ACTIVATED)
    return;

  // Call runner->RunUntilIdle() to skip the delay for the activate event in
  // ServiceWorkerRegistration.
  RunUntilStatusChange(version, ServiceWorkerVersion::ACTIVATING);
  runner->RunUntilIdle();
  RunUntilStatusChange(version, ServiceWorkerVersion::ACTIVATED);
}

void TestServiceWorkerObserver::RunUntilLiveVersion() {
  if (!wrapper_->GetAllLiveVersionInfo().empty())
    return;

  base::RunLoop loop;
  DCHECK(!quit_closure_for_live_version_);
  quit_closure_for_live_version_ = loop.QuitClosure();
  loop.Run();
}

void TestServiceWorkerObserver::OnVersionStateChanged(
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    ServiceWorkerVersion::Status status) {
  if (version_id == version_id_for_status_change_ &&
      status == status_for_status_change_ && quit_closure_for_status_change_) {
    std::move(quit_closure_for_status_change_).Run();
  }
}

void TestServiceWorkerObserver::OnNewLiveVersion(
    const ServiceWorkerVersionInfo& version_info) {
  if (quit_closure_for_live_version_)
    std::move(quit_closure_for_live_version_).Run();
}

}  // namespace content
