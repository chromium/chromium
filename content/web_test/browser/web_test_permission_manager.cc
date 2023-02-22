// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_permission_manager.h"

#include <list>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/browser/permissions/permission_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/web_test/browser/web_test_content_browser_client.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace content {

namespace {

std::vector<ContentSettingPatternSource> GetContentSettings(
    const GURL& permission_origin,
    const GURL& embedding_origin,
    blink::mojom::PermissionStatus status) {
  absl::optional<ContentSetting> setting;
  switch (status) {
    case blink::mojom::PermissionStatus::GRANTED:
      setting = ContentSetting::CONTENT_SETTING_ALLOW;
      break;
    case blink::mojom::PermissionStatus::DENIED:
      setting = ContentSetting::CONTENT_SETTING_BLOCK;
      break;
    case blink::mojom::PermissionStatus::ASK:
      break;
  }
  std::vector<ContentSettingPatternSource> patterns;
  if (setting) {
    patterns.emplace_back(ContentSettingsPattern::FromURL(permission_origin),
                          ContentSettingsPattern::FromURL(embedding_origin),
                          base::Value(*setting), /*source=*/"",
                          /*incognito=*/false);
  }
  return patterns;
}

}  // namespace

struct WebTestPermissionManager::Subscription {
  PermissionDescription permission;
  base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback;
  blink::mojom::PermissionStatus current_value;
};

WebTestPermissionManager::PermissionDescription::PermissionDescription(
    blink::PermissionType type,
    const GURL& origin,
    const GURL& embedding_origin)
    : type(type), origin(origin), embedding_origin(embedding_origin) {}

bool WebTestPermissionManager::PermissionDescription::operator==(
    const PermissionDescription& other) const {
  return type == other.type && origin == other.origin &&
         embedding_origin == other.embedding_origin;
}

bool WebTestPermissionManager::PermissionDescription::operator!=(
    const PermissionDescription& other) const {
  return !this->operator==(other);
}

size_t WebTestPermissionManager::PermissionDescription::Hash::operator()(
    const PermissionDescription& description) const {
  size_t hash = std::hash<int>()(static_cast<int>(description.type));
  hash += std::hash<std::string>()(description.embedding_origin.spec());
  hash += std::hash<std::string>()(description.origin.spec());
  return hash;
}

WebTestPermissionManager::WebTestPermissionManager(
    BrowserContext& browser_context)
    : PermissionControllerDelegate(), browser_context_(browser_context) {}

WebTestPermissionManager::~WebTestPermissionManager() = default;

void WebTestPermissionManager::RequestPermission(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  std::move(callback).Run(
      GetPermissionStatus(permission, requesting_origin,
                          PermissionUtil::GetLastCommittedOriginAsURL(
                              render_frame_host->GetMainFrame())));
}

void WebTestPermissionManager::RequestPermissions(
    const std::vector<blink::PermissionType>& permissions,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(std::vector<blink::mojom::PermissionStatus>(
        permissions.size(), blink::mojom::PermissionStatus::DENIED));
    return;
  }

  std::vector<blink::mojom::PermissionStatus> result;
  result.reserve(permissions.size());
  const GURL& embedding_origin = PermissionUtil::GetLastCommittedOriginAsURL(
      render_frame_host->GetMainFrame());
  for (const auto& permission : permissions) {
    result.push_back(
        GetPermissionStatus(permission, requesting_origin, embedding_origin));
  }

  std::move(callback).Run(result);
}

void WebTestPermissionManager::ResetPermission(blink::PermissionType permission,
                                               const GURL& requesting_origin,
                                               const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::AutoLock lock(permissions_lock_);

  auto it = permissions_.find(
      PermissionDescription(permission, requesting_origin, embedding_origin));
  if (it == permissions_.end())
    return;
  permissions_.erase(it);
}

void WebTestPermissionManager::RequestPermissionsFromCurrentDocument(
    const std::vector<blink::PermissionType>& permissions,
    RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(std::vector<blink::mojom::PermissionStatus>(
        permissions.size(), blink::mojom::PermissionStatus::DENIED));
    return;
  }

  std::vector<blink::mojom::PermissionStatus> result;
  result.reserve(permissions.size());
  const GURL& requesting_origin =
      PermissionUtil::GetLastCommittedOriginAsURL(render_frame_host);
  const GURL& embedding_origin = PermissionUtil::GetLastCommittedOriginAsURL(
      render_frame_host->GetMainFrame());
  for (const auto& permission : permissions) {
    result.push_back(
        GetPermissionStatus(permission, requesting_origin, embedding_origin));
  }

  std::move(callback).Run(result);
}

blink::mojom::PermissionStatus WebTestPermissionManager::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::AutoLock lock(permissions_lock_);

  auto it = permissions_.find(
      PermissionDescription(permission, requesting_origin, embedding_origin));
  if (it == permissions_.end()) {
    auto default_state = default_permission_status_.find(permission);
    if (default_state != default_permission_status_.end()) {
      return default_state->second;
    }
    return blink::mojom::PermissionStatus::DENIED;
  }

  // Immitates the behaviour of the NotificationPermissionContext in that
  // permission cannot be requested from cross-origin iframes, which the current
  // permission status should reflect when it's status is ASK.
  if (permission == blink::PermissionType::NOTIFICATIONS) {
    if (requesting_origin != embedding_origin &&
        it->second == blink::mojom::PermissionStatus::ASK) {
      return blink::mojom::PermissionStatus::DENIED;
    }
  }

  return it->second;
}

PermissionResult
WebTestPermissionManager::GetPermissionResultForOriginWithoutContext(
    blink::PermissionType permission,
    const url::Origin& origin) {
  blink::mojom::PermissionStatus status =
      GetPermissionStatus(permission, origin.GetURL(), origin.GetURL());

  return PermissionResult(status, content::PermissionStatusSource::UNSPECIFIED);
}

blink::mojom::PermissionStatus
WebTestPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsNestedWithinFencedFrame())
    return blink::mojom::PermissionStatus::DENIED;
  return GetPermissionStatus(
      permission,
      PermissionUtil::GetLastCommittedOriginAsURL(render_frame_host),
      PermissionUtil::GetLastCommittedOriginAsURL(
          render_frame_host->GetMainFrame()));
}

blink::mojom::PermissionStatus
WebTestPermissionManager::GetPermissionStatusForWorker(
    blink::PermissionType permission,
    RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return GetPermissionStatus(permission, worker_origin, worker_origin);
}

WebTestPermissionManager::SubscriptionId
WebTestPermissionManager::SubscribePermissionStatusChange(
    blink::PermissionType permission,
    RenderProcessHost* render_process_host,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the request is from a worker, it won't have a RFH.
  GURL embedding_origin = requesting_origin;
  if (render_frame_host) {
    embedding_origin = PermissionUtil::GetLastCommittedOriginAsURL(
        render_frame_host->GetMainFrame());
  }

  auto subscription = std::make_unique<Subscription>();
  subscription->permission =
      PermissionDescription(permission, requesting_origin, embedding_origin);
  subscription->callback = std::move(callback);
  subscription->current_value =
      GetPermissionStatus(permission, subscription->permission.origin,
                          subscription->permission.embedding_origin);

  auto id = subscription_id_generator_.GenerateNextId();
  subscriptions_.AddWithID(std::move(subscription), id);
  return id;
}

void WebTestPermissionManager::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!subscriptions_.Lookup(subscription_id))
    return;

  subscriptions_.Remove(subscription_id);
}

void WebTestPermissionManager::SetPermission(
    blink::PermissionType permission,
    blink::mojom::PermissionStatus status,
    const GURL& url,
    const GURL& embedding_url,
    blink::test::mojom::PermissionAutomation::SetPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionDescription description(permission, url.DeprecatedGetOriginAsURL(),
                                    embedding_url.DeprecatedGetOriginAsURL());

  {
    base::AutoLock lock(permissions_lock_);

    auto it = permissions_.find(description);
    if (it == permissions_.end()) {
      permissions_.insert(
          std::pair<PermissionDescription, blink::mojom::PermissionStatus>(
              description, status));
    } else {
      it->second = status;
    }
  }

  OnPermissionChanged(description, status, std::move(callback));
}

void WebTestPermissionManager::SetPermission(
    blink::mojom::PermissionDescriptorPtr descriptor,
    blink::mojom::PermissionStatus status,
    const GURL& url,
    const GURL& embedding_url,
    blink::test::mojom::PermissionAutomation::SetPermissionCallback callback) {
  auto type = blink::PermissionDescriptorToPermissionType(descriptor);
  if (!type) {
    std::move(callback).Run(false);
    return;
  }
  GURL applicable_permission_url = url;
  if (PermissionUtil::IsDomainOverride(descriptor)) {
    const auto overridden_origin =
        PermissionUtil::ExtractDomainOverride(descriptor);
    applicable_permission_url = overridden_origin.GetURL();
  }

  SetPermission(*type, status, applicable_permission_url, embedding_url,
                std::move(callback));
}

void WebTestPermissionManager::ResetPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::AutoLock lock(permissions_lock_);
  permissions_.clear();
}

void WebTestPermissionManager::Bind(
    mojo::PendingReceiver<blink::test::mojom::PermissionAutomation> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void WebTestPermissionManager::OnPermissionChanged(
    const PermissionDescription& permission,
    blink::mojom::PermissionStatus status,
    blink::test::mojom::PermissionAutomation::SetPermissionCallback
        permission_callback) {
  std::vector<base::OnceClosure> callbacks;
  callbacks.reserve(subscriptions_.size());

  for (SubscriptionsMap::iterator iter(&subscriptions_); !iter.IsAtEnd();
       iter.Advance()) {
    Subscription* subscription = iter.GetCurrentValue();
    if (subscription->permission != permission)
      continue;

    if (subscription->current_value == status)
      continue;

    subscription->current_value = status;

    // Add the callback to |callbacks| which will be run after the loop to
    // prevent re-entrance issues.
    callbacks.push_back(base::BindOnce(subscription->callback, status));
  }

  for (auto& callback : callbacks)
    std::move(callback).Run();

  // The network service expects to hear about any new storage-access permission
  // grants, so we have to inform it. This is true for "regular" or top-level
  // storage access permission changes.
  switch (permission.type) {
    case blink::PermissionType::STORAGE_ACCESS_GRANT:
      browser_context_->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess()
          ->SetStorageAccessGrantSettings(
              GetContentSettings(permission.origin, permission.embedding_origin,
                                 status),
              base::BindOnce(std::move(permission_callback), /*success=*/true));
      break;
    case blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      browser_context_->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess()
          ->SetTopLevelStorageAccessSettings(
              GetContentSettings(permission.origin, permission.embedding_origin,
                                 status),
              base::BindOnce(std::move(permission_callback), /*success=*/true));
      break;
    default:
      std::move(permission_callback).Run(true);
      break;
  }
}

}  // namespace content
