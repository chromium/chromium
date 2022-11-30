// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_permission_manager.h"
#include "content/public/browser/permission_controller.h"

#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {

MockPermissionManager::MockPermissionManager() = default;

MockPermissionManager::~MockPermissionManager() = default;

void MockPermissionManager::RequestPermission(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {}

void MockPermissionManager::RequestPermissions(
    const std::vector<blink::PermissionType>& permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {}

void MockPermissionManager::ResetPermission(blink::PermissionType permission,
                                            const GURL& requesting_origin,
                                            const GURL& embedding_origin) {}

void MockPermissionManager::RequestPermissionsFromCurrentDocument(
    const std::vector<blink::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {}
}  // namespace content
