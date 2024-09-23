// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_PERMISSION_MANAGER_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_PERMISSION_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/permission_controller_delegate.h"

namespace blink {
enum class PermissionType;
}

namespace content {
class BrowserContext;
struct PermissionResult;
}

namespace headless {

class HeadlessPermissionManager : public content::PermissionControllerDelegate {
 public:
  explicit HeadlessPermissionManager(content::BrowserContext* browser_context);

  HeadlessPermissionManager(const HeadlessPermissionManager&) = delete;
  HeadlessPermissionManager& operator=(const HeadlessPermissionManager&) =
      delete;

  ~HeadlessPermissionManager() override;

  // PermissionManager implementation.
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

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace content

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_PERMISSION_MANAGER_H_
