// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_context.h"

#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace content {

BucketContext::BucketContext(
    const GlobalRenderFrameHostId& render_frame_host_id,
    const url::Origin& origin)
    : id_(render_frame_host_id), origin_(origin) {}

BucketContext::BucketContext(int render_process_id, const url::Origin& origin)
    : id_(render_process_id), origin_(origin) {}

BucketContext::BucketContext(const BucketContext& other) = default;

BucketContext::~BucketContext() = default;

StoragePartitionImpl* BucketContext::GetStoragePartition() const {
  if (absl::holds_alternative<int>(id_)) {
    RenderProcessHost* rph = RenderProcessHost::FromID(absl::get<int>(id_));
    if (!rph)
      return nullptr;
    return static_cast<StoragePartitionImpl*>(rph->GetStoragePartition());
  }

  RenderFrameHost* rfh =
      RenderFrameHost::FromID(absl::get<GlobalRenderFrameHostId>(id_));
  if (!rfh)
    return nullptr;
  return static_cast<StoragePartitionImpl*>(rfh->GetStoragePartition());
}

blink::mojom::PermissionStatus BucketContext::GetPermissionStatus(
    blink::PermissionType permission_type) const {
  if (permission_status_for_test_)
    return *permission_status_for_test_;

  if (absl::holds_alternative<int>(id_)) {
    RenderProcessHost* rph = RenderProcessHost::FromID(absl::get<int>(id_));
    if (!rph)
      return blink::mojom::PermissionStatus::DENIED;
    return rph->GetBrowserContext()
        ->GetPermissionController()
        ->GetPermissionStatusForWorker(permission_type, rph, origin_);
  }

  RenderFrameHost* rfh =
      RenderFrameHost::FromID(absl::get<GlobalRenderFrameHostId>(id_));
  if (!rfh)
    return blink::mojom::PermissionStatus::DENIED;
  return rfh->GetBrowserContext()
      ->GetPermissionController()
      ->GetPermissionStatusForCurrentDocument(permission_type, rfh);
}

}  // namespace content
