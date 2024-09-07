// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_PERMISSION_DELEGATE_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_PERMISSION_DELEGATE_H_

#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"

namespace blink {
enum class PermissionType;
}

// PermissionControllerDelegate implementation for WebEngine. It redirects
// permission redirects all calls to the appropriate FramePermissionController
// instance.
class WebEnginePermissionDelegate
    : public content::PermissionControllerDelegate {
 public:
  WebEnginePermissionDelegate();
  ~WebEnginePermissionDelegate() override;

  WebEnginePermissionDelegate(WebEnginePermissionDelegate&) = delete;
  WebEnginePermissionDelegate& operator=(WebEnginePermissionDelegate&) = delete;

  // content::PermissionControllerDelegate implementation:
  void RequestPermissions(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  void RequestPermissionsFromCurrentDocument(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  content::PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      bool should_include_device_status) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForEmbeddedRequester(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const url::Origin& overridden_origin) override;
  void OnPermissionStatusChangeSubscriptionAdded(
      content::PermissionController::SubscriptionId subscription_id) override;
  void UnsubscribeFromPermissionStatusChange(
      content::PermissionController::SubscriptionId subscription_id) override;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_PERMISSION_DELEGATE_H_
