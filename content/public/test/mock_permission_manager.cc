// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_permission_manager.h"
#include "content/public/browser/permission_controller.h"

#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {

MockPermissionManager::MockPermissionManager() = default;

MockPermissionManager::~MockPermissionManager() = default;

void MockPermissionManager::RequestPermissions(
    RenderFrameHost* render_frame_host,
    const PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {}

void MockPermissionManager::ResetPermission(blink::PermissionType permission,
                                            const GURL& requesting_origin,
                                            const GURL& embedding_origin) {}

void MockPermissionManager::RequestPermissionsFromCurrentDocument(
    RenderFrameHost* render_frame_host,
    const PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {}
}  // namespace content
