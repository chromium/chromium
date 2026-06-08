// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/host_access_request_helper.h"

#include <sys/types.h>

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/url_pattern.h"

namespace extensions {

base::TimeDelta HostAccessRequestsHelper::cooldown_duration_ = base::Seconds(1);

// static
void HostAccessRequestsHelper::SetCooldownForTesting(base::TimeDelta cooldown) {
  cooldown_duration_ = cooldown;
}

HostAccessRequestsHelper::HostAccessRequestsHelper(
    PassKey pass_key,
    PermissionsManager* permissions_manager,
    content::WebContents* web_contents,
    int tab_id)
    : content::WebContentsObserver(web_contents),
      permissions_manager_(permissions_manager),
      web_contents_(web_contents),
      tab_id_(tab_id) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(web_contents->GetBrowserContext()));
}

HostAccessRequestsHelper::~HostAccessRequestsHelper() = default;

HostAccessRequestsHelper::AddRequestResult HostAccessRequestsHelper::AddRequest(
    const Extension& extension,
    const std::optional<URLPattern>& filter) {
  // Extension must not have granted access to the current site.
  auto site_access = permissions_manager_->GetSiteAccess(
      extension, web_contents_->GetLastCommittedURL());
  CHECK(!site_access.has_site_access && !site_access.has_all_sites_access);

  if (extensions_with_requests_.contains(extension.id())) {
    return AddRequestResult::kDuplicate;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  auto time_iter = last_request_times_.find(extension.id());
  if (time_iter != last_request_times_.end() &&
      now - time_iter->second < cooldown_duration_) {
    return AddRequestResult::kThrottled;
  }
  last_request_times_[extension.id()] = now;

  extensions_with_requests_.insert({extension.id(), filter});
  return AddRequestResult::kSuccess;
}

void HostAccessRequestsHelper::UpdateRequest(
    const Extension& extension,
    const std::optional<URLPattern>& filter) {
  // We can only update a request if there is an existent one.
  CHECK(HasRequest(extension.id()));

  // Extension must not have granted access to the current site.
  auto site_access = permissions_manager_->GetSiteAccess(
      extension, web_contents_->GetLastCommittedURL());
  CHECK(!site_access.has_site_access && !site_access.has_all_sites_access);

  extensions_with_requests_.at(extension.id()) = filter;
}

HostAccessRequestsHelper::RemoveRequestResult
HostAccessRequestsHelper::RemoveRequest(const ExtensionId& extension_id,
                                        const std::optional<URLPattern>& filter,
                                        bool bypass_cooldown) {
  auto requests_iter = extensions_with_requests_.find(extension_id);
  if (requests_iter == extensions_with_requests_.end()) {
    return RemoveRequestResult::kNotFound;
  }

  // Remove request iff it matches the parameter when given. Otherwise, always
  // remove the request.
  if (!filter || requests_iter->second == filter) {
    base::TimeTicks now = base::TimeTicks::Now();
    auto time_iter = last_request_times_.find(extension_id);
    if (!bypass_cooldown && time_iter != last_request_times_.end() &&
        now - time_iter->second < cooldown_duration_) {
      return RemoveRequestResult::kThrottled;
    }
    if (!bypass_cooldown) {
      last_request_times_[extension_id] = now;
    }

    extensions_with_requests_.erase(extension_id);
    return RemoveRequestResult::kSuccess;
  }

  return RemoveRequestResult::kNotFound;
}

bool HostAccessRequestsHelper::RemoveRequestIfGrantedAccess(
    const Extension& extension) {
  // Request is removed iff extension has access to the current site.
  const GURL& url = web_contents_->GetLastCommittedURL();
  PermissionsManager::ExtensionSiteAccess site_access =
      permissions_manager_->GetSiteAccess(extension, url);
  if (!site_access.has_site_access && !site_access.has_all_sites_access &&
      !permissions_manager_->HasActiveTabAndCanAccess(extension, url)) {
    return false;
  }

  // Cooldown is bypassed because access has been granted, so the request is
  // no longer needed and should be removed immediately.
  return RemoveRequest(extension.id(), /*filter=*/std::nullopt,
                       /*bypass_cooldown=*/true) ==
         RemoveRequestResult::kSuccess;
}

void HostAccessRequestsHelper::UserDismissedRequest(
    const ExtensionId& extension_id) {
  CHECK(extensions_with_requests_.contains(extension_id));

  // Remove the request from the active list, bypassing the cooldown to ensure
  // the user can always dismiss it immediately.
  RemoveRequest(extension_id, std::nullopt, /*bypass_cooldown=*/true);

  extensions_with_requests_dismissed_.insert(extension_id);

  // Manually update the timestamp since RemoveRequest with bypass_cooldown=true
  // skips it. This prevents the extension from immediately re-adding the
  // request right after the user's dismissal.
  last_request_times_[extension_id] = base::TimeTicks::Now();
}

bool HostAccessRequestsHelper::HasRequest(
    const ExtensionId& extension_id) const {
  return extensions_with_requests_.contains(extension_id);
}

bool HostAccessRequestsHelper::HasActiveRequest(
    const ExtensionId& extension_id) const {
  if (!extensions_with_requests_.contains(extension_id)) {
    return false;
  }

  if (extensions_with_requests_dismissed_.contains(extension_id)) {
    return false;
  }

  // Web contents must match the filter if provided, otherwise request is always
  // active.
  std::optional<URLPattern> filter = extensions_with_requests_.at(extension_id);
  return !filter ||
         filter.value().MatchesURL(web_contents_->GetLastCommittedURL());
}

bool HostAccessRequestsHelper::HasRequests() {
  return !extensions_with_requests_.empty();
}

void HostAccessRequestsHelper::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Cooldown is bypassed during extension unloading. This ensures the request
  // is successfully cleaned up and doesn't remain as a zombie request in the
  // helper if the extension is unloaded within the 1-second cooldown window.
  RemoveRequest(extension->id(), /*filter=*/std::nullopt,
                /*bypass_cooldown=*/true);

  if (!HasRequests()) {
    permissions_manager_->DeleteHostAccessRequestHelperFor(tab_id_);
    // IMPORTANT: This object is now deleted and is unsafe to use.
  }
}

void HostAccessRequestsHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Sub-frames don't get specific requests.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Only clear requests for cross-origin navigations.
  if (navigation_handle->IsSameOrigin()) {
    return;
  }

  extensions_with_requests_.clear();
  extensions_with_requests_dismissed_.clear();

  permissions_manager_->NotifyHostAccessRequestsCleared(tab_id_);
  permissions_manager_->DeleteHostAccessRequestHelperFor(tab_id_);
  // IMPORTANT: This object is now deleted and is unsafe to use.
}

void HostAccessRequestsHelper::WebContentsDestroyed() {
  // Delete web contents pointer so it's not dangling at helper's destruction.
  web_contents_ = nullptr;

  extensions_with_requests_.clear();
  extensions_with_requests_dismissed_.clear();

  permissions_manager_->DeleteHostAccessRequestHelperFor(tab_id_);
  // IMPORTANT: This object is now deleted and is unsafe to use.
}

}  // namespace extensions
