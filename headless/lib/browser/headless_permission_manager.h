// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_PERMISSION_MANAGER_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_PERMISSION_MANAGER_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/permission_controller_delegate.h"

namespace content {
class BrowserContext;
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
  void RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void RequestPermissions(
      const std::vector<content::PermissionType>& permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      content::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  SubscriptionId SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace content

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_PERMISSION_MANAGER_H_
