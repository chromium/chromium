// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_

#include "base/containers/id_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/public/browser/permission_request_description.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "url/origin.h"

namespace blink {
enum class PermissionType;
}

namespace content {

// Implements the PermissionService Mojo interface.
// This service can be created from a RenderFrameHost or a RenderProcessHost.
// It is owned by a PermissionServiceContext.
// It receives at PermissionServiceContext instance when created which allows it
// to have some information about the current context. That enables the service
// to know whether it can show UI and have knowledge of the associated
// WebContents for example.
// TODO(crbug.com/40220500): Use url::Origin instead of GURL.
class PermissionServiceImpl : public blink::mojom::PermissionService {
 public:
  PermissionServiceImpl(PermissionServiceContext* context,
                        const url::Origin& origin);

  PermissionServiceImpl(const PermissionServiceImpl&) = delete;
  PermissionServiceImpl& operator=(const PermissionServiceImpl&) = delete;

  ~PermissionServiceImpl() override;

 private:
  friend class PermissionServiceImplTest;

  using PermissionStatusCallback =
      base::OnceCallback<void(blink::mojom::PermissionStatus)>;

  class PendingRequest;
  using RequestsMap = base::IDMap<std::unique_ptr<PendingRequest>>;

  // blink::mojom::PermissionService.
  void HasPermission(blink::mojom::PermissionDescriptorPtr permission,
                     PermissionStatusCallback callback) override;
  void RegisterPageEmbeddedPermissionControl(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      mojo::PendingRemote<blink::mojom::EmbeddedPermissionControlClient> client)
      override;
  void RequestPageEmbeddedPermission(
      blink::mojom::EmbeddedPermissionRequestDescriptorPtr descriptor,
      RequestPageEmbeddedPermissionCallback callback) override;
  void RequestPermission(blink::mojom::PermissionDescriptorPtr permission,
                         bool user_gesture,
                         PermissionStatusCallback callback) override;
  void RequestPermissions(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      bool user_gesture,
      RequestPermissionsCallback callback) override;
  void RevokePermission(blink::mojom::PermissionDescriptorPtr permission,
                        PermissionStatusCallback callback) override;
  void AddPermissionObserver(
      blink::mojom::PermissionDescriptorPtr permission,
      blink::mojom::PermissionStatus last_known_status,
      mojo::PendingRemote<blink::mojom::PermissionObserver> observer) override;
  void AddPageEmbeddedPermissionObserver(
      blink::mojom::PermissionDescriptorPtr permission,
      blink::mojom::PermissionStatus last_known_status,
      mojo::PendingRemote<blink::mojom::PermissionObserver> observer) override;
  void NotifyEventListener(blink::mojom::PermissionDescriptorPtr permission,
                           const std::string& event_type,
                           bool is_added) override;

  void RequestPermissionsInternal(
      BrowserContext* browser_context,
      const std::vector<blink::mojom::PermissionDescriptorPtr>& permissions,
      PermissionRequestDescription request_description,
      RequestPermissionsCallback callback);

  void OnRequestPermissionsResponse(
      int pending_request_id,
      const std::vector<blink::mojom::PermissionStatus>& result);

  void OnPageEmbeddedPermissionControlRegistered(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      bool allow,
      const mojo::Remote<blink::mojom::EmbeddedPermissionControlClient>&
          client);

  blink::mojom::PermissionStatus GetPermissionStatus(
      const blink::mojom::PermissionDescriptorPtr& permission);
  blink::mojom::PermissionStatus GetPermissionStatusFromType(
      blink::PermissionType type);
  blink::mojom::PermissionStatus GetCombinedPermissionAndDeviceStatus(
      const blink::mojom::PermissionDescriptorPtr& permission);
  void ResetPermissionStatus(blink::PermissionType type);
  void ReceivedBadMessage();

  RequestsMap pending_requests_;
  // context_ owns |this|.
  raw_ptr<PermissionServiceContext> context_;
  const url::Origin origin_;
  base::WeakPtrFactory<PermissionServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_
