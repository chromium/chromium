// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_permission_controller.h"
#include "url/gurl.h"

namespace content {

MockPermissionController::MockPermissionController() = default;

MockPermissionController::~MockPermissionController() = default;

void MockPermissionController::RequestPermission(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {}

void MockPermissionController::RequestPermissionFromCurrentDocument(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {}

}  // namespace content
