// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/browser/background_sync/background_sync_scheduler.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"

namespace content {

class BackgroundSyncProxy::Core {
 public:
  Core(const base::WeakPtr<BackgroundSyncProxy>& io_parent,
       scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
      : io_parent_(io_parent),
        service_worker_context_(std::move(service_worker_context)),
        weak_ptr_factory_(this) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    DCHECK(service_worker_context_);
  }

  ~Core() { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  base::WeakPtr<Core> GetWeakPtrOnCoreThread() {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    return weak_ptr_factory_.GetWeakPtr();
  }

  BrowserContext* browser_context() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!service_worker_context_)
      return nullptr;

    StoragePartitionImpl* storage_partition_impl =
        service_worker_context_->storage_partition();
    if (!storage_partition_impl)  // may be null in tests
      return nullptr;

    return storage_partition_impl->browser_context();
  }

  void ScheduleDelayedProcessing(blink::mojom::BackgroundSyncType sync_type,
                                 base::TimeDelta delay,
                                 base::OnceClosure delayed_task) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (!browser_context())
      return;

    auto* scheduler = BackgroundSyncScheduler::GetFor(browser_context());
    DCHECK(scheduler);
    DCHECK(delay != base::TimeDelta::Max());

    scheduler->ScheduleDelayedProcessing(
        service_worker_context_->storage_partition(), sync_type, delay,
        base::BindOnce(
            [](base::OnceClosure delayed_task) {
              RunOrPostTaskOnThread(FROM_HERE,
                                    ServiceWorkerContext::GetCoreThreadId(),
                                    std::move(delayed_task));
            },
            std::move(delayed_task)));
  }

  void CancelDelayedProcessing(blink::mojom::BackgroundSyncType sync_type) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (!browser_context())
      return;

    auto* scheduler = BackgroundSyncScheduler::GetFor(browser_context());
    DCHECK(scheduler);

    scheduler->CancelDelayedProcessing(
        service_worker_context_->storage_partition(), sync_type);
  }

  void SendSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!browser_context())
      return;

    auto* controller = browser_context()->GetBackgroundSyncController();
    DCHECK(controller);

    controller->NoteSuspendedPeriodicSyncOrigins(std::move(suspended_origins));
  }

 private:
  base::WeakPtr<BackgroundSyncProxy> io_parent_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  base::WeakPtrFactory<Core> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Core);
};

BackgroundSyncProxy::BackgroundSyncProxy(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(service_worker_context);
  ui_core_ = std::unique_ptr<Core, BrowserThread::DeleteOnUIThread>(new Core(
      weak_ptr_factory_.GetWeakPtr(), std::move(service_worker_context)));
  ui_core_weak_ptr_ = ui_core_->GetWeakPtrOnCoreThread();
}

BackgroundSyncProxy::~BackgroundSyncProxy() = default;

void BackgroundSyncProxy::ScheduleDelayedProcessing(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay,
    base::OnceClosure delayed_task) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // Schedule Chrome wakeup.
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&Core::ScheduleDelayedProcessing, ui_core_weak_ptr_,
                     sync_type, delay, std::move(delayed_task)));
}

void BackgroundSyncProxy::CancelDelayedProcessing(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(&Core::CancelDelayedProcessing,
                                       ui_core_weak_ptr_, sync_type));
}

void BackgroundSyncProxy::SendSuspendedPeriodicSyncOrigins(
    std::set<url::Origin> suspended_origins) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&Core::SendSuspendedPeriodicSyncOrigins, ui_core_weak_ptr_,
                     std::move(suspended_origins)));
}

}  // namespace content
