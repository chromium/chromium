// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_

#include <map>
#include <optional>
#include <set>

#include "base/containers/id_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/permissions/permission_overrides.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
enum class PermissionType;
}

namespace content {

class BrowserContext;
class PermissionControllerImplTest;
class RenderProcessHost;
class PermissionServiceImpl;
class WebContents;
struct PermissionResult;

using blink::PermissionType;

// Implementation of the PermissionController interface. This
// is used by content/ layer to manage permissions.
// There is one instance of this class per BrowserContext.
class CONTENT_EXPORT PermissionControllerImpl : public PermissionController {
 public:
  explicit PermissionControllerImpl(BrowserContext* browser_context);

  PermissionControllerImpl(const PermissionControllerImpl&) = delete;
  PermissionControllerImpl& operator=(const PermissionControllerImpl&) = delete;

  ~PermissionControllerImpl() override;

  static PermissionControllerImpl* FromBrowserContext(
      BrowserContext* browser_context);

  enum class OverrideStatus { kOverrideNotSet, kOverrideSet };

  // Sets status for |permissions| to GRANTED for |requesting_origin| and
  // |embedding_origin|, and DENIED for all others. Null |requesting_origin| and
  // |embedding_origin| grants permissions globally for context.
  // It is invalid to call these methods with exactly one non-null origin.
  // If the overrides were set, the callback is run with
  // |OverrideStatus::kOverrideSet|. If not set, the callback is run with
  // |OverrideStatus::kOverrideNotSet|.
  void GrantPermissionOverrides(
      base::optional_ref<const url::Origin> requesting_origin,
      base::optional_ref<const url::Origin> embedding_origin,
      const std::vector<PermissionType>& permissions,
      base::OnceCallback<void(OverrideStatus)> callback);
  // If the overrides were set, the callback is run with
  // |OverrideStatus::kOverrideSet|. If not set, the callback is run with
  // |OverrideStatus::kOverrideNotSet|.
  void SetPermissionOverride(
      base::optional_ref<const url::Origin> requesting_origin,
      base::optional_ref<const url::Origin> embedding_origin,
      PermissionType permission,
      const PermissionStatus& status,
      base::OnceCallback<void(OverrideStatus)> callback);
  void ResetPermissionOverrides(base::OnceClosure callback);

  void ResetPermission(PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin);

  // Only one of |render_process_host| and |render_frame_host| should be set,
  // or neither. RenderProcessHost will be inferred from |render_frame_host|.
  SubscriptionId SubscribeToPermissionResultChange(
      blink::mojom::PermissionDescriptorPtr permission_descriptor,
      RenderProcessHost* render_process_host,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool should_include_device_status,
      const base::RepeatingCallback<void(PermissionResult)>& callback) override;

  void UnsubscribeFromPermissionResultChange(
      SubscriptionId subscription_id) override;

  // If there's currently a permission prompt bubble for the given WebContents,
  // returns the bounds of the bubble view as exclusion area in screen
  // coordinates.
  std::optional<gfx::Rect> GetExclusionAreaBoundsInScreen(
      WebContents* web_contents) const;

  void add_notify_listener_observer_for_tests(base::RepeatingClosure callback) {
    onchange_listeners_callback_for_tests_ = std::move(callback);
  }

  void set_exclusion_area_bounds_for_tests(
      const std::optional<gfx::Rect>& bounds) {
    exclusion_area_bounds_for_tests_ = bounds;
  }

 private:
  friend class PermissionControllerImplTest;
  friend class PermissionServiceImpl;

  // Updates CookieManager content settings. Currently this is only used for
  // Storage Access permissions. This method can be used for extra processing
  // for other permissions. If you add more permissions or different
  // processesing steps, update the method's name.
  void UpdateCookieManagerContentSettings(
      std::optional<PermissionType> permission_to_process,
      base::OnceClosure callback);

  PermissionResult GetPermissionResultInternal(
      const blink::mojom::PermissionDescriptorPtr& permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  PermissionResult GetPermissionResultForCurrentDocumentInternal(
      const blink::mojom::PermissionDescriptorPtr& permission,
      RenderFrameHost* render_frame_host,
      bool should_include_device_status = false);

  // PermissionController implementation.
  PermissionStatus GetPermissionStatusForWorker(
      const blink::mojom::PermissionDescriptorPtr& permission,
      RenderProcessHost* render_process_host,
      const url::Origin& worker_origin) override;
  PermissionResult GetPermissionResultForWorker(
      const blink::mojom::PermissionDescriptorPtr& permission,
      RenderProcessHost* render_process_host,
      const url::Origin& worker_origin) override;
  PermissionStatus GetPermissionStatusForCurrentDocument(
      const blink::mojom::PermissionDescriptorPtr& permission,
      RenderFrameHost* render_frame_host) override;
  PermissionResult GetPermissionResultForCurrentDocument(
      const blink::mojom::PermissionDescriptorPtr& permission,
      RenderFrameHost* render_frame_host) override;
  PermissionStatus GetCombinedPermissionAndDeviceStatus(
      const blink::mojom::PermissionDescriptorPtr& permission,
      RenderFrameHost* render_frame_host) override;
  PermissionResult GetPermissionResultForOriginWithoutContext(
      const blink::mojom::PermissionDescriptorPtr& permission,
      const url::Origin& origin) override;
  PermissionResult GetPermissionResultForOriginWithoutContext(
      const blink::mojom::PermissionDescriptorPtr& permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  // WARNING: Permission requests order is not guaranteed.
  // TODO(crbug.com/40864728): Migrate to `std::set`.
  // TODO(crbug.com/40275129): `RequestPermissions` and
  // `RequestPermissionsFromCurrentDocument` do exactly the same things.
  // Merge them together.
  void RequestPermissions(
      RenderFrameHost* render_frame_host,
      PermissionRequestDescription request_description,
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback);
  void RequestPermissionFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      PermissionRequestDescription request_description,
      base::OnceCallback<void(PermissionResult)> callback) override;
  // WARNING: Permission requests order is not guaranteed.
  // TODO(crbug.com/40864728): Migrate to `std::set`.
  void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      PermissionRequestDescription request_description,
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback)
      override;
  void ResetPermission(blink::PermissionType permission,
                       const url::Origin& origin) override;

  PermissionResult GetPermissionResultForEmbeddedRequester(
      const blink::mojom::PermissionDescriptorPtr& permission,
      RenderFrameHost* render_frame_host,
      const url::Origin& requesting_origin);

  using SubscriptionsStatusMap =
      base::flat_map<SubscriptionsMap::KeyType, PermissionResult>;

  PermissionResult GetSubscriptionCurrentResult(
      const content::PermissionResultSubscription& subscription);
  SubscriptionsStatusMap GetSubscriptionsStatuses(
      const std::optional<GURL>& requesting_origin = std::nullopt,
      const std::optional<GURL>& embedding_origin = std::nullopt);
  void NotifyChangedSubscriptions(const SubscriptionsStatusMap& old_statuses);
  // Notifies the callback of the new permission status.
  // If `ignore_status_override` is true, the status override is not applied,
  // which means that the permission status change will be notified to
  // subscribed users even the status has been overridden.
  void PermissionResultChange(
      const base::RepeatingCallback<void(PermissionResult)>& callback,
      SubscriptionId subscription_id,
      PermissionResult result,
      bool ignore_status_override = false);
  bool IsSubscribedToPermissionChangeEvent(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host) override;

  // Notifies that an onchange event listener was added.
  void NotifyEventListener();

  PermissionOverrides permission_overrides_;

  base::RepeatingClosure onchange_listeners_callback_for_tests_;

  std::optional<gfx::Rect> exclusion_area_bounds_for_tests_;

  // Note that SubscriptionId is distinct from
  // PermissionControllerDelegate::SubscriptionId, and the concrete ID values
  // may be different as well.
  SubscriptionsMap subscriptions_;
  SubscriptionId::Generator subscription_id_generator_;

  raw_ptr<BrowserContext> browser_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_
