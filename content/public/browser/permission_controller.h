// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_H_

#include "base/supports_user_data.h"
#include "base/types/id_type.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class GURL;

namespace blink {
enum class PermissionType;
}

namespace url {
class Origin;
}

namespace content {
class RenderFrameHost;
class RenderProcessHost;

// This class allows the content layer to manipulate permissions. It's behavior
// is defined by the embedder via PermissionControllerDelegate implementation.
// TODO(crbug.com/1312212): Use url::Origin instead of GURL.
class CONTENT_EXPORT PermissionController
    : public base::SupportsUserData::Data {
 public:
  // Identifier for an active subscription. This is intentionally a distinct
  // type from PermissionControllerDelegate::SubscriptionId as the concrete
  // identifier values may be different.
  using SubscriptionId = base::IdType64<PermissionController>;

  ~PermissionController() override {}

  // Returns the status of the given |permission| for a worker on
  // |worker_origin| running in the renderer corresponding to
  // |render_process_host|.
  virtual blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      RenderProcessHost* render_process_host,
      const url::Origin& worker_origin) = 0;

  // Returns the permission status for the current document in the given
  // RenderFrameHost. This API takes into account the lifecycle state of a given
  // document (i.e. whether it's in back-forward cache or being prerendered) in
  // addition to its origin.
  virtual blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host) = 0;

  // Returns the permission status for a given origin. Use this API only if
  // there is no document and it is not a ServiceWorker.
  virtual blink::mojom::PermissionStatus
  GetPermissionStatusForOriginWithoutContext(blink::PermissionType permission,
                                             const url::Origin& origin) = 0;

  // Requests the permission from the current document in the given
  // RenderFrameHost. This API takes into account the lifecycle state of a given
  // document (i.e. whether it's in back-forward cache or being prerendered) in
  // addition to its origin.
  virtual void RequestPermissionFromCurrentDocument(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) = 0;

  // Requests permissions from the current document in the given
  // RenderFrameHost. This API takes into account the lifecycle state of a given
  // document (i.e. whether it's in back-forward cache or being prerendered) in
  // addition to its origin.
  virtual void RequestPermissionsFromCurrentDocument(
      const std::vector<blink::PermissionType>& permission,
      RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<void(
          const std::vector<blink::mojom::PermissionStatus>&)> callback) = 0;
};

}  // namespace content

namespace std {

template <>
struct hash<content::PermissionController::SubscriptionId> {
  std::size_t operator()(
      const content::PermissionController::SubscriptionId& v) const {
    content::PermissionController::SubscriptionId::Hasher hasher;
    return hasher(v);
  }
};

}  // namespace std

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_H_
