// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_DELEGATE_H_

#include "base/types/id_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_permission_overrides.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class GURL;

namespace content {
enum class PermissionType;
class RenderFrameHost;
class RenderProcessHost;

class CONTENT_EXPORT PermissionControllerDelegate {
 public:
  using PermissionOverrides = DevToolsPermissionOverrides::PermissionOverrides;

  // Identifier for an active subscription.
  using SubscriptionId = base::IdType64<PermissionControllerDelegate>;

  virtual ~PermissionControllerDelegate() = default;

  // Requests a permission on behalf of a frame identified by
  // render_frame_host.
  // When the permission request is handled, whether it failed, timed out or
  // succeeded, the |callback| will be run.
  virtual void RequestPermission(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) = 0;

  // Requests multiple permissions on behalf of a frame identified by
  // render_frame_host.
  // When the permission request is handled, whether it failed, timed out or
  // succeeded, the |callback| will be run. The order of statuses in the
  // returned vector will correspond to the order of requested permission
  // types.
  virtual void RequestPermissions(
      const std::vector<PermissionType>& permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(
          const std::vector<blink::mojom::PermissionStatus>&)> callback) = 0;

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
  // Deprecated. Use `GetPermissionStatusForCurrentDocument` instead.
  virtual blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) = 0;

  // Returns the permission status for the current document in the given
  // RenderFrameHost. Use this over `GetPermissionStatusForFrame` and
  // `GetPermissionStatus` whenever possible as this API takes into account the
  // lifecycle state of a given document (i.e. whether it's in back-forward
  // cache or being prerendered) in addition to its origin.
  virtual blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host) = 0;

  // Returns the status of the given `permission` for a worker on
  // `worker_origin` running in `render_process_host`, also performing
  // additional checks such as Permission Policy.  Use this over
  // GetPermissionStatus whenever possible.
  virtual blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      PermissionType permission,
      RenderProcessHost* render_process_host,
      const GURL& worker_origin) = 0;

  // Sets the permission back to its default for the requesting_origin/
  // embedding_origin tuple.
  virtual void ResetPermission(PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) = 0;

  // Runs the given |callback| whenever the |permission| associated with the
  // given |render_frame_host| changes. |render_process_host| should be passed
  // instead if the request is from a worker. Returns the ID to be used to
  // unsubscribe, which can be `is_null()` if the subscribe was not successful.
  // Exactly one of |render_process_host| and |render_frame_host| should be
  // set, RenderProcessHost will be inferred from |render_frame_host|.
  virtual SubscriptionId SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)>
          callback) = 0;

  // Unregisters from permission status change notifications. The
  // |subscription_id| must match the value returned by the
  // SubscribePermissionStatusChange call. Unsubscribing an already
  // unsubscribed |subscription_id| or an `is_null()` ID is a no-op.
  virtual void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) = 0;

  // Manually overrides default permission settings of delegate, if overrides
  // are tracked by the delegate. This method should only be called by the
  // PermissionController owning the delegate.
  virtual void SetPermissionOverridesForDevTools(
      const absl::optional<url::Origin>& origin,
      const PermissionOverrides& overrides) {}

  // Removes overrides that have been set, if any, for all origins. If delegate
  // does not maintain own permission set, then nothing happens.
  virtual void ResetPermissionOverridesForDevTools() {}

  // Returns whether permission can be overridden by
  // DevToolsPermissionOverrides.
  virtual bool IsPermissionOverridableByDevTools(
      PermissionType permission,
      const absl::optional<url::Origin>& origin);
};

}  // namespace content

namespace std {

template <>
struct hash<content::PermissionControllerDelegate::SubscriptionId> {
  std::size_t operator()(
      const content::PermissionControllerDelegate::SubscriptionId& v) const {
    content::PermissionControllerDelegate::SubscriptionId::Hasher hasher;
    return hasher(v);
  }
};

}  // namespace std

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_DELEGATE_H_
