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
  // outside of a frame context. Prefer GetPermissionStatusForFrame (below)
  // whenever possible.
  virtual blink::mojom::PermissionStatus GetPermissionStatus(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) = 0;

  // Returns the permission status for a given frame. Use this over
  // GetPermissionStatus whenever possible.
  // TODO(raymes): Currently we still pass the |requesting_origin| as a separate
  // parameter because we can't yet guarantee that it matches the last committed
  // origin of the RenderFrameHost. See https://crbug.com/698985.
  virtual blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) = 0;
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
