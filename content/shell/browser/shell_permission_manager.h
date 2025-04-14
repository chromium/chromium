// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_PERMISSION_MANAGER_H_
#define CONTENT_SHELL_BROWSER_SHELL_PERMISSION_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"

namespace blink {
enum class PermissionType;
}

namespace content {

class ShellPermissionManager : public PermissionControllerDelegate {
 public:
  ShellPermissionManager();

  ShellPermissionManager(const ShellPermissionManager&) = delete;
  ShellPermissionManager& operator=(const ShellPermissionManager&) = delete;

  ~ShellPermissionManager() override;

  // PermissionManager implementation.
  void RequestPermissions(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  PermissionResult GetPermissionResultForOriginWithoutContext(
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
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_PERMISSION_MANAGER_H_
