// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_H_

#include "base/containers/id_map.h"
#include "base/supports_user_data.h"
#include "base/types/id_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/permission_status_subscription.h"
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
// TODO(crbug.com/40220500): Use url::Origin instead of GURL.
class CONTENT_EXPORT PermissionController
    : public base::SupportsUserData::Data {
 public:
  // Identifier for an active PermissionStatusSubscription. This is
  // intentionally a distinct type from
  // PermissionControllerDelegate::SubscriptionId as the concrete identifier
  // values may be different.
  using SubscriptionId = base::IdType64<PermissionController>;

  using SubscriptionsMap =
      base::IDMap<std::unique_ptr<PermissionStatusSubscription>,
                  SubscriptionId>;

  ~PermissionController() override = default;

  // Returns the status of the given |permission| for a worker on
  // |worker_origin| running in the renderer corresponding to
  // |render_process_host|.
  virtual PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      RenderProcessHost* render_process_host,
      const url::Origin& worker_origin) = 0;

  // Returns the permission status for the current document in the given
  // RenderFrameHost. This API takes into account the lifecycle state of a given
  // document (i.e. whether it's in back-forward cache or being prerendered) in
  // addition to its origin.
  virtual PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host) = 0;

  // The method does the same as `GetPermissionStatusForCurrentDocument` but
  // additionally returns a source or reason for the permission status.
  virtual PermissionResult GetPermissionResultForCurrentDocument(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host) = 0;

  // Returns the permission status for a given origin. Use this API only if
  // there is no document and it is not a ServiceWorker.
  virtual PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& origin) = 0;

  // The method does the same as `GetPermissionResultForOriginWithoutContext`
  // but it can be used for `PermissionType` that are keyed on a combination of
  // requesting and embedding origins, e.g., Notifications or StorageAccess.
  virtual PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) = 0;

  // Requests the permission from the current document in the given
  // RenderFrameHost. This API takes into account the lifecycle state of a given
  // document (i.e. whether it's in back-forward cache or being prerendered) in
  // addition to its origin.
  // TODO(crbug.com/40275129): Remove this function because it doesn't make
  // sense to have a single `PermissionStatus` as the callback. Related code
  // should be refactored and use the function following this one which is using
  // a vector of PermissionStatus for callbacks.
  virtual void RequestPermissionFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      PermissionRequestDescription request_description,
      base::OnceCallback<void(PermissionStatus)> callback) = 0;

  // RenderFrameHost. This API takes into account the lifecycle state of a given
  // document (i.e. whether it's in back-forward cache or being prerendered) in
  // addition to its origin.
  // WARNING: Permission requests order is not guaranteed.
  // TODO(crbug.com/40864728): Migrate to `std::set`.
  virtual void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      PermissionRequestDescription request_description,
      base::OnceCallback<void(const std::vector<PermissionStatus>&)>
          callback) = 0;

  // Sets the permission back to its default for the `origin`.
  virtual void ResetPermission(blink::PermissionType permission,
                               const url::Origin& origin) = 0;

  // Create a new subscription for permission status changes and call the
  // permission_controller_delegate.
  // Only one of |render_process_host| and |render_frame_host| should be set,
  // or neither. RenderProcessHost will be inferred from |render_frame_host|.
  virtual SubscriptionId SubscribeToPermissionStatusChange(
      blink::PermissionType permission,
      RenderProcessHost* render_process_host,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool should_include_device_status,
      const base::RepeatingCallback<void(PermissionStatus)>& callback) = 0;

  // Unsubscribe permission status change. This function will remove
  // subscription from subscriptions list and call
  // permission_controller_delegate to remove related data in the delegate.
  virtual void UnsubscribeFromPermissionStatusChange(
      SubscriptionId subscription_id) = 0;

  // Returns `true` if a document subscribed to
  // `PermissionStatus.onchange` listener or `PermissionStatus.AddEventListener`
  // with a type `change` was added. Returns `false` otherwise.
  virtual bool IsSubscribedToPermissionChangeEvent(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host) = 0;
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
