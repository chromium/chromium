// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota/quota_manager_host.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "content/browser/quota/quota_change_dispatcher.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {

QuotaManagerHost::QuotaManagerHost(
    int process_id,
    int render_frame_id,
    const blink::StorageKey& storage_key,
    storage::QuotaManager* quota_manager,
    QuotaPermissionContext* permission_context,
    scoped_refptr<QuotaChangeDispatcher> quota_change_dispatcher)
    : process_id_(process_id),
      render_frame_id_(render_frame_id),
      storage_key_(storage_key),
      quota_manager_(quota_manager),
      permission_context_(permission_context),
      quota_change_dispatcher_(std::move(quota_change_dispatcher)) {
  DCHECK(quota_manager);
  // TODO(sashab): Work out the conditions for permission_context to be set and
  // add a DCHECK for it here.
}

void QuotaManagerHost::AddChangeListener(
    mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener,
    AddChangeListenerCallback callback) {
  if (quota_change_dispatcher_) {
    quota_change_dispatcher_->AddChangeListener(storage_key_,
                                                std::move(mojo_listener));
  }
  std::move(callback).Run();
}

void QuotaManagerHost::QueryStorageUsageAndQuota(
    blink::mojom::StorageType storage_type,
    QueryStorageUsageAndQuotaCallback callback) {
  quota_manager_->GetUsageAndQuotaWithBreakdown(
      storage_key_, storage_type,
      base::BindOnce(&QuotaManagerHost::DidQueryStorageUsageAndQuota,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerHost::RequestStorageQuota(
    blink::mojom::StorageType storage_type,
    uint64_t requested_size,
    blink::mojom::QuotaManagerHost::RequestStorageQuotaCallback callback) {
  if (storage_type != blink::mojom::StorageType::kTemporary &&
      storage_type != blink::mojom::StorageType::kPersistent) {
    mojo::ReportBadMessage("Unsupported storage type specified.");
    return;
  }

  if (render_frame_id_ == MSG_ROUTING_NONE) {
    mojo::ReportBadMessage(
        "Requests may show permission UI and are not allowed from workers.");
    return;
  }

  if (storage_key_.origin().opaque()) {
    mojo::ReportBadMessage("Opaque origins may not request storage quota.");
    return;
  }

  DCHECK(storage_type == blink::mojom::StorageType::kTemporary ||
         storage_type == blink::mojom::StorageType::kPersistent);
  if (storage_type == blink::mojom::StorageType::kPersistent) {
    quota_manager_->GetUsageAndQuotaForWebApps(
        storage_key_, storage_type,
        base::BindOnce(&QuotaManagerHost::DidGetPersistentUsageAndQuota,
                       weak_factory_.GetWeakPtr(), storage_type, requested_size,
                       std::move(callback)));
  } else {
    quota_manager_->GetUsageAndQuotaForWebApps(
        storage_key_, storage_type,
        base::BindOnce(&QuotaManagerHost::DidGetTemporaryUsageAndQuota,
                       weak_factory_.GetWeakPtr(), requested_size,
                       std::move(callback)));
  }
}

void QuotaManagerHost::DidQueryStorageUsageAndQuota(
    QueryStorageUsageAndQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  std::move(callback).Run(status, usage, quota, std::move(usage_breakdown));
}

void QuotaManagerHost::DidGetPersistentUsageAndQuota(
    blink::mojom::StorageType storage_type,
    uint64_t requested_quota,
    RequestStorageQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t current_usage,
    int64_t current_quota) {
  if (status != blink::mojom::QuotaStatusCode::kOk) {
    std::move(callback).Run(status, 0, 0);
    return;
  }

  // If we have enough quota for the requested storage, we can just let it go.
  // Convert the requested size from uint64_t to int64_t since the quota backend
  // requires int64_t values.
  // TODO(nhiroki): The backend should accept uint64_t values.
  int64_t requested_quota_signed =
      base::saturated_cast<int64_t>(requested_quota);
  if (quota_manager_->IsStorageUnlimited(storage_key_, storage_type) ||
      requested_quota_signed <= current_quota) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, current_usage,
                            requested_quota);
    return;
  }

  // Otherwise we need to consult with the permission context and possibly show
  // a prompt.
  DCHECK(permission_context_);
  StorageQuotaParams params;
  params.render_frame_id = render_frame_id_;
  params.origin_url = storage_key_.origin().GetURL();
  params.storage_type = storage_type;
  params.requested_size = requested_quota;

  permission_context_->RequestQuotaPermission(
      params, process_id_,
      base::BindOnce(&QuotaManagerHost::DidGetPermissionResponse,
                     weak_factory_.GetWeakPtr(), requested_quota, current_usage,
                     current_quota, std::move(callback)));
}

void QuotaManagerHost::DidGetPermissionResponse(
    uint64_t requested_quota,
    int64_t current_usage,
    int64_t current_quota,
    RequestStorageQuotaCallback callback,
    QuotaPermissionContext::QuotaPermissionResponse response) {
  // If user didn't allow the new quota, just return the current quota.
  if (response != QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, current_usage,
                            current_quota);
    return;
  }

  // Otherwise, return the new quota.
  quota_manager_->SetPersistentHostQuota(
      storage_key_.origin().host(), requested_quota,
      base::BindOnce(&QuotaManagerHost::DidSetHostQuota,
                     weak_factory_.GetWeakPtr(), current_usage,
                     std::move(callback)));
}

void QuotaManagerHost::DidSetHostQuota(int64_t current_usage,
                                       RequestStorageQuotaCallback callback,
                                       blink::mojom::QuotaStatusCode status,
                                       int64_t new_quota) {
  std::move(callback).Run(status, current_usage, new_quota);
}

void QuotaManagerHost::DidGetTemporaryUsageAndQuota(
    int64_t requested_quota,
    RequestStorageQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  std::move(callback).Run(status, usage, std::min(requested_quota, quota));
}

QuotaManagerHost::~QuotaManagerHost() = default;

}  // namespace content
