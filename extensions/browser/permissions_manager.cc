// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {

// Entries of `kUserPermissions` dictionary.
const char kRestrictedSites[] = "restricted_sites";
const char kPermittedSites[] = "permitted_sites";

// Sets `pref` in `extension_prefs` if it doesn't exist, and appends
// `origin` to its list.
void AddSiteToPrefs(ExtensionPrefs* extension_prefs,
                    const char* pref,
                    const url::Origin& origin) {
  std::unique_ptr<prefs::ScopedDictionaryPrefUpdate> update =
      extension_prefs->CreatePrefUpdate(kUserPermissions);
  base::ListValue* list = nullptr;

  bool pref_exists = (*update)->GetList(pref, &list);
  if (pref_exists) {
    list->Append(origin.Serialize());
  } else {
    auto sites = std::make_unique<base::Value>(base::Value::Type::LIST);
    sites->Append(origin.Serialize());
    (*update)->Set(pref, std::move(sites));
  }
}

// Removes `origin` from `pref` in `extension_prefs`.
void RemoveSiteFromPrefs(ExtensionPrefs* extension_prefs,
                         const char* pref,
                         const url::Origin& origin) {
  std::unique_ptr<prefs::ScopedDictionaryPrefUpdate> update =
      extension_prefs->CreatePrefUpdate(kUserPermissions);
  base::ListValue* list;
  (*update)->GetList(pref, &list);
  list->EraseListValue(base::Value(origin.Serialize()));
}

// Returns sites from `pref` in `extension_prefs`.
std::set<url::Origin> GetSitesFromPrefs(ExtensionPrefs* extension_prefs,
                                        const char* pref) {
  const base::Value* user_permissions =
      extension_prefs->GetPrefAsDictionary(kUserPermissions);
  std::set<url::Origin> sites;

  auto* list = user_permissions->FindListKey(pref);
  if (!list)
    return sites;

  for (const auto& site : list->GetListDeprecated()) {
    const std::string* site_as_string = site.GetIfString();
    if (!site_as_string)
      continue;

    GURL site_as_url(*site_as_string);
    if (!site_as_url.is_valid())
      continue;

    url::Origin origin = url::Origin::Create(site_as_url);
    sites.insert(origin);
  }
  return sites;
}

class PermissionsManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  PermissionsManagerFactory();
  ~PermissionsManagerFactory() override = default;
  PermissionsManagerFactory(const PermissionsManagerFactory&) = delete;
  const PermissionsManagerFactory& operator=(const PermissionsManagerFactory&) =
      delete;

  PermissionsManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  // BrowserContextKeyedServiceFactory
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* browser_context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
};

PermissionsManagerFactory::PermissionsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PermissionsManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

PermissionsManager* PermissionsManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<PermissionsManager*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

content::BrowserContext* PermissionsManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context);
}

KeyedService* PermissionsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new PermissionsManager(browser_context);
}

}  // namespace

// Implementation of UserPermissionsSettings.
PermissionsManager::UserPermissionsSettings::UserPermissionsSettings() =
    default;

PermissionsManager::UserPermissionsSettings::~UserPermissionsSettings() =
    default;

// Implementation of PermissionsManager.
PermissionsManager::PermissionsManager(content::BrowserContext* browser_context)
    : extension_prefs_(ExtensionPrefs::Get(browser_context)) {
  user_permissions_.restricted_sites =
      GetSitesFromPrefs(extension_prefs_, kRestrictedSites);
  user_permissions_.permitted_sites =
      GetSitesFromPrefs(extension_prefs_, kPermittedSites);
}

PermissionsManager::~PermissionsManager() {
  user_permissions_.restricted_sites.clear();
  user_permissions_.permitted_sites.clear();
}

// static
PermissionsManager* PermissionsManager::Get(
    content::BrowserContext* browser_context) {
  return static_cast<PermissionsManagerFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* PermissionsManager::GetFactory() {
  static base::NoDestructor<PermissionsManagerFactory> g_factory;
  return g_factory.get();
}

// static
void PermissionsManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kUserPermissions.name);
}

void PermissionsManager::AddUserRestrictedSite(const url::Origin& origin) {
  if (base::Contains(user_permissions_.restricted_sites, origin))
    return;

  // Origin cannot be both restricted and permitted.
  RemovePermittedSiteAndUpdatePrefs(origin);

  user_permissions_.restricted_sites.insert(origin);
  AddSiteToPrefs(extension_prefs_, kRestrictedSites, origin);
  SignalUserPermissionsSettingsChanged();
}

void PermissionsManager::RemoveUserRestrictedSite(const url::Origin& origin) {
  if (RemoveRestrictedSiteAndUpdatePrefs(origin))
    SignalUserPermissionsSettingsChanged();
}

void PermissionsManager::AddUserPermittedSite(const url::Origin& origin) {
  if (base::Contains(user_permissions_.permitted_sites, origin))
    return;

  // Origin cannot be both restricted and permitted.
  RemoveRestrictedSiteAndUpdatePrefs(origin);

  user_permissions_.permitted_sites.insert(origin);
  AddSiteToPrefs(extension_prefs_, kPermittedSites, origin);
  SignalUserPermissionsSettingsChanged();
}

void PermissionsManager::RemoveUserPermittedSite(const url::Origin& origin) {
  if (RemovePermittedSiteAndUpdatePrefs(origin))
    SignalUserPermissionsSettingsChanged();
}

const PermissionsManager::UserPermissionsSettings&
PermissionsManager::GetUserPermissionsSettings() const {
  return user_permissions_;
}

PermissionsManager::UserSiteSetting PermissionsManager::GetUserSiteSetting(
    const url::Origin& origin) const {
  if (user_permissions_.permitted_sites.find(origin) !=
      user_permissions_.permitted_sites.end()) {
    return UserSiteSetting::kGrantAllExtensions;
  }
  if (user_permissions_.restricted_sites.find(origin) !=
      user_permissions_.restricted_sites.end()) {
    return UserSiteSetting::kBlockAllExtensions;
  }
  return UserSiteSetting::kCustomizeByExtension;
}

PermissionsManager::ExtensionSiteAccess PermissionsManager::GetSiteAccess(
    const Extension& extension,
    const GURL& url) const {
  PermissionsManager::ExtensionSiteAccess extension_access;

  // Awkward holder object because permission sets are immutable, and when
  // return from prefs, ownership is passed.
  std::unique_ptr<const PermissionSet> permission_holder;

  const PermissionSet* granted_permissions = nullptr;
  if (!HasWithheldHostPermissions(extension.id())) {
    // If the extension doesn't have any withheld permissions, we look at the
    // current active permissions.
    // TODO(devlin): This is clunky. It would be nice to have runtime-granted
    // permissions be correctly populated in all cases, rather than looking at
    // two different sets.
    // TODO(devlin): This won't account for granted permissions that aren't
    // currently active, even though the extension may re-request them (and be
    // silently granted them) at any time.
    granted_permissions = &extension.permissions_data()->active_permissions();
  } else {
    permission_holder = GetRuntimePermissionsFromPrefs(extension);
    granted_permissions = permission_holder.get();
  }

  DCHECK(granted_permissions);

  const bool is_restricted_site =
      extension.permissions_data()->IsRestrictedUrl(url, /*error=*/nullptr);

  // For indicating whether an extension has access to a site, we look at the
  // granted permissions, which could include patterns that weren't explicitly
  // requested. However, we should still indicate they are granted, so that the
  // user can revoke them (and because if the extension does request them and
  // they are already granted, they are silently added).
  // The extension should never have access to restricted sites (even if a
  // pattern matches, as it may for e.g. the webstore).
  if (!is_restricted_site &&
      granted_permissions->effective_hosts().MatchesSecurityOrigin(url)) {
    extension_access.has_site_access = true;
  }

  const PermissionSet& withheld_permissions =
      extension.permissions_data()->withheld_permissions();

  // Be sure to check |access.has_site_access| in addition to withheld
  // permissions, so that we don't indicate we've withheld permission if an
  // extension is granted https://a.com/*, but has *://*/* withheld.
  // We similarly don't show access as withheld for restricted sites, since
  // withheld permissions should only include those that are conceivably
  // grantable.
  if (!is_restricted_site && !extension_access.has_site_access &&
      withheld_permissions.effective_hosts().MatchesSecurityOrigin(url)) {
    extension_access.withheld_site_access = true;
  }

  constexpr bool include_api_permissions = false;
  if (granted_permissions->ShouldWarnAllHosts(include_api_permissions))
    extension_access.has_all_sites_access = true;

  if (withheld_permissions.ShouldWarnAllHosts(include_api_permissions) &&
      !extension_access.has_all_sites_access) {
    extension_access.withheld_all_sites_access = true;
  }

  return extension_access;
}

bool PermissionsManager::HasWithheldHostPermissions(
    const ExtensionId& extension_id) const {
  return extension_prefs_->GetWithholdingPermissions(extension_id);
}

std::unique_ptr<const PermissionSet>
PermissionsManager::GetRuntimePermissionsFromPrefs(
    const Extension& extension) const {
  std::unique_ptr<const PermissionSet> permissions =
      extension_prefs_->GetRuntimeGrantedPermissions(extension.id());

  // If there are no stored permissions, there's nothing to adjust.
  if (!permissions)
    return nullptr;

  // If the extension is allowed to run on chrome:// URLs, then we don't have
  // to adjust anything.
  if (PermissionsData::AllUrlsIncludesChromeUrls(extension.id()))
    return permissions;

  // We need to adjust a pattern if it matches all URLs and includes the
  // chrome:-scheme. These patterns would otherwise match hosts like
  // chrome://settings, which should not be allowed.
  // NOTE: We don't need to adjust for the file scheme, because
  // ExtensionPrefs properly does that based on the extension's file access.
  auto needs_chrome_scheme_adjustment = [](const URLPattern& pattern) {
    return pattern.match_all_urls() &&
           ((pattern.valid_schemes() & URLPattern::SCHEME_CHROMEUI) != 0);
  };

  // NOTE: We don't need to check scriptable_hosts, because the default
  // scriptable_hosts scheme mask omits the chrome:-scheme in normal
  // circumstances (whereas the default explicit scheme does not, in order to
  // allow for patterns like chrome://favicon).

  bool needs_adjustment = std::any_of(permissions->explicit_hosts().begin(),
                                      permissions->explicit_hosts().end(),
                                      needs_chrome_scheme_adjustment);
  // If no patterns need adjustment, return the original set.
  if (!needs_adjustment)
    return permissions;

  // Otherwise, iterate over the explicit hosts, and modify any that need to be
  // tweaked, adding back in permitted chrome:-scheme hosts. This logic mirrors
  // that in PermissionsParser, and is also similar to logic in
  // permissions_api_helpers::UnpackOriginPermissions(), and has some overlap
  // to URLPatternSet::Populate().
  // TODO(devlin): ^^ Ouch. Refactor so that this isn't duplicated.
  URLPatternSet new_explicit_hosts;
  for (const auto& pattern : permissions->explicit_hosts()) {
    if (!needs_chrome_scheme_adjustment(pattern)) {
      new_explicit_hosts.AddPattern(pattern);
      continue;
    }

    URLPattern new_pattern(pattern);
    int new_valid_schemes =
        pattern.valid_schemes() & ~URLPattern::SCHEME_CHROMEUI;
    new_pattern.SetValidSchemes(new_valid_schemes);
    new_explicit_hosts.AddPattern(std::move(new_pattern));
  }

  return std::make_unique<PermissionSet>(
      permissions->apis().Clone(), permissions->manifest_permissions().Clone(),
      std::move(new_explicit_hosts), permissions->scriptable_hosts().Clone());
}

void PermissionsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PermissionsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PermissionsManager::SignalUserPermissionsSettingsChanged() const {
  for (auto& observer : observers_)
    observer.UserPermissionsSettingsChanged(GetUserPermissionsSettings());
}

bool PermissionsManager::RemovePermittedSiteAndUpdatePrefs(
    const url::Origin& origin) {
  bool removed_site = user_permissions_.permitted_sites.erase(origin);
  if (removed_site)
    RemoveSiteFromPrefs(extension_prefs_, kPermittedSites, origin);

  return removed_site;
}

bool PermissionsManager::RemoveRestrictedSiteAndUpdatePrefs(
    const url::Origin& origin) {
  bool removed_site = user_permissions_.restricted_sites.erase(origin);
  if (removed_site)
    RemoveSiteFromPrefs(extension_prefs_, kRestrictedSites, origin);

  return removed_site;
}

}  // namespace extensions
