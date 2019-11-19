// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_registration_helper.h"

#include "base/memory/weak_ptr.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BackgroundSyncRegistrationHelper::BackgroundSyncRegistrationHelper(
    BackgroundSyncContextImpl* background_sync_context)
    : background_sync_context_(background_sync_context) {
  DCHECK(background_sync_context_);
}

BackgroundSyncRegistrationHelper::~BackgroundSyncRegistrationHelper() = default;

void BackgroundSyncRegistrationHelper::Register(
    blink::mojom::SyncRegistrationOptionsPtr options,
    int64_t sw_registration_id,
    RegisterCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);

  background_sync_manager->Register(
      sw_registration_id, *options,
      base::BindOnce(&BackgroundSyncRegistrationHelper::OnRegisterResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncRegistrationHelper::DidResolveRegistration(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);

  background_sync_manager->DidResolveRegistration(std::move(registration_info));
}

void BackgroundSyncRegistrationHelper::OnRegisterResult(
    RegisterCallback callback,
    BackgroundSyncStatus status,
    std::unique_ptr<BackgroundSyncRegistration> result) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // TODO(crbug.com/932591): Use blink::mojom::BackgroundSyncError
  // directly.
  if (status != BACKGROUND_SYNC_STATUS_OK) {
    std::move(callback).Run(
        static_cast<blink::mojom::BackgroundSyncError>(status),
        /* options= */ nullptr);
    return;
  }

  DCHECK(result);
  std::move(callback).Run(
      static_cast<blink::mojom::BackgroundSyncError>(status),
      result->options()->Clone());
}

void BackgroundSyncRegistrationHelper::NotifyInvalidOptionsProvided(
    RegisterCallback callback) const {
  mojo::ReportBadMessage(
      "BackgroundSyncRegistrationHelper: Invalid options passed.");
  std::move(callback).Run(blink::mojom::BackgroundSyncError::NOT_ALLOWED,
                          /* options= */ nullptr);
}

void BackgroundSyncRegistrationHelper::OnGetRegistrationsResult(
    GetRegistrationsCallback callback,
    BackgroundSyncStatus status,
    std::vector<std::unique_ptr<BackgroundSyncRegistration>>
        result_registrations) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  std::vector<blink::mojom::SyncRegistrationOptionsPtr> mojo_registrations;
  mojo_registrations.reserve(result_registrations.size());
  for (const auto& registration : result_registrations)
    mojo_registrations.push_back(registration->options()->Clone());

  std::move(callback).Run(
      static_cast<blink::mojom::BackgroundSyncError>(status),
      std::move(mojo_registrations));
}

base::WeakPtr<BackgroundSyncRegistrationHelper>
BackgroundSyncRegistrationHelper::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
