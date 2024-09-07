// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_permission_manager.h"

#include <functional>
#include <list>
#include <memory>
#include <utility>

#include "base/barrier_callback.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/browser/permissions/permission_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/web_test/browser/web_test_content_browser_client.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace content {

namespace {

std::vector<ContentSettingPatternSource> GetContentSettings(
    const ContentSettingsPattern& permission_pattern,
    const ContentSettingsPattern& embedding_pattern,
    blink::mojom::PermissionStatus status) {
  std::optional<ContentSetting> setting;
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
    patterns.emplace_back(permission_pattern, embedding_pattern,
                          base::Value(*setting),
                          content_settings::ProviderType::kNone,
                          /*incognito=*/false);
  }
  return patterns;
}

bool ShouldHideDeniedState(blink::PermissionType permission_type) {
  return permission_type == blink::PermissionType::STORAGE_ACCESS_GRANT ||
         permission_type == blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS;
}

}  // namespace

WebTestPermissionManager::PermissionDescription::PermissionDescription(
    blink::PermissionType type,
    const GURL& origin,
    const GURL& embedding_origin)
    : type(type), origin(origin), embedding_origin(embedding_origin) {}

bool WebTestPermissionManager::PermissionDescription::operator==(
    const PermissionDescription& other) const {
  if (type != other.type) {
    return false;
  }

  if (type == blink::PermissionType::STORAGE_ACCESS_GRANT) {
    const net::SchemefulSite requesting_site(origin);
    const net::SchemefulSite other_requesting_site(other.origin);
    const net::SchemefulSite embedding_site(embedding_origin);
    const net::SchemefulSite other_embedding_site(other.embedding_origin);
    return requesting_site == other_requesting_site &&
           embedding_site == other_embedding_site;
  }

  return origin == other.origin && embedding_origin == other.embedding_origin;
}

bool WebTestPermissionManager::PermissionDescription::operator!=(
    const PermissionDescription& other) const {
  return !this->operator==(other);
}

bool WebTestPermissionManager::PermissionDescription::operator==(
    PermissionStatusSubscription* other) const {
  if (type != other->permission) {
    return false;
  }

  if (type == blink::PermissionType::STORAGE_ACCESS_GRANT) {
    const net::SchemefulSite requesting_site(origin);
    const net::SchemefulSite other_requesting_site(
        other->requesting_origin_delegation);
    const net::SchemefulSite embedding_site(embedding_origin);
    const net::SchemefulSite other_embedding_site(other->embedding_origin);
    return requesting_site == other_requesting_site &&
           embedding_site == other_embedding_site;
  }

  return origin == other->requesting_origin_delegation &&
         embedding_origin == other->embedding_origin;
}

bool WebTestPermissionManager::PermissionDescription::operator!=(
    PermissionStatusSubscription* other) const {
  return !this->operator==(other);
}

size_t WebTestPermissionManager::PermissionDescription::Hash::operator()(
    const PermissionDescription& description) const {
  const int type_int = static_cast<int>(description.type);

  if (description.type == blink::PermissionType::STORAGE_ACCESS_GRANT) {
    const net::SchemefulSite requesting_site(description.origin);
    const net::SchemefulSite embedding_site(description.embedding_origin);
    const size_t hash =
        base::HashInts(type_int, base::FastHash(embedding_site.Serialize()));
    return base::HashInts(hash, base::FastHash(requesting_site.Serialize()));
  }

  const size_t hash = base::HashInts(
      type_int, base::FastHash(description.embedding_origin.spec()));
  return base::HashInts(hash, base::FastHash(description.origin.spec()));
}

WebTestPermissionManager::WebTestPermissionManager(
    BrowserContext& browser_context)
    : PermissionControllerDelegate(), browser_context_(browser_context) {}

WebTestPermissionManager::~WebTestPermissionManager() = default;

void WebTestPermissionManager::RequestPermissions(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(std::vector<blink::mojom::PermissionStatus>(
        request_description.permissions.size(),
        blink::mojom::PermissionStatus::DENIED));
    return;
  }

  std::vector<blink::mojom::PermissionStatus> result;
  result.reserve(request_description.permissions.size());
  const GURL& embedding_origin = PermissionUtil::GetLastCommittedOriginAsURL(
      render_frame_host->GetMainFrame());
  for (const auto& permission : request_description.permissions) {
    result.push_back(GetPermissionStatusForRequestPermission(
        permission, request_description.requesting_origin, embedding_origin));
  }

  std::move(callback).Run(result);
}

void WebTestPermissionManager::ResetPermission(blink::PermissionType permission,
                                               const GURL& requesting_origin,
                                               const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::AutoLock lock(permissions_lock_);

  const auto key =
      PermissionDescription(permission, requesting_origin, embedding_origin);
  if (!base::Contains(permissions_, key)) {
    return;
  }
  permissions_.erase(key);
}

void WebTestPermissionManager::RequestPermissionsFromCurrentDocument(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(std::vector<blink::mojom::PermissionStatus>(
        request_description.permissions.size(),
        blink::mojom::PermissionStatus::DENIED));
    return;
  }

  std::vector<blink::mojom::PermissionStatus> result;
  result.reserve(request_description.permissions.size());
  const GURL& embedding_origin = PermissionUtil::GetLastCommittedOriginAsURL(
      render_frame_host->GetMainFrame());
  for (const auto& permission : request_description.permissions) {
    result.push_back(GetPermissionStatusForRequestPermission(
        permission, request_description.requesting_origin, embedding_origin));
  }

  std::move(callback).Run(result);
}

blink::mojom::PermissionStatus
WebTestPermissionManager::GetPermissionStatusForRequestPermission(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));

  // The same-site auto-grant mechanism for STORAGE_ACCESS_GRANT currently only
  // works when requesting permissions.
  // TODO(crbug.com/40278136): maybe it should also work when querying
  // permissions.
  if (permission == blink::PermissionType::STORAGE_ACCESS_GRANT &&
      (requesting_origin == embedding_origin ||
       net::SchemefulSite(requesting_origin) ==
           net::SchemefulSite(embedding_origin))) {
    return blink::mojom::PermissionStatus::GRANTED;
  }

  return GetPermissionStatus(permission, requesting_origin, embedding_origin);
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

  // Some permissions (currently storage access related) do not expose the
  // denied state to avoid exposing potentially private user choices to
  // developers.
  if (ShouldHideDeniedState(permission) &&
      it->second == blink::mojom::PermissionStatus::DENIED) {
    return blink::mojom::PermissionStatus::ASK;
  }

  return it->second;
}

PermissionResult
WebTestPermissionManager::GetPermissionResultForOriginWithoutContext(
    blink::PermissionType permission,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  blink::mojom::PermissionStatus status = GetPermissionStatus(
      permission, requesting_origin.GetURL(), embedding_origin.GetURL());

  return PermissionResult(status, content::PermissionStatusSource::UNSPECIFIED);
}

blink::mojom::PermissionStatus
WebTestPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
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

blink::mojom::PermissionStatus
WebTestPermissionManager::GetPermissionStatusForEmbeddedRequester(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const url::Origin& overridden_origin) {
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    return blink::mojom::PermissionStatus::DENIED;
  }
  return GetPermissionStatus(permission, overridden_origin.GetURL(),
                             PermissionUtil::GetLastCommittedOriginAsURL(
                                 render_frame_host->GetMainFrame()));
}

void WebTestPermissionManager::OnPermissionStatusChangeSubscriptionAdded(
    content::PermissionController::SubscriptionId subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!subscriptions() || subscriptions()->IsEmpty()) {
    return;
  }
  content::PermissionStatusSubscription* subscription =
      subscriptions()->Lookup(subscription_id);
  if (!subscription) {
    return;
  }

  // If the request is from a worker, it won't have a RFH.
  GURL embedding_origin = subscription->requesting_origin;
  if (subscription->render_frame_id != -1) {
    subscription->embedding_origin = embedding_origin =
        PermissionUtil::GetLastCommittedOriginAsURL(
            content::RenderFrameHost::FromID(subscription->render_process_id,
                                             subscription->render_frame_id)
                ->GetMainFrame());
  }
  subscription->requesting_origin_delegation = subscription->requesting_origin;
  subscription->permission_result =
      PermissionResult(GetPermissionStatus(subscription->permission,
                                           subscription->requesting_origin,
                                           subscription->embedding_origin),
                       content::PermissionStatusSource::UNSPECIFIED);
}

void WebTestPermissionManager::UnsubscribeFromPermissionStatusChange(
    content::PermissionController::SubscriptionId subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  if (!subscriptions()) {
    return;
  }

  std::vector<base::OnceClosure> callbacks;
  callbacks.reserve(subscriptions()->size());

  for (content::PermissionController::SubscriptionsMap::iterator iter(
           subscriptions());
       !iter.IsAtEnd(); iter.Advance()) {
    PermissionStatusSubscription* subscription = iter.GetCurrentValue();
    if (permission != subscription) {
      continue;
    }

    if (subscription->permission_result &&
        subscription->permission_result->status == status) {
      continue;
    }

    subscription->permission_result =
        PermissionResult(status, PermissionStatusSource::UNSPECIFIED);

    // Add the callback to |callbacks| which will be run after the loop to
    // prevent re-entrance issues.
    callbacks.push_back(base::BindOnce(subscription->callback, status,
                                       /*ignore_status_override=*/false));
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
          ->SetContentSettings(
              ContentSettingsType::STORAGE_ACCESS,
              GetContentSettings(
                  ContentSettingsPattern::FromURLToSchemefulSitePattern(
                      permission.origin),
                  ContentSettingsPattern::FromURLToSchemefulSitePattern(
                      permission.embedding_origin),
                  status),
              base::BindOnce(std::move(permission_callback), /*success=*/true));
      break;
    case blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS: {
      // We dual-write `TOP_LEVEL_STORAGE_ACCESS` and `STORAGE_ACCESS_GRANT` due
      // to the former granting a superset of the latter. Accordingly, we wait
      // until both permissions have been written, including the notification to
      // the network service, to run the permission callback. This could happen
      // in either order without issue, so a barrier callback is used to ensure
      // whichever finishes last then runs the callback. The asynchronicity
      // comes in the form of the updates to the network service.
      auto barrier_callback = base::BarrierCallback<bool>(
          /*num_callbacks=*/3,
          base::BindOnce(
              [](blink::test::mojom::PermissionAutomation::SetPermissionCallback
                     permission_callback,
                 const std::vector<bool>& successes) {
                std::move(permission_callback)
                    .Run(base::ranges::all_of(successes, std::identity()));
              },
              std::move(permission_callback)));
      SetPermission(blink::PermissionType::STORAGE_ACCESS_GRANT,
                    blink::mojom::PermissionStatus::GRANTED, permission.origin,
                    permission.embedding_origin, barrier_callback);

      auto* cookie_manager = browser_context_->GetDefaultStoragePartition()
                                 ->GetCookieManagerForBrowserProcess();

      cookie_manager->SetContentSettings(
          ContentSettingsType::STORAGE_ACCESS,
          GetContentSettings(
              ContentSettingsPattern::FromURL(permission.origin),
              ContentSettingsPattern::FromURL(permission.embedding_origin),
              status),
          base::BindOnce(barrier_callback, true));

      cookie_manager->SetContentSettings(
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
          GetContentSettings(
              ContentSettingsPattern::FromURL(permission.origin),
              ContentSettingsPattern::FromURL(permission.embedding_origin),
              status),
          base::BindOnce(barrier_callback, true));

      break;
    }
    default:
      std::move(permission_callback).Run(true);
      break;
  }
}

}  // namespace content
