// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_permission_controller.h"

namespace content {

MockPermissionController::MockPermissionController() = default;

MockPermissionController::~MockPermissionController() = default;

void MockPermissionController::RequestPermissionFromCurrentDocument(
    RenderFrameHost* render_frame_host,
    PermissionRequestDescription request_description,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {}

void MockPermissionController::RequestPermissionsFromCurrentDocument(
    RenderFrameHost* render_frame_host,
    PermissionRequestDescription request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {}

void MockPermissionController::ResetPermission(blink::PermissionType permission,
                                               const url::Origin& origin) {}

void MockPermissionController::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {}

}  // namespace content
