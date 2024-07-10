// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/types/id_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "ui/gfx/geometry/rect.h"

class GURL;

namespace url {
class Origin;
}

namespace blink {
enum class PermissionType;
}

namespace content {
class RenderFrameHost;
class RenderProcessHost;
class WebContents;
struct PermissionRequestDescription;

class CONTENT_EXPORT PermissionControllerDelegate {
 public:
  virtual ~PermissionControllerDelegate();

  // Requests multiple permissions on behalf of a frame identified by
  // |render_frame_host|. When the permission request is handled, whether it
  // failed, timed out or succeeded, the |callback| will be run. The order of
  // statuses in the returned vector will correspond to the order of requested
  // permission types.
  // TODO(crbug.com/40275129): `RequestPermissions` and
  // `RequestPermissionsFromCurrentDocument` do exactly the same things. Merge
  // them together.
  virtual void RequestPermissions(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<PermissionStatus>&)>
          callback) = 0;

  // Requests permissions from the current document in the given
  // RenderFrameHost. Use this over `RequestPermission` whenever possible as
  // this API takes into account the lifecycle state of a given document (i.e.
  // whether it's in back-forward cache or being prerendered) in addition to its
  // origin.
  virtual void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<PermissionStatus>&)>
          callback) = 0;

  // Returns the permission status of a given requesting_origin/embedding_origin
  // tuple. This is not taking a RenderFrameHost because the call might happen
  // outside of a frame context. Prefer GetPermissionStatusForCurrentDocument
  // (below) whenever possible.
  virtual PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) = 0;

  virtual PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) = 0;

  // Should return the permission status for the current document in the given
  // RenderFrameHost. This is used over `GetPermissionStatus` whenever possible
  // as this API takes into account the lifecycle state of a given document
  // (i.e. whether it's in back-forward cache or being prerendered) in addition
  // to its origin.
  // When called with should_include_device_status set to true, the delegate
  // should return a combination of the document permission status (site-level)
  // and the device-level permission status. For example, it should return
  // PermissionStatus::DENIED in scenarios where the site-level permission is
  // granted but the device-level permission is not.
  virtual PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host,
      bool should_include_device_status) = 0;

  // The method does the same as `GetPermissionStatusForCurrentDocument` but
  // additionally returns a source or reason for the permission status.
  virtual PermissionResult GetPermissionResultForCurrentDocument(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host,
      bool should_include_device_status);

  // Returns the status of the given `permission` for a worker on
  // `worker_origin` running in `render_process_host`, also performing
  // additional checks such as Permission Policy.  Use this over
  // GetPermissionStatus whenever possible.
  virtual PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      RenderProcessHost* render_process_host,
      const GURL& worker_origin) = 0;

  // Returns the permission status for `requesting_origin` in the given
  // `RenderFrameHost`. Other APIs interpret `requesting_origin` as the last
  // committed origin of the requesting frame. This API takes
  // `requesting_origin` as a separate parameter because it does not equal the
  // last committed origin of the requesting frame.  It is designed to be used
  // only for `TOP_LEVEL_STORAGE_ACCESS`.
  virtual PermissionStatus GetPermissionStatusForEmbeddedRequester(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host,
      const url::Origin& requesting_origin) = 0;

  // Sets the permission back to its default for the requesting_origin/
  // embedding_origin tuple.
  virtual void ResetPermission(blink::PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) = 0;

  // Set a pointer of subscriptions map from PermissionController.
  virtual void OnPermissionStatusChangeSubscriptionAdded(
      content::PermissionController::SubscriptionId subscription_id) {}

  // Unregisters from permission status change notifications. This function
  // is only called by PermissionController. In any other cases, please call the
  // `PermissionController::UnsubscribeFromPermissionStatusChange`.
  virtual void UnsubscribeFromPermissionStatusChange(
      content::PermissionController::SubscriptionId subscription_id) {}

  // If there's currently a permission UI presenting for the given WebContents,
  // returns bounds of the view as an exclusion area. We will use these bounds
  // to avoid situations where users may make bad decisions based on incorrect
  // contextual information (due to content or widgets overlaying the exclusion
  // area)
  virtual std::optional<gfx::Rect> GetExclusionAreaBoundsInScreen(
      WebContents* web_contents) const;

  // Returns whether permission can be overridden.
  virtual bool IsPermissionOverridable(
      blink::PermissionType permission,
      const std::optional<url::Origin>& origin);

  void SetSubscriptions(
      content::PermissionController::SubscriptionsMap* subscriptions);
  content::PermissionController::SubscriptionsMap* subscriptions();

 private:
  raw_ptr<content::PermissionController::SubscriptionsMap> subscriptions_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_CONTROLLER_DELEGATE_H_
