// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/one_shot_background_sync_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"

namespace content {

OneShotBackgroundSyncServiceImpl::OneShotBackgroundSyncServiceImpl(
    BackgroundSyncContextImpl* background_sync_context,
    const url::Origin& origin,
    RenderProcessHost* render_process_host,
    mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService> receiver)
    : background_sync_context_(background_sync_context),
      origin_(origin),
      receiver_(this, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(background_sync_context_);

  registration_helper_ = std::make_unique<BackgroundSyncRegistrationHelper>(
      background_sync_context_, render_process_host);

  receiver_.set_disconnect_handler(base::BindOnce(
      &OneShotBackgroundSyncServiceImpl::OnMojoDisconnect,
      base::Unretained(this) /* the channel is owned by |this| */));
}

OneShotBackgroundSyncServiceImpl::~OneShotBackgroundSyncServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void OneShotBackgroundSyncServiceImpl::OnMojoDisconnect() {
  background_sync_context_->OneShotSyncServiceHadConnectionError(this);
  // |this| is now deleted.
}

void OneShotBackgroundSyncServiceImpl::Register(
    blink::mojom::SyncRegistrationOptionsPtr options,
    int64_t sw_registration_id,
    RegisterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(options);

  if (options->min_interval != -1) {
    registration_helper_->NotifyInvalidOptionsProvided(std::move(callback));
    return;
  }

  if (!registration_helper_->ValidateSWRegistrationID(sw_registration_id,
                                                      origin_)) {
    std::move(callback).Run(blink::mojom::BackgroundSyncError::STORAGE,
                            /* registrations= */ nullptr);
    return;
  }

  registration_helper_->Register(std::move(options), sw_registration_id,
                                 std::move(callback));
}

void OneShotBackgroundSyncServiceImpl::DidResolveRegistration(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  registration_helper_->DidResolveRegistration(std::move(registration_info));
}

void OneShotBackgroundSyncServiceImpl::GetRegistrations(
    int64_t sw_registration_id,
    GetRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!registration_helper_->ValidateSWRegistrationID(sw_registration_id,
                                                      origin_)) {
    std::move(callback).Run(blink::mojom::BackgroundSyncError::STORAGE,
                            /* options= */ {});
    return;
  }

  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);

  background_sync_manager->GetOneShotSyncRegistrations(
      sw_registration_id,
      base::BindOnce(
          &BackgroundSyncRegistrationHelper::OnGetRegistrationsResult,
          registration_helper_->GetWeakPtr(), std::move(callback)));
}

}  // namespace content
