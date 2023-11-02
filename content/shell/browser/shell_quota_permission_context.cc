// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_quota_permission_context.h"

#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {

ShellQuotaPermissionContext::ShellQuotaPermissionContext() {}

void ShellQuotaPermissionContext::RequestQuotaPermission(
    const StorageQuotaParams& params,
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

ShellQuotaPermissionContext::~ShellQuotaPermissionContext() {}

}  // namespace content
