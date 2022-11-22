// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_

#include <map>
#include <set>

#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_overrides.h"
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

  // Sets status for |permissions| to GRANTED in |origin|, and DENIED
  // for all others.
  // Null |origin| grants permissions globally for context.
  OverrideStatus GrantPermissionOverrides(
      const absl::optional<url::Origin>& origin,
      const std::vector<PermissionType>& permissions);
  OverrideStatus SetPermissionOverride(
      const absl::optional<url::Origin>& origin,
      PermissionType permission,
      const blink::mojom::PermissionStatus& status);
  void ResetPermissionOverrides();

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
  SubscriptionId SubscribePermissionStatusChange(
      PermissionType permission,
      RenderProcessHost* render_process_host,
      const url::Origin& requesting_origin,
      const base::RepeatingCallback<void(blink::mojom::PermissionStatus)>&
          callback) override;

  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;

  void add_notify_listener_observer_for_tests(base::RepeatingClosure callback) {
    onchange_listeners_callback_for_tests_ = std::move(callback);
  }

 private:
  friend class PermissionControllerImplTest;
  friend class PermissionServiceImpl;

  blink::mojom::PermissionStatus GetPermissionStatusInternal(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  // PermissionController implementation.
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      PermissionType permission,
      RenderProcessHost* render_process_host,
      const url::Origin& worker_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host) override;
  PermissionResult GetPermissionResultForCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host) override;
  PermissionResult GetPermissionResultForOriginWithoutContext(
      PermissionType permission,
      const url::Origin& origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  // WARNING: Permission requests order is not guaranteed.
  // TODO(crbug.com/1363094): Migrate to `std::set`.
  void RequestPermissions(
      const std::vector<blink::PermissionType>& permissions,
      RenderFrameHost* render_frame_host,
      const url::Origin& requested_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback);
  void RequestPermissionFromCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  // WARNING: Permission requests order is not guaranteed.
  // TODO(crbug.com/1363094): Migrate to `std::set`.
  void RequestPermissionsFromCurrentDocument(
      const std::vector<PermissionType>& permissions,
      RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(blink::PermissionType permission,
                       const url::Origin& origin) override;

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
  bool IsSubscribedToPermissionChangeEvent(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host) override;

  void NotifyEventListener();

  PermissionOverrides permission_overrides_;

  absl::optional<base::RepeatingClosure> onchange_listeners_callback_for_tests_;

  // Note that SubscriptionId is distinct from
  // PermissionControllerDelegate::SubscriptionId, and the concrete ID values
  // may be different as well.
  SubscriptionsMap subscriptions_;
  SubscriptionId::Generator subscription_id_generator_;

  raw_ptr<BrowserContext> browser_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_
