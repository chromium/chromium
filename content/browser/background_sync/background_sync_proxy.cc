// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_proxy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "content/browser/background_sync/background_sync_scheduler.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BackgroundSyncProxy::BackgroundSyncProxy(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(std::move(service_worker_context)) {
  // This class lives on the UI thread. Check explicitly that it is on the UI
  // thread in the constructor, and use the `sequence_checker_` in
  // each method to check that it is on a single sequence (the UI thread).
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_worker_context_);
}

BackgroundSyncProxy::~BackgroundSyncProxy() = default;

void BackgroundSyncProxy::ScheduleDelayedProcessing(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay,
    base::OnceClosure delayed_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Schedule Chrome wakeup.
  if (!browser_context())
    return;

  auto* scheduler = BackgroundSyncScheduler::GetFor(browser_context());
  DCHECK(scheduler);
  DCHECK(delay != base::TimeDelta::Max());

  scheduler->ScheduleDelayedProcessing(
      service_worker_context_->storage_partition(), sync_type, delay,
      std::move(delayed_task));
}

void BackgroundSyncProxy::CancelDelayedProcessing(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!browser_context())
    return;

  auto* scheduler = BackgroundSyncScheduler::GetFor(browser_context());
  DCHECK(scheduler);

  scheduler->CancelDelayedProcessing(
      service_worker_context_->storage_partition(), sync_type);
}

void BackgroundSyncProxy::SendSuspendedPeriodicSyncOrigins(
    std::set<url::Origin> suspended_origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!browser_context())
    return;

  auto* controller = browser_context()->GetBackgroundSyncController();
  DCHECK(controller);

  controller->NoteSuspendedPeriodicSyncOrigins(std::move(suspended_origins));
}

void BackgroundSyncProxy::SendRegisteredPeriodicSyncOrigins(
    std::set<url::Origin> registered_origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!browser_context())
    return;

  auto* controller = browser_context()->GetBackgroundSyncController();
  DCHECK(controller);

  controller->NoteRegisteredPeriodicSyncOrigins(std::move(registered_origins));
}

void BackgroundSyncProxy::AddToTrackedOrigins(url::Origin origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context())
    return;

  auto* controller = browser_context()->GetBackgroundSyncController();
  DCHECK(controller);

  controller->AddToTrackedOrigins(origin);
}

void BackgroundSyncProxy::RemoveFromTrackedOrigins(url::Origin origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!browser_context())
    return;

  auto* controller = browser_context()->GetBackgroundSyncController();
  DCHECK(controller);

  controller->RemoveFromTrackedOrigins(origin);
}

BrowserContext* BackgroundSyncProxy::browser_context() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!service_worker_context_)
    return nullptr;

  StoragePartitionImpl* storage_partition_impl =
      service_worker_context_->storage_partition();
  if (!storage_partition_impl)  // may be null in tests
    return nullptr;

  return storage_partition_impl->browser_context();
}

}  // namespace content
