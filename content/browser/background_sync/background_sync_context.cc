// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_context.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_service_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BackgroundSyncContext::BackgroundSyncContext()
    : base::RefCountedDeleteOnSequence<BackgroundSyncContext>(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})) {}

BackgroundSyncContext::~BackgroundSyncContext() {
  // The destructor must run on the IO thread because it implicitly accesses
  // background_sync_manager_ and services_, when it runs their destructors.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!background_sync_manager_);
  DCHECK(services_.empty());
}

void BackgroundSyncContext::Init(
    const scoped_refptr<ServiceWorkerContextWrapper>& context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BackgroundSyncContext::CreateBackgroundSyncManager, this,
                     context));
}

void BackgroundSyncContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BackgroundSyncContext::ShutdownOnIO, this));
}

void BackgroundSyncContext::CreateService(
    blink::mojom::BackgroundSyncServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BackgroundSyncContext::CreateServiceOnIOThread, this,
                     std::move(request)));
}

void BackgroundSyncContext::ServiceHadConnectionError(
    BackgroundSyncServiceImpl* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::ContainsKey(services_, service));

  services_.erase(service);
}

BackgroundSyncManager* BackgroundSyncContext::background_sync_manager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return background_sync_manager_.get();
}

void BackgroundSyncContext::set_background_sync_manager_for_testing(
    std::unique_ptr<BackgroundSyncManager> manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  background_sync_manager_ = std::move(manager);
}

void BackgroundSyncContext::CreateBackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!background_sync_manager_);

  background_sync_manager_ = BackgroundSyncManager::Create(context);
}

void BackgroundSyncContext::CreateServiceOnIOThread(
    mojo::InterfaceRequest<blink::mojom::BackgroundSyncService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_manager_);
  auto service =
      std::make_unique<BackgroundSyncServiceImpl>(this, std::move(request));
  services_[service.get()] = std::move(service);
}

void BackgroundSyncContext::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  services_.clear();
  background_sync_manager_.reset();
}

}  // namespace content
