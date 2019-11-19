// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_permission_manager.h"

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"

namespace {
blink::mojom::PermissionStatus CheckPermissionStatus(
    content::PermissionType permission_type) {
  // TODO(crbug/922833): Temporary grant permission of
  // PROTECTED_MEDIA_IDENTIFIER to unblock EME development.
  return permission_type == content::PermissionType::PROTECTED_MEDIA_IDENTIFIER
             ? blink::mojom::PermissionStatus::GRANTED
             : blink::mojom::PermissionStatus::DENIED;
}
}  // namespace

WebEnginePermissionManager::WebEnginePermissionManager() = default;

WebEnginePermissionManager::~WebEnginePermissionManager() = default;

int WebEnginePermissionManager::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  std::move(callback).Run(CheckPermissionStatus(permission));
  return content::PermissionController::kNoPendingOperation;
}

int WebEnginePermissionManager::RequestPermissions(
    const std::vector<content::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  std::vector<blink::mojom::PermissionStatus> statuses;
  for (auto permission : permissions)
    statuses.push_back(CheckPermissionStatus(permission));

  std::move(callback).Run(statuses);
  return content::PermissionController::kNoPendingOperation;
}

void WebEnginePermissionManager::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  NOTIMPLEMENTED() << ": " << static_cast<int>(permission);
}

blink::mojom::PermissionStatus WebEnginePermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return CheckPermissionStatus(permission);
}

blink::mojom::PermissionStatus
WebEnginePermissionManager::GetPermissionStatusForFrame(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  return CheckPermissionStatus(permission);
}

int WebEnginePermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  NOTIMPLEMENTED() << ": " << static_cast<int>(permission);
  return content::PermissionController::kNoPendingOperation;
}

void WebEnginePermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
  NOTIMPLEMENTED();
}
