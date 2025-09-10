// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_PERMISSION_MANAGER_H_
#define CONTENT_PUBLIC_TEST_MOCK_PERMISSION_MANAGER_H_

#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
enum class PermissionType;
}

namespace content {

// Mock of the permission manager for unit tests.
class MockPermissionManager : public PermissionControllerDelegate {
 public:
  MockPermissionManager();

  MockPermissionManager(const MockPermissionManager&) = delete;
  MockPermissionManager& operator=(const MockPermissionManager&) = delete;

  ~MockPermissionManager() override;

  // PermissionManager:
  MOCK_METHOD3(
      GetPermissionStatus,
      blink::mojom::PermissionStatus(
          const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
          const GURL& requesting_origin,
          const GURL& embedding_origin));
  MOCK_METHOD3(GetPermissionResultForOriginWithoutContext,
               PermissionResult(const blink::mojom::PermissionDescriptorPtr&
                                    permission_descriptor,
                                const url::Origin& requesting_origin,
                                const url::Origin& embedding_origin));
  MOCK_METHOD3(GetPermissionResultForCurrentDocument,
               PermissionResult(const blink::mojom::PermissionDescriptorPtr&
                                    permission_descriptor,
                                RenderFrameHost* render_frame_host,
                                bool should_include_device_status));
  MOCK_METHOD3(
      GetPermissionStatusForCurrentDocument,
      blink::mojom::PermissionStatus(
          const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
          RenderFrameHost* render_frame_host,
          bool should_include_device_status));
  MOCK_METHOD3(
      GetPermissionStatusForWorker,
      blink::mojom::PermissionStatus(
          const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
          RenderProcessHost* render_process_host,
          const GURL& worker_origin));
  MOCK_METHOD3(GetPermissionResultForWorker,
               PermissionResult(const blink::mojom::PermissionDescriptorPtr&
                                    permission_descriptor,
                                RenderProcessHost* render_process_host,
                                const GURL& worker_origin));
  MOCK_METHOD3(GetPermissionResultForEmbeddedRequester,
               PermissionResult(const blink::mojom::PermissionDescriptorPtr&
                                    permission_descriptor,
                                RenderFrameHost* render_frame_host,
                                const url::Origin& overridden_origin));
  void RequestPermissions(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback)
      override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback)
      override;
  MOCK_METHOD1(
      OnPermissionStatusChangeSubscriptionAdded,
      void(content::PermissionController::SubscriptionId subscription_id));
  MOCK_METHOD1(
      UnsubscribeFromPermissionResultChange,
      void(content::PermissionController::SubscriptionId subscription_id));
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_PERMISSION_MANAGER_H_
