// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_permission_manager.h"

#include "base/functional/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace headless {

HeadlessPermissionManager::HeadlessPermissionManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

HeadlessPermissionManager::~HeadlessPermissionManager() = default;

void HeadlessPermissionManager::RequestPermissions(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  // In headless mode we just pretent the user "closes" any permission prompt,
  // without accepting or denying.
  std::vector<blink::mojom::PermissionStatus> result(
      request_description.permissions.size(),
      blink::mojom::PermissionStatus::ASK);
  std::move(callback).Run(result);
}

void HeadlessPermissionManager::ResetPermission(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}

void HeadlessPermissionManager::RequestPermissionsFromCurrentDocument(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  // In headless mode we just pretent the user "closes" any permission prompt,
  // without accepting or denying.
  std::vector<blink::mojom::PermissionStatus> result(
      request_description.permissions.size(),
      blink::mojom::PermissionStatus::ASK);
  std::move(callback).Run(result);
}

blink::mojom::PermissionStatus HeadlessPermissionManager::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return blink::mojom::PermissionStatus::ASK;
}

content::PermissionResult
HeadlessPermissionManager::GetPermissionResultForOriginWithoutContext(
    blink::PermissionType permission,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  blink::mojom::PermissionStatus status = GetPermissionStatus(
      permission, requesting_origin.GetURL(), embedding_origin.GetURL());

  return content::PermissionResult(
      status, content::PermissionStatusSource::UNSPECIFIED);
}

blink::mojom::PermissionStatus
HeadlessPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  return blink::mojom::PermissionStatus::ASK;
}

blink::mojom::PermissionStatus
HeadlessPermissionManager::GetPermissionStatusForWorker(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return blink::mojom::PermissionStatus::ASK;
}

blink::mojom::PermissionStatus
HeadlessPermissionManager::GetPermissionStatusForEmbeddedRequester(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const url::Origin& overridden_origin) {
  return blink::mojom::PermissionStatus::ASK;
}

}  // namespace headless
