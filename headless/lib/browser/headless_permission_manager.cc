// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_permission_manager.h"

#include "base/functional/callback.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace headless {

HeadlessPermissionManager::HeadlessPermissionManager() = default;

HeadlessPermissionManager::~HeadlessPermissionManager() = default;

void HeadlessPermissionManager::RequestPermissions(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
        callback) {
  // In headless mode we just pretend the user "closes" any permission prompt,
  // without accepting or denying.
  std::vector<content::PermissionResult> result(
      request_description.permissions.size(),
      content::PermissionResult(blink::mojom::PermissionStatus::ASK));
  std::move(callback).Run(result);
}

void HeadlessPermissionManager::ResetPermission(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}

void HeadlessPermissionManager::RequestPermissionsFromCurrentDocument(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
        callback) {
  // In headless mode we just pretent the user "closes" any permission prompt,
  // without accepting or denying.
  std::vector<content::PermissionResult> result(
      request_description.permissions.size(),
      content::PermissionResult(blink::mojom::PermissionStatus::ASK));
  std::move(callback).Run(result);
}

blink::mojom::PermissionStatus HeadlessPermissionManager::GetPermissionStatus(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return blink::mojom::PermissionStatus::ASK;
}

content::PermissionResult
HeadlessPermissionManager::GetPermissionResultForOriginWithoutContext(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  blink::mojom::PermissionStatus status =
      GetPermissionStatus(permission_descriptor, requesting_origin.GetURL(),
                          embedding_origin.GetURL());

  return content::PermissionResult(status);
}

content::PermissionResult
HeadlessPermissionManager::GetPermissionResultForCurrentDocument(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  return content::PermissionResult(blink::mojom::PermissionStatus::ASK);
}

content::PermissionResult
HeadlessPermissionManager::GetPermissionResultForWorker(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return content::PermissionResult(blink::mojom::PermissionStatus::ASK);
}

content::PermissionResult
HeadlessPermissionManager::GetPermissionResultForEmbeddedRequester(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderFrameHost* render_frame_host,
    const url::Origin& overridden_origin) {
  return content::PermissionResult(blink::mojom::PermissionStatus::ASK);
}

}  // namespace headless
