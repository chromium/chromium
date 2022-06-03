// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_background_sync_manager.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TestBackgroundSyncManager::TestBackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context)
    : BackgroundSyncManager(service_worker_context,
                            std::move(devtools_context)) {}

TestBackgroundSyncManager::~TestBackgroundSyncManager() {}

void TestBackgroundSyncManager::DoInit() {
  Init();
}

void TestBackgroundSyncManager::ResumeBackendOperation() {
  ASSERT_TRUE(continuation_);
  std::move(continuation_).Run();
}

void TestBackgroundSyncManager::StoreDataInBackend(
    int64_t sw_registration_id,
    const url::Origin& origin,
    const std::string& key,
    const std::string& data,
    ServiceWorkerRegistry::StatusCallback callback) {
  EXPECT_FALSE(continuation_);
  if (corrupt_backend_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }
  continuation_ =
      base::BindOnce(&TestBackgroundSyncManager::StoreDataInBackendContinue,
                     base::Unretained(this), sw_registration_id, origin, key,
                     data, std::move(callback));
  if (delay_backend_)
    return;

  ResumeBackendOperation();
}

void TestBackgroundSyncManager::GetDataFromBackend(
    const std::string& key,
    ServiceWorkerRegistry::GetUserDataForAllRegistrationsCallback callback) {
  EXPECT_FALSE(continuation_);
  if (corrupt_backend_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<std::pair<int64_t, std::string>>(),
                       blink::ServiceWorkerStatusCode::kErrorFailed));
    return;
  }
  continuation_ =
      base::BindOnce(&TestBackgroundSyncManager::GetDataFromBackendContinue,
                     base::Unretained(this), key, std::move(callback));
  if (delay_backend_)
    return;

  ResumeBackendOperation();
}

void TestBackgroundSyncManager::DispatchSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    bool last_chance,
    ServiceWorkerVersion::StatusCallback callback) {
  ASSERT_TRUE(dispatch_sync_callback_);
  last_chance_ = last_chance;
  dispatch_sync_callback_.Run(active_version, std::move(callback));
}

void TestBackgroundSyncManager::DispatchPeriodicSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    ServiceWorkerVersion::StatusCallback callback) {
  ASSERT_TRUE(dispatch_periodic_sync_callback_);
  dispatch_periodic_sync_callback_.Run(active_version, std::move(callback));
}

void TestBackgroundSyncManager::HasMainFrameWindowClient(
    const url::Origin& origin,
    BoolCallback callback) {
  std::move(callback).Run(has_main_frame_window_client_);
}

void TestBackgroundSyncManager::FireReadyEvents(
    blink::mojom::BackgroundSyncType sync_type,
    bool reschedule,
    base::OnceClosure callback,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive) {
  if (dont_fire_sync_events_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  } else {
    BackgroundSyncManager::FireReadyEvents(
        sync_type, reschedule, std::move(callback), std::move(keepalive));
  }
}

void TestBackgroundSyncManager::StoreDataInBackendContinue(
    int64_t sw_registration_id,
    const url::Origin& origin,
    const std::string& key,
    const std::string& data,
    ServiceWorkerRegistry::StatusCallback callback) {
  BackgroundSyncManager::StoreDataInBackend(sw_registration_id, origin, key,
                                            data, std::move(callback));
}

void TestBackgroundSyncManager::GetDataFromBackendContinue(
    const std::string& key,
    ServiceWorkerRegistry::GetUserDataForAllRegistrationsCallback callback) {
  BackgroundSyncManager::GetDataFromBackend(key, std::move(callback));
}

base::TimeDelta TestBackgroundSyncManager::GetSoonestWakeupDelta(
    blink::mojom::BackgroundSyncType sync_type,
    base::Time last_browser_wakeup_for_periodic_sync) {
  base::TimeDelta soonest_wakeup_delta =
      BackgroundSyncManager::GetSoonestWakeupDelta(
          sync_type, last_browser_wakeup_for_periodic_sync);
  if (sync_type == blink::mojom::BackgroundSyncType::ONE_SHOT)
    soonest_one_shot_sync_wakeup_delta_ = soonest_wakeup_delta;
  else
    soonest_periodic_sync_wakeup_delta_ = soonest_wakeup_delta;
  return soonest_wakeup_delta;
}

}  // namespace content
