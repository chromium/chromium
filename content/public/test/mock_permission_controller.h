// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_PERMISSION_CONTROLLER_H_
#define CONTENT_PUBLIC_TEST_MOCK_PERMISSION_CONTROLLER_H_

#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {
enum class PermissionType;
}

namespace url {
class Origin;
}

namespace content {

// Mock of the permission controller for unit tests.
class MockPermissionController : public PermissionController {
 public:
  MockPermissionController();

  MockPermissionController(const MockPermissionController&) = delete;
  MockPermissionController& operator=(const MockPermissionController&) = delete;

  ~MockPermissionController() override;

  // PermissionController:
  MOCK_METHOD(blink::mojom::PermissionStatus,
              GetPermissionStatusForWorker,
              (blink::PermissionType permission,
               RenderProcessHost* render_process_host,
               const url::Origin& worker_origin));
  MOCK_METHOD(blink::mojom::PermissionStatus,
              GetPermissionStatusForCurrentDocument,
              (blink::PermissionType permission,
               RenderFrameHost* render_frame_host));
  MOCK_METHOD(content::PermissionResult,
              GetPermissionResultForCurrentDocument,
              (blink::PermissionType permission,
               RenderFrameHost* render_frame_host));
  MOCK_METHOD(content::PermissionResult,
              GetPermissionResultForOriginWithoutContext,
              (blink::PermissionType permission,
               const url::Origin& requesting_origin));
  MOCK_METHOD(content::PermissionResult,
              GetPermissionResultForOriginWithoutContext,
              (blink::PermissionType permission,
               const url::Origin& requesting_origin,
               const url::Origin& embedding_origin));
  MOCK_METHOD(blink::mojom::PermissionStatus,
              GetPermissionStatusForEmbeddedRequester,
              (blink::PermissionType permission,
               RenderFrameHost* render_frame_host,
               const url::Origin& requesting_origin));
  MOCK_METHOD(bool,
              IsSubscribedToPermissionChangeEvent,
              (blink::PermissionType permission,
               RenderFrameHost* render_frame_host));
  MOCK_METHOD(
      void,
      RequestPermissionFromCurrentDocument,
      (RenderFrameHost * render_frame_host,
       PermissionRequestDescription request_description,
       base::OnceCallback<void(blink::mojom::PermissionStatus)> callback));
  MOCK_METHOD(
      void,
      RequestPermissionsFromCurrentDocument,
      (RenderFrameHost * render_frame_host,
       PermissionRequestDescription request_description,
       base::OnceCallback<
           void(const std::vector<blink::mojom::PermissionStatus>&)> callback));
  MOCK_METHOD(void,
              ResetPermission,
              (blink::PermissionType permission, const url::Origin& origin));
  MOCK_METHOD(
      SubscriptionId,
      SubscribeToPermissionStatusChange,
      (blink::PermissionType permission,
       RenderProcessHost* render_process_host,
       RenderFrameHost* render_frame_host,
       const GURL& requesting_origin,
       bool should_include_device_status,
       const base::RepeatingCallback<void(PermissionStatus)>& callback));
  MOCK_METHOD(void,
              UnsubscribeFromPermissionStatusChange,
              (SubscriptionId subscription_id));
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_PERMISSION_CONTROLLER_H_
