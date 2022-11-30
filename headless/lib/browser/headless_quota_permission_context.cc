// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_quota_permission_context.h"

#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace headless {

HeadlessQuotaPermissionContext::HeadlessQuotaPermissionContext() = default;

void HeadlessQuotaPermissionContext::RequestQuotaPermission(
    const content::StorageQuotaParams& params,
    int render_process_id,
    PermissionCallback callback) {
  if (params.storage_type != blink::mojom::StorageType::kPersistent) {
    // For now we only support requesting quota with this interface
    // for Persistent storage type.
    std::move(callback).Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
    return;
  }

  std::move(callback).Run(QUOTA_PERMISSION_RESPONSE_ALLOW);
}

HeadlessQuotaPermissionContext::~HeadlessQuotaPermissionContext() = default;

}  // namespace headless
