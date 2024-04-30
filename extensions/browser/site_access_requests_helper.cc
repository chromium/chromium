// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/site_access_requests_helper.h"

#include <sys/types.h>

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"

namespace extensions {

SiteAccessRequestsHelper::SiteAccessRequestsHelper(
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

SiteAccessRequestsHelper::~SiteAccessRequestsHelper() = default;

void SiteAccessRequestsHelper::AddRequest(const Extension& extension) {
  // Extension must not have granted access to the current site.
  auto site_access = permissions_manager_->GetSiteAccess(
      extension, web_contents_->GetLastCommittedURL());
  CHECK(!site_access.has_site_access && !site_access.has_all_sites_access);

  extensions_with_requests_.insert(extension.id());
}

bool SiteAccessRequestsHelper::RemoveRequest(const ExtensionId& extension_id) {
  if (!extensions_with_requests_.contains(extension_id)) {
    return false;
  }

  extensions_with_requests_.erase(extension_id);
  // TODO(crbug.com/330588494): Remove request from dismissed set, if existent,
  // once dismissed requests are moved to SiteAccessRequestsHelper.
  return true;
}

bool SiteAccessRequestsHelper::RemoveRequestIfGrantedAccess(
    const Extension& extension) {
  // Request is removed iff extension has access to the current site.
  const GURL& url = web_contents_->GetLastCommittedURL();
  PermissionsManager::ExtensionSiteAccess site_access =
      permissions_manager_->GetSiteAccess(extension, url);
  if (!site_access.has_site_access && !site_access.has_all_sites_access &&
      !permissions_manager_->HasActiveTabAndCanAccess(extension, url)) {
    return false;
  }

  return RemoveRequest(extension.id());
}

void SiteAccessRequestsHelper::UserDismissedRequest(
    const ExtensionId& extension_id) {
  CHECK(extensions_with_requests_.contains(extension_id));
  extensions_with_requests_dismissed_.insert(extension_id);
}

bool SiteAccessRequestsHelper::HasActiveRequest(
    const ExtensionId& extension_id) {
  return extensions_with_requests_.contains(extension_id) &&
         !extensions_with_requests_dismissed_.contains(extension_id);
}

bool SiteAccessRequestsHelper::HasRequests() {
  return !extensions_with_requests_.empty();
}

void SiteAccessRequestsHelper::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  RemoveRequest(extension->id());

  if (!HasRequests()) {
    permissions_manager_->DeleteSiteAccessRequestHelperFor(tab_id_);
    // IMPORTANT: This object is now deleted and is unsafe to use.
  }
}

void SiteAccessRequestsHelper::DidFinishNavigation(
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

  permissions_manager_->NotifySiteAccessRequestsCleared(tab_id_);
  permissions_manager_->DeleteSiteAccessRequestHelperFor(tab_id_);
  // IMPORTANT: This object is now deleted and is unsafe to use.
}

void SiteAccessRequestsHelper::WebContentsDestroyed() {
  // Delete web contents pointer so it's not dangling at helper's destruction.
  web_contents_ = nullptr;

  extensions_with_requests_.clear();
  extensions_with_requests_dismissed_.clear();

  permissions_manager_->DeleteSiteAccessRequestHelperFor(tab_id_);
  // IMPORTANT: This object is now deleted and is unsafe to use.
}

}  // namespace extensions
