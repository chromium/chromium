// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/browser/background_sync/background_sync_launcher.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/one_shot_background_sync_service_impl.h"
#include "content/browser/background_sync/periodic_background_sync_service_impl.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"
#include "url/origin.h"

namespace content {

BackgroundSyncContextImpl::BackgroundSyncContextImpl()
    : base::RefCountedDeleteOnSequence<BackgroundSyncContextImpl>(
          base::CreateSingleThreadTaskRunner(
              {ServiceWorkerContext::GetCoreThreadId()})),
      test_wakeup_delta_(
          {{blink::mojom::BackgroundSyncType::ONE_SHOT, base::TimeDelta::Max()},
           {blink::mojom::BackgroundSyncType::PERIODIC,
            base::TimeDelta::Max()}}) {}

BackgroundSyncContextImpl::~BackgroundSyncContextImpl() {
  // The destructor must run on the core thread because it implicitly accesses
  // background_sync_manager_ and services_, when it runs their destructors.
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  DCHECK(!background_sync_manager_);
  DCHECK(one_shot_sync_services_.empty());
  DCHECK(periodic_sync_services_.empty());
}

// static
#if defined(OS_ANDROID)
void BackgroundSyncContext::FireBackgroundSyncEventsAcrossPartitions(
    BrowserContext* browser_context,
    blink::mojom::BackgroundSyncType sync_type,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);
  BackgroundSyncLauncher::FireBackgroundSyncEvents(browser_context, sync_type,
                                                   j_runnable);
}
#endif

// static
void BackgroundSyncContext::GetSoonestWakeupDeltaAcrossPartitions(
    blink::mojom::BackgroundSyncType sync_type,
    BrowserContext* browser_context,
    base::OnceCallback<void(base::TimeDelta)> callback) {
  DCHECK(browser_context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncLauncher::GetSoonestWakeupDelta(sync_type, browser_context,
                                                std::move(callback));
}

void BackgroundSyncContextImpl::Init(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<DevToolsBackgroundServicesContextImpl>&
        devtools_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundSyncContextImpl::CreateBackgroundSyncManager,
                     this, service_worker_context, devtools_context));
}

void BackgroundSyncContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundSyncContextImpl::ShutdownOnCoreThread, this));
}

void BackgroundSyncContextImpl::CreateOneShotSyncService(
    mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
        receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &BackgroundSyncContextImpl::CreateOneShotSyncServiceOnCoreThread,
          this, std::move(receiver)));
}

void BackgroundSyncContextImpl::CreatePeriodicSyncService(
    mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
        receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &BackgroundSyncContextImpl::CreatePeriodicSyncServiceOnCoreThread,
          this, std::move(receiver)));
}

void BackgroundSyncContextImpl::OneShotSyncServiceHadConnectionError(
    OneShotBackgroundSyncServiceImpl* service) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(service);

  auto iter = one_shot_sync_services_.find(service);
  DCHECK(iter != one_shot_sync_services_.end());
  one_shot_sync_services_.erase(iter);
}

void BackgroundSyncContextImpl::PeriodicSyncServiceHadConnectionError(
    PeriodicBackgroundSyncServiceImpl* service) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(service);

  auto iter = periodic_sync_services_.find(service);
  DCHECK(iter != periodic_sync_services_.end());
  periodic_sync_services_.erase(iter);
}

BackgroundSyncManager* BackgroundSyncContextImpl::background_sync_manager()
    const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  return background_sync_manager_.get();
}

void BackgroundSyncContextImpl::set_background_sync_manager_for_testing(
    std::unique_ptr<BackgroundSyncManager> manager) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  background_sync_manager_ = std::move(manager);
}

void BackgroundSyncContextImpl::set_wakeup_delta_for_testing(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta wakeup_delta) {
  test_wakeup_delta_[sync_type] = wakeup_delta;
}

void BackgroundSyncContextImpl::GetSoonestWakeupDelta(
    blink::mojom::BackgroundSyncType sync_type,
    base::Time last_browser_wakeup_for_periodic_sync,
    base::OnceCallback<void(base::TimeDelta)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/824858): Remove the else branch after the feature is
  // enabled. Also, try to make a RunOrPostTaskOnThreadAndReplyWithResult()
  // function so the if/else isn't needed.
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    base::TimeDelta delta = GetSoonestWakeupDeltaOnCoreThread(
        sync_type, last_browser_wakeup_for_periodic_sync);
    DidGetSoonestWakeupDelta(std::move(callback), delta);
  } else {
    base::PostTaskAndReplyWithResult(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(
            &BackgroundSyncContextImpl::GetSoonestWakeupDeltaOnCoreThread, this,
            sync_type, last_browser_wakeup_for_periodic_sync),
        base::BindOnce(&BackgroundSyncContextImpl::DidGetSoonestWakeupDelta,
                       this, std::move(callback)));
  }
}

void BackgroundSyncContextImpl::RevivePeriodicBackgroundSyncRegistrations(
    url::Origin origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundSyncContextImpl::
                         RevivePeriodicBackgroundSyncRegistrationsOnCoreThread,
                     this, std::move(origin)));
}

base::TimeDelta BackgroundSyncContextImpl::GetSoonestWakeupDeltaOnCoreThread(
    blink::mojom::BackgroundSyncType sync_type,
    base::Time last_browser_wakeup_for_periodic_sync) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  auto test_wakeup_delta = test_wakeup_delta_[sync_type];
  if (!test_wakeup_delta.is_max())
    return test_wakeup_delta;
  if (!background_sync_manager_)
    return base::TimeDelta::Max();

  return background_sync_manager_->GetSoonestWakeupDelta(
      sync_type, last_browser_wakeup_for_periodic_sync);
}

void BackgroundSyncContextImpl::DidGetSoonestWakeupDelta(
    base::OnceCallback<void(base::TimeDelta)> callback,
    base::TimeDelta soonest_wakeup_delta) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(callback).Run(soonest_wakeup_delta);
}

void BackgroundSyncContextImpl::
    RevivePeriodicBackgroundSyncRegistrationsOnCoreThread(url::Origin origin) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (!background_sync_manager_)
    return;

  background_sync_manager_->RevivePeriodicSyncRegistrations(std::move(origin));
}

void BackgroundSyncContextImpl::FireBackgroundSyncEvents(
    blink::mojom::BackgroundSyncType sync_type,
    base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &BackgroundSyncContextImpl::FireBackgroundSyncEventsOnCoreThread,
          this, sync_type, std::move(done_closure)));
}

void BackgroundSyncContextImpl::FireBackgroundSyncEventsOnCoreThread(
    blink::mojom::BackgroundSyncType sync_type,
    base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (!background_sync_manager_) {
    DidFireBackgroundSyncEventsOnCoreThread(std::move(done_closure));
    return;
  }

  background_sync_manager_->FireReadyEvents(
      sync_type, /* reschedule= */ false,
      base::BindOnce(
          &BackgroundSyncContextImpl::DidFireBackgroundSyncEventsOnCoreThread,
          this, std::move(done_closure)));
}

void BackgroundSyncContextImpl::DidFireBackgroundSyncEventsOnCoreThread(
    base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // Use PostTask() rather than RunOrPostTaskOnThread() to ensure the callback
  // is called asynchronously.
  base::PostTask(FROM_HERE, {BrowserThread::UI}, std::move(done_closure));
}

void BackgroundSyncContextImpl::CreateBackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!background_sync_manager_);

  background_sync_manager_ = BackgroundSyncManager::Create(
      std::move(service_worker_context), std::move(devtools_context));
}

void BackgroundSyncContextImpl::CreateOneShotSyncServiceOnCoreThread(
    mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
        receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(background_sync_manager_);
  one_shot_sync_services_.insert(
      std::make_unique<OneShotBackgroundSyncServiceImpl>(this,
                                                         std::move(receiver)));
}

void BackgroundSyncContextImpl::CreatePeriodicSyncServiceOnCoreThread(
    mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
        receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(background_sync_manager_);
  periodic_sync_services_.insert(
      std::make_unique<PeriodicBackgroundSyncServiceImpl>(this,
                                                          std::move(receiver)));
}

void BackgroundSyncContextImpl::ShutdownOnCoreThread() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  one_shot_sync_services_.clear();
  periodic_sync_services_.clear();
  background_sync_manager_.reset();
}

}  // namespace content
