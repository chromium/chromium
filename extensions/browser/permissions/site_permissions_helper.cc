// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions/site_permissions_helper.h"

#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/browser/permissions/scripting_permissions_modifier.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// A preference indicating if the extension can show site access requests
// directly in the toolbar next to the omnibox.
constexpr const char kPrefShowAccessRequestsInToolbar[] =
    "show_access_requests_in_toolbar";

// The blocked actions that require a page refresh to run.
constexpr int kRefreshRequiredActionsMask =
    BLOCKED_ACTION_WEB_REQUEST | BLOCKED_ACTION_SCRIPT_AT_START;

}  // namespace

SitePermissionsHelper::SitePermissionsHelper(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

SitePermissionsHelper::~SitePermissionsHelper() = default;

SitePermissionsHelper::SiteInteraction
SitePermissionsHelper::GetSiteInteraction(
    const Extension& extension,
    content::WebContents* web_contents) const {
  if (!web_contents) {
    return SiteInteraction::kNone;
  }

  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  const GURL& url = web_contents->GetLastCommittedURL();
  PermissionsData::PageAccess page_access =
      extension.permissions_data()->GetPageAccess(url, tab_id,
                                                  /*error=*/nullptr);
  PermissionsData::PageAccess script_access =
      extension.permissions_data()->GetContentScriptAccess(url, tab_id,
                                                           /*error=*/nullptr);

  if (page_access == PermissionsData::PageAccess::kAllowed ||
      script_access == PermissionsData::PageAccess::kAllowed) {
    return SiteInteraction::kGranted;
  }

  // An extension can request both host permissions and activeTab permission.
  // Withholding a host permission takes priority over activeTab permission,
  // because withheld hosts are hosts that the extension explicitly marked as
  // 'required' permissions, so it is a stronger signal that the extension
  // should run on the site. ActiveTab extensions, by contrast, are designed to
  // run when the user explicitly invokes them.
  // TODO(tjudkins): Investigate if we need to check HasBeenBlocked() for this
  // case. We do know that extensions that have been blocked should always be
  // marked pending, but those cases should be covered by the withheld page
  // access checks.
  if (page_access == PermissionsData::PageAccess::kWithheld ||
      script_access == PermissionsData::PageAccess::kWithheld ||
      HasBeenBlocked(extension, web_contents)) {
    return SiteInteraction::kWithheld;
  }

  if (PermissionsManager::Get(browser_context_)
          ->HasActiveTabAndCanAccess(extension, url)) {
    return SiteInteraction::kActiveTab;
  }

  return SiteInteraction::kNone;
}

void SitePermissionsHelper::UpdateSiteAccess(
    const Extension& extension,
    content::WebContents* web_contents,
    PermissionsManager::UserSiteAccess new_access) {
  std::vector<const Extension*> extensions = {&extension};
  UpdateSiteAccess(extensions, web_contents, new_access);
}

void SitePermissionsHelper::UpdateSiteAccess(
    const std::vector<const Extension*>& extensions,
    content::WebContents* web_contents,
    PermissionsManager::UserSiteAccess new_access) {
  auto current_url = web_contents->GetLastCommittedURL();

  auto* permissions_manager = PermissionsManager::Get(browser_context_);
  bool reload_required = false;

  for (auto const* extension : extensions) {
    auto current_access =
        permissions_manager->GetUserSiteAccess(*extension, current_url);
    if (new_access == current_access) {
      break;
    }

    CHECK(permissions_manager->CanAffectExtension(*extension));
    CHECK(permissions_manager->CanUserSelectSiteAccess(*extension, current_url,
                                                       new_access));

    // Store/remove whether the extension has broad site access so we can
    // preserve the setting when toggling an extension's site access back on.
    if (current_access == PermissionsManager::UserSiteAccess::kOnAllSites) {
      permissions_manager->AddExtensionToPreviousBroadSiteAccessSet(
          extension->id());
    } else {
      permissions_manager->RemoveExtensionFromPreviousBroadSiteAccessSet(
          extension->id());
    }

    // Update the extension's site access.
    ScriptingPermissionsModifier modifier(browser_context_, extension);
    switch (new_access) {
      case PermissionsManager::UserSiteAccess::kOnClick:
        if (permissions_manager->HasBroadGrantedHostPermissions(*extension)) {
          modifier.RemoveBroadGrantedHostPermissions();
        }
        // Note: SetWithholdHostPermissions() is a no-op if host permissions are
        // already being withheld.
        modifier.SetWithholdHostPermissions(true);
        if (permissions_manager->HasGrantedHostPermission(*extension,
                                                          current_url)) {
          modifier.RemoveGrantedHostPermission(current_url);
        }
        break;
      case PermissionsManager::UserSiteAccess::kOnSite:
        if (permissions_manager->HasBroadGrantedHostPermissions(*extension)) {
          modifier.RemoveBroadGrantedHostPermissions();
        }
        // Note: SetWithholdHostPermissions() is a no-op if host permissions are
        // already being withheld.
        modifier.SetWithholdHostPermissions(true);
        if (!permissions_manager->HasGrantedHostPermission(*extension,
                                                           current_url)) {
          modifier.GrantHostPermission(current_url);
        }
        break;
      case PermissionsManager::UserSiteAccess::kOnAllSites:
        modifier.SetWithholdHostPermissions(false);
        break;
    }

    // Clear extension's tab permission when revoking user site permissions.
    bool revoking_current_site_permissions =
        new_access == PermissionsManager::UserSiteAccess::kOnClick;
    if (revoking_current_site_permissions) {
      ActiveTabPermissionGranter::FromWebContents(web_contents)
          ->ClearActiveExtensionAndNotify(extension->id());
      // While revoking permissions doesn't necessarily mandate a page
      // refresh, it is complicated to determine when an extension has affected
      // the page. Showing a reload page bubble after the user blocks the
      // extension re enforces the user confidence on blocking the extension.
      // Also, this scenario should not be that common and therefore hopefully
      // is not too noisy.
      reload_required = true;
      break;
    }

    // Reload bubble will be shown when blocked actions mandate a page refresh.
    // Refreshing the page will run them.
    ExtensionsBrowserClient::Get()->RunBlockActionsIfNeeded(
        extension, web_contents, this, &reload_required);
  }

  if (reload_required) {
    // Show the reload bubble for all extensions, since it could be confusing to
    // the user why only some of them appear on the dialog.
    ExtensionsBrowserClient::Get()->ShowReloadBubbleForAllExtensions(
        extensions, web_contents);
  }
}

bool SitePermissionsHelper::PageNeedsRefreshToRun(int blocked_actions) {
  return blocked_actions & kRefreshRequiredActionsMask;
}

bool SitePermissionsHelper::HasBeenBlocked(
    const Extension& extension,
    content::WebContents* web_contents) const {
  return ExtensionsBrowserClient::Get()->HasBeenBlocked(extension,
                                                        web_contents);
}

bool SitePermissionsHelper::ShowAccessRequestsInToolbar(
    const std::string& extension_id) {
  // By default, extensions requesting access should be visible in toolbar,
  // otherwise the user would most likely never grant the extensions access.
  bool show_access_requests = true;

  ExtensionPrefs::Get(browser_context_)
      ->ReadPrefAsBoolean(extension_id, kPrefShowAccessRequestsInToolbar,
                          &show_access_requests);
  return show_access_requests;
}

void SitePermissionsHelper::SetShowAccessRequestsInToolbar(
    const std::string& extension_id,
    bool show_access_requests_in_toolbar) {
  ExtensionPrefs::Get(browser_context_)
      ->UpdateExtensionPref(extension_id, kPrefShowAccessRequestsInToolbar,
                            base::Value(show_access_requests_in_toolbar));
  PermissionsManager::Get(browser_context_)
      ->NotifyShowAccessRequestsInToolbarChanged(
          extension_id, show_access_requests_in_toolbar);
}

}  // namespace extensions
