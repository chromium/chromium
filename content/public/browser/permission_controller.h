// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_H_

#include "base/supports_user_data.h"
#include "base/types/id_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_type.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class GURL;

namespace url {
class Origin;
}

namespace content {
class RenderFrameHost;

// This class allows the content layer to manipulate permissions. It's behavior
// is defined by the embedder via PermissionControllerDelegate implementation.
class CONTENT_EXPORT PermissionController
    : public base::SupportsUserData::Data {
 public:
  // Identifier for an active subscription. This is intentionally a distinct
  // type from PermissionControllerDelegate::SubscriptionId as the concrete
  // identifier values may be different.
  using SubscriptionId = base::IdType64<PermissionController>;

  ~PermissionController() override {}

  // Returns the permission status of a given requesting_origin/embedding_origin
  // tuple. This is not taking a RenderFrameHost because the call might happen
  // outside of a frame context. Prefer GetPermissionStatusForCurrentDocument
  // (below) whenever possible.
  virtual blink::mojom::PermissionStatus DeprecatedGetPermissionStatus(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) = 0;

  // Returns the permission status for a given origin. ServiceWorker isn't
  // associated with any WebContents, hence a document's lifecycle state isn't
  // considered.
  virtual blink::mojom::PermissionStatus GetPermissionStatusForServiceWorker(
      PermissionType permission,
      const url::Origin& service_worker_origin) = 0;

  // Returns the permission status for the current document in the given
  // RenderFrameHost. Use this over `DeprecatedGetPermissionStatus` whenever
  // possible as this API takes into account the lifecycle state of a given
  // document (i.e. whether it's in back-forward cache or being prerendered) in
  // addition to its origin.
  virtual blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host) = 0;

  // Returns the permission status for a given origin. Use this API only if
  // there is no document and it is not a ServiceWorker.
  virtual blink::mojom::PermissionStatus
  GetPermissionStatusForOriginWithoutContext(PermissionType permission,
                                             const url::Origin& origin) = 0;

  // Requests the permission for a given requesting_origin. Prefer
  // `RequestPermissionFromCurrentDocument` whenever possible.
  virtual void RequestPermission(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) = 0;

  // Requests the permission for the current document in the given
  // RenderFrameHost. Use this over `RequestPermission` whenever
  // possible as this API takes into account the lifecycle state of a given
  // document (i.e. whether it's in back-forward cache or being prerendered) in
  // addition to its origin.
  virtual void RequestPermissionFromCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) = 0;
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
