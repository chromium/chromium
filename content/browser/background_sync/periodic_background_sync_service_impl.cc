// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/periodic_background_sync_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PeriodicBackgroundSyncServiceImpl::PeriodicBackgroundSyncServiceImpl(
    BackgroundSyncContextImpl* background_sync_context,
    mojo::InterfaceRequest<blink::mojom::PeriodicBackgroundSyncService> request)
    : background_sync_context_(background_sync_context),
      binding_(this, std::move(request)) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(background_sync_context_);

  registration_helper_ = std::make_unique<BackgroundSyncRegistrationHelper>(
      background_sync_context_);
  DCHECK(registration_helper_);

  binding_.set_connection_error_handler(base::BindOnce(
      &PeriodicBackgroundSyncServiceImpl::OnConnectionError,
      base::Unretained(this) /* the channel is owned by this */));
}

PeriodicBackgroundSyncServiceImpl::~PeriodicBackgroundSyncServiceImpl() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

void PeriodicBackgroundSyncServiceImpl::OnConnectionError() {
  background_sync_context_->PeriodicSyncServiceHadConnectionError(this);
  // |this| is now deleted.
}

void PeriodicBackgroundSyncServiceImpl::Register(
    blink::mojom::SyncRegistrationOptionsPtr options,
    int64_t sw_registration_id,
    RegisterCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(options);

  if (options->min_interval < 0) {
    registration_helper_->NotifyInvalidOptionsProvided(std::move(callback));
    return;
  }

  registration_helper_->Register(std::move(options), sw_registration_id,
                                 std::move(callback));
}

void PeriodicBackgroundSyncServiceImpl::Unregister(
    int64_t sw_registration_id,
    const std::string& tag,
    UnregisterCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->UnregisterPeriodicSync(
      sw_registration_id, tag,
      base::BindOnce(&PeriodicBackgroundSyncServiceImpl::OnUnregisterResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PeriodicBackgroundSyncServiceImpl::GetRegistrations(
    int64_t sw_registration_id,
    GetRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);

  // BackgroundSyncContextImpl owns both PeriodicBackgroundSyncServiceImpl and
  // BackgroundSyncManager. The manager will be destroyed after the service,
  // thus passing the |registration_helper_| pointer is safe here.
  background_sync_manager->GetPeriodicSyncRegistrations(
      sw_registration_id,
      base::BindOnce(
          &BackgroundSyncRegistrationHelper::OnGetRegistrationsResult,
          registration_helper_->GetWeakPtr(), std::move(callback)));
}

void PeriodicBackgroundSyncServiceImpl::OnUnregisterResult(
    UnregisterCallback callback,
    BackgroundSyncStatus status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  std::move(callback).Run(
      static_cast<blink::mojom::BackgroundSyncError>(status));
}

}  // namespace content
