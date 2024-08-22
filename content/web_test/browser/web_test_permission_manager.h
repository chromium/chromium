// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_PERMISSION_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_PERMISSION_MANAGER_H_

#include <stddef.h>

#include "base/containers/id_map.h"
#include "base/functional/callback_forward.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_automation.mojom.h"
#include "url/gurl.h"

namespace blink {
enum class PermissionType;
}

namespace content {

class WebTestPermissionManager
    : public PermissionControllerDelegate,
      public blink::test::mojom::PermissionAutomation {
 public:
  // `browser_context` must outlive `this`.
  explicit WebTestPermissionManager(BrowserContext& browser_context);

  WebTestPermissionManager(const WebTestPermissionManager&) = delete;
  WebTestPermissionManager& operator=(const WebTestPermissionManager&) = delete;

  ~WebTestPermissionManager() override;

  // PermissionManager overrides.
  void RequestPermissions(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;

  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  void RequestPermissionsFromCurrentDocument(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      bool should_include_device_status) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForEmbeddedRequester(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const url::Origin& overridden_origin) override;
  void OnPermissionStatusChangeSubscriptionAdded(
      content::PermissionController::SubscriptionId subscription_id) override;

  void UnsubscribeFromPermissionStatusChange(
      content::PermissionController::SubscriptionId subscription_id) override;
  void SetPermission(
      blink::PermissionType permission,
      blink::mojom::PermissionStatus status,
      const GURL& url,
      const GURL& embedding_url,
      blink::test::mojom::PermissionAutomation::SetPermissionCallback callback);
  void ResetPermissions();

  // blink::test::mojom::PermissionAutomation
  void SetPermission(
      blink::mojom::PermissionDescriptorPtr descriptor,
      blink::mojom::PermissionStatus status,
      const GURL& url,
      const GURL& embedding_url,
      blink::test::mojom::PermissionAutomation::SetPermissionCallback) override;

  void Bind(
      mojo::PendingReceiver<blink::test::mojom::PermissionAutomation> receiver);

 private:
  // Representation of a permission for the WebTestPermissionManager.
  struct PermissionDescription {
    PermissionDescription() = default;
    PermissionDescription(blink::PermissionType type,
                          const GURL& origin,
                          const GURL& embedding_origin);
    // Note that the comparison operator does not always require strict
    // origin equality for the requesting and embedding origin. For permission
    // types such as STORAGE_ACCESS_GRANT, which are scoped to (requesting
    // site, embedding site), it will apply a same-site check instead.
    bool operator==(const PermissionDescription& other) const;
    bool operator!=(const PermissionDescription& other) const;

    bool operator==(PermissionStatusSubscription* other) const;
    bool operator!=(PermissionStatusSubscription* other) const;

    // Hash operator for hash maps.
    struct Hash {
      size_t operator()(const PermissionDescription& description) const;
    };

    blink::PermissionType type;
    GURL origin;
    GURL embedding_origin;
  };

  using PermissionsMap = std::unordered_map<PermissionDescription,
                                            blink::mojom::PermissionStatus,
                                            PermissionDescription::Hash>;
  using DefaultPermissionStatusMap =
      std::unordered_map<blink::PermissionType, blink::mojom::PermissionStatus>;

  // A wrapper function of `GetPermissionStatus`. Called in requesting
  // permissions to handle the case when `GetPermissionStatus` should behave
  // differently when requesting and getting permissions.
  blink::mojom::PermissionStatus GetPermissionStatusForRequestPermission(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  void OnPermissionChanged(
      const PermissionDescription& permission,
      blink::mojom::PermissionStatus status,
      blink::test::mojom::PermissionAutomation::SetPermissionCallback callback);

  raw_ref<BrowserContext> browser_context_;

  // Mutex for permissions access. Unfortunately, the permissions can be
  // accessed from the IO thread because of Notifications' synchronous IPC.
  base::Lock permissions_lock_;

  // List of permissions currently known by the WebTestPermissionManager and
  // their associated |PermissionStatus|.
  PermissionsMap permissions_;

  // A map of permission types to their default statuses, for those that require
  // specific statuses to be returned in the absence of another value.
  DefaultPermissionStatusMap default_permission_status_ = {
      {blink::PermissionType::STORAGE_ACCESS_GRANT,
       blink::mojom::PermissionStatus::ASK},
      {blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS,
       blink::mojom::PermissionStatus::ASK},
  };

  mojo::ReceiverSet<blink::test::mojom::PermissionAutomation> receivers_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_PERMISSION_MANAGER_H_
