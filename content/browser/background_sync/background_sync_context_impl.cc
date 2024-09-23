// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_context_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
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
    : test_wakeup_delta_(
          {{blink::mojom::BackgroundSyncType::ONE_SHOT, base::TimeDelta::Max()},
           {blink::mojom::BackgroundSyncType::PERIODIC,
            base::TimeDelta::Max()}}) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

BackgroundSyncContextImpl::~BackgroundSyncContextImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!background_sync_manager_);
  DCHECK(one_shot_sync_services_.empty());
  DCHECK(periodic_sync_services_.empty());
}

// static
#if BUILDFLAG(IS_ANDROID)
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

void BackgroundSyncContextImpl::Init(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    DevToolsBackgroundServicesContextImpl& devtools_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CreateBackgroundSyncManager(service_worker_context, devtools_context);
}

void BackgroundSyncContextImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  one_shot_sync_services_.clear();
  periodic_sync_services_.clear();
  background_sync_manager_.reset();
}

void BackgroundSyncContextImpl::CreateOneShotSyncService(
    const url::Origin& origin,
    RenderProcessHost* render_process_host,
    mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(background_sync_manager_);
  one_shot_sync_services_.insert(
      std::make_unique<OneShotBackgroundSyncServiceImpl>(
          this, origin, render_process_host, std::move(receiver)));
}

void BackgroundSyncContextImpl::CreatePeriodicSyncService(
    const url::Origin& origin,
    RenderProcessHost* render_process_host,
    mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(background_sync_manager_);
  periodic_sync_services_.insert(
      std::make_unique<PeriodicBackgroundSyncServiceImpl>(
          this, origin, render_process_host, std::move(receiver)));
}

void BackgroundSyncContextImpl::OneShotSyncServiceHadConnectionError(
    OneShotBackgroundSyncServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service);

  auto iter = one_shot_sync_services_.find(service);
  CHECK(iter != one_shot_sync_services_.end(), base::NotFatalUntil::M130);
  one_shot_sync_services_.erase(iter);
}

void BackgroundSyncContextImpl::PeriodicSyncServiceHadConnectionError(
    PeriodicBackgroundSyncServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service);

  auto iter = periodic_sync_services_.find(service);
  CHECK(iter != periodic_sync_services_.end(), base::NotFatalUntil::M130);
  periodic_sync_services_.erase(iter);
}

BackgroundSyncManager* BackgroundSyncContextImpl::background_sync_manager()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return background_sync_manager_.get();
}

void BackgroundSyncContextImpl::set_background_sync_manager_for_testing(
    std::unique_ptr<BackgroundSyncManager> manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  background_sync_manager_ = std::move(manager);
}

void BackgroundSyncContextImpl::set_wakeup_delta_for_testing(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta wakeup_delta) {
  test_wakeup_delta_[sync_type] = wakeup_delta;
}

base::TimeDelta BackgroundSyncContextImpl::GetSoonestWakeupDelta(
    blink::mojom::BackgroundSyncType sync_type,
    base::Time last_browser_wakeup_for_periodic_sync) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto test_wakeup_delta = test_wakeup_delta_[sync_type];
  if (!test_wakeup_delta.is_max())
    return test_wakeup_delta;
  if (!background_sync_manager_)
    return base::TimeDelta::Max();

  return background_sync_manager_->GetSoonestWakeupDelta(
      sync_type, last_browser_wakeup_for_periodic_sync);
}

void BackgroundSyncContextImpl::RevivePeriodicBackgroundSyncRegistrations(
    url::Origin origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!background_sync_manager_)
    return;

  background_sync_manager_->RevivePeriodicSyncRegistrations(std::move(origin));
}

void BackgroundSyncContextImpl::UnregisterPeriodicSyncForOrigin(
    url::Origin origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!background_sync_manager_)
    return;

  background_sync_manager_->UnregisterPeriodicSyncForOrigin(origin);
}

void BackgroundSyncContextImpl::FireBackgroundSyncEvents(
    blink::mojom::BackgroundSyncType sync_type,
    base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!background_sync_manager_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(done_closure));
    return;
  }

  background_sync_manager_->FireReadyEvents(sync_type, /* reschedule= */ false,
                                            std::move(done_closure));
}

void BackgroundSyncContextImpl::CreateBackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    DevToolsBackgroundServicesContextImpl& devtools_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!background_sync_manager_);

  background_sync_manager_ = BackgroundSyncManager::Create(
      std::move(service_worker_context), devtools_context);
}

}  // namespace content
