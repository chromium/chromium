// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_

#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_permission_overrides.h"
#include "content/public/browser/permission_controller.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class RenderProcessHost;

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

  using PermissionOverrides = DevToolsPermissionOverrides::PermissionOverrides;
  enum class OverrideStatus { kOverrideNotSet, kOverrideSet };

  // For the given |origin|, grant permissions in |overrides| and reject all
  // others. If no |origin| is specified, grant permissions to all origins in
  // the browser context.
  OverrideStatus GrantOverridesForDevTools(
      const absl::optional<url::Origin>& origin,
      const std::vector<PermissionType>& permissions);
  OverrideStatus SetOverrideForDevTools(
      const absl::optional<url::Origin>& origin,
      PermissionType permission,
      const blink::mojom::PermissionStatus& status);
  void ResetOverridesForDevTools();

  // PermissionController implementation.
  blink::mojom::PermissionStatus DeprecatedGetPermissionStatus(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;

  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      PermissionType permission,
      RenderProcessHost* render_process_host,
      const url::Origin& worker_origin) override;

  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host) override;

  blink::mojom::PermissionStatus GetPermissionStatusForOriginWithoutContext(
      PermissionType permission,
      const url::Origin& origin) override;

  void RequestPermissionFromCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;

  void RequestPermissions(
      const std::vector<PermissionType>& permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback);

  void ResetPermission(PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin);

  // Only one of |render_process_host| and |render_frame_host| should be set,
  // or neither. RenderProcessHost will be inferred from |render_frame_host|.
  SubscriptionId SubscribePermissionStatusChange(
      PermissionType permission,
      RenderProcessHost* render_process_host,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const base::RepeatingCallback<void(blink::mojom::PermissionStatus)>&
          callback);

  void UnsubscribePermissionStatusChange(SubscriptionId subscription_id);

 private:
  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin);

  void RequestPermission(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;

  struct Subscription;
  using SubscriptionsMap =
      base::IDMap<std::unique_ptr<Subscription>, SubscriptionId>;
  using SubscriptionsStatusMap =
      base::flat_map<SubscriptionsMap::KeyType, blink::mojom::PermissionStatus>;

  blink::mojom::PermissionStatus GetSubscriptionCurrentValue(
      const Subscription& subscription);
  SubscriptionsStatusMap GetSubscriptionsStatuses(
      const absl::optional<GURL>& origin = absl::nullopt);
  void NotifyChangedSubscriptions(const SubscriptionsStatusMap& old_statuses);
  void OnDelegatePermissionStatusChange(SubscriptionId subscription_id,
                                        blink::mojom::PermissionStatus status);
  void UpdateDelegateOverridesForDevTools(
      const absl::optional<url::Origin>& origin);

  DevToolsPermissionOverrides devtools_permission_overrides_;

  // Note that SubscriptionId is distinct from
  // PermissionControllerDelegate::SubscriptionId, and the concrete ID values
  // may be different as well.
  SubscriptionsMap subscriptions_;
  SubscriptionId::Generator subscription_id_generator_;

  raw_ptr<BrowserContext> browser_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_
