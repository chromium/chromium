// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"

#include <memory>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/network_permissions_updater.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/pref_types.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/site_access_requests_helper.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/mojom/renderer.mojom.h"
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
  base::Value::List* list = nullptr;

  bool pref_exists = (*update)->GetListWithoutPathExpansion(pref, &list);
  if (pref_exists) {
    list->Append(origin.Serialize());
  } else {
    base::Value::List sites;
    sites.Append(origin.Serialize());
    (*update)->SetKey(pref, base::Value(std::move(sites)));
  }
}

// Removes `origin` from `pref` in `extension_prefs`.
void RemoveSiteFromPrefs(ExtensionPrefs* extension_prefs,
                         const char* pref,
                         const url::Origin& origin) {
  std::unique_ptr<prefs::ScopedDictionaryPrefUpdate> update =
      extension_prefs->CreatePrefUpdate(kUserPermissions);
  base::Value::List* list = nullptr;
  (*update)->GetListWithoutPathExpansion(pref, &list);
  DCHECK(list);
  list->EraseValue(base::Value(origin.Serialize()));
}

// Returns sites from `pref` in `extension_prefs`.
std::set<url::Origin> GetSitesFromPrefs(ExtensionPrefs* extension_prefs,
                                        const char* pref) {
  const base::Value::Dict& user_permissions =
      extension_prefs->GetPrefAsDictionary(kUserPermissions);
  std::set<url::Origin> sites;

  auto* list = user_permissions.FindList(pref);
  if (!list)
    return sites;

  for (const auto& site : *list) {
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

// Returns the set of permissions that the extension is allowed to have after
// withholding any that should not be granted. `desired_permissions` is the set
// of permissions the extension wants, `runtime_granted_permissions` are the
// permissions the user explicitly granted the extension at runtime, and
// `user_granted_permissions` are permissions that the user has indicated any
// extension may have.
// This should only be called for extensions that have permissions withheld.
std::unique_ptr<PermissionSet> GetAllowedPermissionsAfterWithholding(
    const PermissionSet& desired_permissions,
    const PermissionSet& runtime_granted_permissions,
    const PermissionSet& user_granted_permissions) {
  // 1) Take the set of all allowed permissions. This is the union of
  //    runtime-granted permissions (where the user said "this extension may run
  //    on this site") and `user_granted_permissions` (sites the user allows any
  //    extension to run on).
  std::unique_ptr<PermissionSet> allowed_permissions =
      PermissionSet::CreateUnion(user_granted_permissions,
                                 runtime_granted_permissions);

  // 2) Add in any always-approved hosts that shouldn't be removed (such as
  //    chrome://favicon).
  ExtensionsBrowserClient::Get()->AddAdditionalAllowedHosts(
      desired_permissions, allowed_permissions.get());

  // 3) Finalize the allowed set. Since we don't allow withholding of API and
  //    manifest permissions, the allowed set always contains all (bounded)
  //    requested API and manifest permissions.
  allowed_permissions->SetAPIPermissions(desired_permissions.apis().Clone());
  allowed_permissions->SetManifestPermissions(
      desired_permissions.manifest_permissions().Clone());

  // 4) Calculate the set of permissions to give to the extension. This is the
  //    intersection of all permissions the extension is allowed to have
  //    (`allowed_permissions`) with all permissions the extension elected to
  //    have (`desired_permissions`).
  //    Said differently, we grant a permission if both the extension and the
  //    user approved it.
  return PermissionSet::CreateIntersection(
      *allowed_permissions, desired_permissions,
      URLPatternSet::IntersectionBehavior::kDetailed);
}

// Adjusts host patterns if they match all URLs and include the chrome:-scheme.
// These patterns would otherwise match hosts like chrome://settings, which
// should not be allowed.
std::unique_ptr<PermissionSet> AdjustHostPatterns(
    std::unique_ptr<PermissionSet> permissions,
    const ExtensionId& id) {
  // If there are no stored permissions, there's nothing to adjust.
  if (!permissions) {
    return nullptr;
  }

  // If the extension is allowed to run on chrome:// URLs, then we don't have
  // to adjust anything.
  if (PermissionsData::AllUrlsIncludesChromeUrls(id)) {
    return permissions;
  }

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

  bool needs_adjustment = base::ranges::any_of(permissions->explicit_hosts(),
                                               needs_chrome_scheme_adjustment);
  // If no patterns need adjustment, return the original set.
  if (!needs_adjustment) {
    return permissions;
  }

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

  permissions->SetExplicitHosts(std::move(new_explicit_hosts));
  return permissions;
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
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      browser_context, /*force_guest_profile=*/true);
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
    : browser_context_(browser_context),
      extension_prefs_(ExtensionPrefs::Get(browser_context)) {
  user_permissions_.restricted_sites =
      GetSitesFromPrefs(extension_prefs_, kRestrictedSites);
  if (base::FeatureList::IsEnabled(
          extensions_features::
              kExtensionsMenuAccessControlWithPermittedSites)) {
    user_permissions_.permitted_sites =
        GetSitesFromPrefs(extension_prefs_, kPermittedSites);
  }
}

PermissionsManager::~PermissionsManager() {
  user_permissions_.restricted_sites.clear();
  user_permissions_.permitted_sites.clear();
  requests_helpers_.clear();
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

void PermissionsManager::UpdateUserSiteSetting(const url::Origin& origin,
                                               UserSiteSetting site_setting) {
  switch (site_setting) {
    case UserSiteSetting::kGrantAllExtensions:
      // Granting access to all extensions is allowed iff feature is
      // enabled.
      DCHECK(base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControlWithPermittedSites));
      AddUserPermittedSite(origin);
      break;
    case UserSiteSetting::kBlockAllExtensions:
      AddUserRestrictedSite(origin);
      break;
    case UserSiteSetting::kCustomizeByExtension:
      if (base::FeatureList::IsEnabled(
              extensions_features::
                  kExtensionsMenuAccessControlWithPermittedSites)) {
        RemoveUserPermittedSite(origin);
      }
      RemoveUserRestrictedSite(origin);
      break;
  }
}

void PermissionsManager::AddUserRestrictedSite(const url::Origin& origin) {
  if (base::Contains(user_permissions_.restricted_sites, origin))
    return;

  // Origin cannot be both restricted and permitted.
  RemovePermittedSiteAndUpdatePrefs(origin);

  user_permissions_.restricted_sites.insert(origin);
  AddSiteToPrefs(extension_prefs_, kRestrictedSites, origin);
  OnUserPermissionsSettingsChanged();
}

void PermissionsManager::RemoveUserRestrictedSite(const url::Origin& origin) {
  if (RemoveRestrictedSiteAndUpdatePrefs(origin))
    OnUserPermissionsSettingsChanged();
}

void PermissionsManager::AddUserPermittedSite(const url::Origin& origin) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControlWithPermittedSites));

  if (base::Contains(user_permissions_.permitted_sites, origin)) {
    return;
  }

  // Origin cannot be both restricted and permitted.
  RemoveRestrictedSiteAndUpdatePrefs(origin);

  user_permissions_.permitted_sites.insert(origin);
  AddSiteToPrefs(extension_prefs_, kPermittedSites, origin);

  OnUserPermissionsSettingsChanged();
}

void PermissionsManager::UpdatePermissionsWithUserSettings(
    const Extension& extension,
    const PermissionSet& user_permitted_set) {
  // If either user cannot be affected by hbe affected by host permissions
  // policy-installed extensions) or the user has not withheld any permissions
  // for the extension, then we don't need to do anything - the extension
  // already has all its requested permissions.
  if (!CanAffectExtension(extension) ||
      !HasWithheldHostPermissions(extension)) {
    return;
  }

  std::unique_ptr<PermissionSet> new_active_permissions =
      GetAllowedPermissionsAfterWithholding(
          *GetBoundedExtensionDesiredPermissions(extension),
          *GetRuntimePermissionsFromPrefs(extension), user_permitted_set);

  // Calculate the new withheld permissions; these are any required permissions
  // that are not in the new active set.
  std::unique_ptr<PermissionSet> new_withheld_permissions =
      PermissionSet::CreateDifference(
          PermissionsParser::GetRequiredPermissions(&extension),
          *new_active_permissions);

  // Set the new permissions on the extension.
  extension.permissions_data()->SetPermissions(
      std::move(new_active_permissions), std::move(new_withheld_permissions));
}

void PermissionsManager::RemoveUserPermittedSite(const url::Origin& origin) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControlWithPermittedSites));

  if (RemovePermittedSiteAndUpdatePrefs(origin))
    OnUserPermissionsSettingsChanged();
}

const PermissionsManager::UserPermissionsSettings&
PermissionsManager::GetUserPermissionsSettings() const {
  return user_permissions_;
}

PermissionsManager::UserSiteSetting PermissionsManager::GetUserSiteSetting(
    const url::Origin& origin) const {
  if (base::Contains(user_permissions_.permitted_sites, origin)) {
    return UserSiteSetting::kGrantAllExtensions;
  }
  if (base::Contains(user_permissions_.restricted_sites, origin)) {
    return UserSiteSetting::kBlockAllExtensions;
  }
  return UserSiteSetting::kCustomizeByExtension;
}

PermissionsManager::UserSiteAccess PermissionsManager::GetUserSiteAccess(
    const Extension& extension,
    const GURL& gurl) const {
  DCHECK(
      !extension.permissions_data()->IsRestrictedUrl(gurl, /*error=*/nullptr));

  ExtensionSiteAccess site_access = GetSiteAccess(extension, gurl);
  if (site_access.has_all_sites_access) {
    return UserSiteAccess::kOnAllSites;
  }
  if (site_access.has_site_access) {
    return UserSiteAccess::kOnSite;
  }
  return UserSiteAccess::kOnClick;
}

PermissionsManager::ExtensionSiteAccess PermissionsManager::GetSiteAccess(
    const Extension& extension,
    const GURL& url) const {
  PermissionsManager::ExtensionSiteAccess extension_access;

  // Extension that doesn't request host permission has no access.
  if (!HasRequestedHostPermissions(extension) &&
      !HasRequestedActiveTab(extension)) {
    return extension_access;
  }

  // Awkward holder object because permission sets are immutable, and when
  // return from prefs, ownership is passed.
  std::unique_ptr<const PermissionSet> permission_holder;

  const PermissionSet* granted_permissions = nullptr;
  if (!HasWithheldHostPermissions(extension)) {
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

bool PermissionsManager::CanAffectExtension(const Extension& extension) const {
  // Certain extensions are always exempt from having permissions withheld.
  if (!util::CanWithholdPermissionsFromExtension(extension))
    return false;

  // The extension can be affected by runtime host permissions if extension can
  // have site access to it.
  return HasRequestedHostPermissions(extension) ||
         HasRequestedActiveTab(extension);
}

bool PermissionsManager::CanUserSelectSiteAccess(
    const Extension& extension,
    const GURL& url,
    UserSiteAccess site_access) const {
  // Extensions cannot run on sites restricted to them (ever), so no type of
  // site access is selectable.
  if (extension.permissions_data()->IsRestrictedUrl(url, /*error=*/nullptr)) {
    return false;
  }

  // The "on click" option is enabled if the extension has active tab,
  // regardless of its granted host permissions.
  if (site_access == PermissionsManager::UserSiteAccess::kOnClick &&
      HasActiveTabAndCanAccess(extension, url)) {
    return true;
  }

  if (!CanAffectExtension(extension)) {
    return false;
  }

  PermissionsManager::ExtensionSiteAccess extension_access =
      GetSiteAccess(extension, url);
  switch (site_access) {
    case UserSiteAccess::kOnClick:
      // The "on click" option is only enabled if the extension has active tab,
      // previously handled, or wants to always run on the site without user
      // interaction.
      return extension_access.has_site_access ||
             extension_access.withheld_site_access;
    case UserSiteAccess::kOnSite:
      // The "on site" option is only enabled if the extension wants to
      // always run on the site without user interaction.
      return extension_access.has_site_access ||
             extension_access.withheld_site_access;
    case UserSiteAccess::kOnAllSites:
      // The "on all sites" option is only enabled if the extension wants to be
      // able to run everywhere.
      return extension_access.has_all_sites_access ||
             extension_access.withheld_all_sites_access;
  }
}

bool PermissionsManager::HasRequestedHostPermissions(
    const Extension& extension) const {
  return !PermissionsParser::GetRequiredPermissions(&extension)
              .effective_hosts()
              .is_empty() ||
         !PermissionsParser::GetOptionalPermissions(&extension)
              .effective_hosts()
              .is_empty();
}

bool PermissionsManager::HasGrantedHostPermission(const Extension& extension,
                                                  const GURL& url) const {
  DCHECK(CanAffectExtension(extension));

  return GetRuntimePermissionsFromPrefs(extension)
      ->effective_hosts()
      .MatchesSecurityOrigin(url);
}

bool PermissionsManager::HasBroadGrantedHostPermissions(
    const Extension& extension) {
  // Don't consider API permissions in this case.
  constexpr bool kIncludeApiPermissions = false;
  return GetRuntimePermissionsFromPrefs(extension)->ShouldWarnAllHosts(
      kIncludeApiPermissions);
}

bool PermissionsManager::HasWithheldHostPermissions(
    const Extension& extension) const {
  return extension_prefs_->GetWithholdingPermissions(extension.id());
}

bool PermissionsManager::HasRequestedActiveTab(
    const Extension& extension) const {
  return PermissionsParser::GetRequiredPermissions(&extension)
             .HasAPIPermission(mojom::APIPermissionID::kActiveTab) ||
         PermissionsParser::GetOptionalPermissions(&extension)
             .HasAPIPermission(mojom::APIPermissionID::kActiveTab);
}

bool PermissionsManager::HasActiveTabAndCanAccess(const Extension& extension,
                                                  const GURL& url) const {
  if (!extension.permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kActiveTab)) {
    return false;
  }

  if (extension.permissions_data()->IsRestrictedUrl(url,
                                                    /*error=*/nullptr)) {
    return false;
  }

  if (extension.permissions_data()->IsPolicyBlockedHost(url)) {
    return false;
  }

  if (url.SchemeIsFile() &&
      !util::AllowFileAccess(extension.id(), browser_context_)) {
    return false;
  }

  return true;
}

std::unique_ptr<PermissionSet>
PermissionsManager::GetRuntimePermissionsFromPrefs(
    const Extension& extension) const {
  std::unique_ptr<PermissionSet> permissions =
      extension_prefs_->GetRuntimeGrantedPermissions(extension.id());
  return AdjustHostPatterns(std::move(permissions), extension.id());
}

std::unique_ptr<PermissionSet>
PermissionsManager::GetDesiredActivePermissionsFromPrefs(
    const Extension& extension) const {
  std::unique_ptr<PermissionSet> permissions =
      extension_prefs_->GetDesiredActivePermissions(extension.id());
  return AdjustHostPatterns(std::move(permissions), extension.id());
}

std::unique_ptr<PermissionSet>
PermissionsManager::GetBoundedExtensionDesiredPermissions(
    const Extension& extension) const {
  // Determine the extension's "required" permissions (though even these can
  // be withheld).
  const PermissionSet& required_permissions =
      PermissionsParser::GetRequiredPermissions(&extension);

  // Retrieve the desired permissions from prefs. "Desired permissions" here
  // are the permissions the extension most recently set for itself.  This
  // might not be all granted permissions, since extensions can revoke their
  // own permissions via chrome.permissions.remove() (which removes the
  // permission from the active set, but not the granted set).
  std::unique_ptr<PermissionSet> desired_active_permissions =
      extension_prefs_->GetDesiredActivePermissions(extension.id());
  // The stored desired permissions may be null if the extension has never
  // used the permissions API to modify its active permissions. In this case,
  // the desired permissions are simply the set of required permissions.
  if (!desired_active_permissions)
    return required_permissions.Clone();

  // Otherwise, the extension has stored a set of desired permissions. This
  // could actually be a superset *or* a subset of requested permissions by the
  // extension (depending on how its permissions have changed).
  // Start by calculating the set of all current potentially-desired
  // permissions by combining the required and optional permissions.
  std::unique_ptr<PermissionSet> requested_permissions =
      PermissionSet::CreateUnion(
          required_permissions,
          PermissionsParser::GetOptionalPermissions(&extension));

  // Now, take the intersection of the requested permissions and the stored
  // permissions. This filters out any previously-stored permissions that are
  // no longer used (which we continue to store in prefs in case the extension
  // wants them back in the future).
  std::unique_ptr<PermissionSet> bounded_desired =
      PermissionSet::CreateIntersection(*desired_active_permissions,
                                        *requested_permissions);

  // Additionally, we ensure that all "required" permissions are included in
  // this desired set (to guard against any pref corruption - this ensures at
  // least everything is in a "sane" state).
  // TODO(crbug.com/40850847): Maddeningly, the order of the arguments
  // passed to CreateUnion() here is *important*. Passing `bounded_desired` as
  // the first param results in the valid schemes being removed.
  bounded_desired =
      PermissionSet::CreateUnion(required_permissions, *bounded_desired);

  return bounded_desired;
}

std::unique_ptr<PermissionSet>
PermissionsManager::GetEffectivePermissionsToGrant(
    const Extension& extension,
    const PermissionSet& desired_permissions) const {
  if (!util::CanWithholdPermissionsFromExtension(extension)) {
    // The withhold creation flag should never have been set in cases where
    // withholding isn't allowed.
    DCHECK(!(extension.creation_flags() & Extension::WITHHOLD_PERMISSIONS));
    return desired_permissions.Clone();
  }

  if (desired_permissions.effective_hosts().is_empty()) {
    return desired_permissions.Clone();  // No hosts to withhold.
  }

  // Determine if we should withhold host permissions. This is different for
  // extensions that are being newly-installed and extensions that have already
  // been installed; this is indicated by the extension creation flags.
  bool should_withhold = false;
  if (extension.creation_flags() & Extension::WITHHOLD_PERMISSIONS)
    should_withhold = true;
  else
    should_withhold = HasWithheldHostPermissions(extension);

  if (!should_withhold)
    return desired_permissions.Clone();

  // Otherwise, permissions should be withheld according to the user-granted
  // permission set.

  // Determine the permissions granted by the user at runtime. If none are found
  // in prefs, default it to an empty set.
  std::unique_ptr<PermissionSet> runtime_granted_permissions =
      GetRuntimePermissionsFromPrefs(extension);
  if (!runtime_granted_permissions)
    runtime_granted_permissions = std::make_unique<PermissionSet>();

  PermissionSet user_granted_permissions;
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    // Also add any hosts the user indicated extensions may always run on.
    URLPatternSet user_allowed_sites;
    for (const auto& site : user_permissions_.permitted_sites) {
      user_allowed_sites.AddOrigin(Extension::kValidHostPermissionSchemes,
                                   site);
    }

    user_granted_permissions =
        PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                      user_allowed_sites.Clone(), user_allowed_sites.Clone());
  }

  return GetAllowedPermissionsAfterWithholding(desired_permissions,
                                               *runtime_granted_permissions,
                                               user_granted_permissions);
}

std::unique_ptr<const PermissionSet>
PermissionsManager::GetRevokablePermissions(const Extension& extension) const {
  // No extra revokable permissions if the extension couldn't ever be affected.
  if (!util::CanWithholdPermissionsFromExtension(extension))
    return nullptr;

  // If we aren't withholding host permissions, then there may be some
  // permissions active on the extension that should be revokable. Otherwise,
  // all granted permissions should be stored in the preferences (and these
  // can be a superset of permissions on the extension, as in the case of e.g.
  // granting origins when only a subset is requested by the extension).
  // TODO(devlin): This is confusing and subtle. We should instead perhaps just
  // add all requested hosts as runtime-granted hosts if we aren't withholding
  // host permissions.
  const PermissionSet* current_granted_permissions = nullptr;
  std::unique_ptr<const PermissionSet> runtime_granted_permissions =
      GetRuntimePermissionsFromPrefs(extension);
  std::unique_ptr<const PermissionSet> union_set;
  if (runtime_granted_permissions) {
    union_set = PermissionSet::CreateUnion(
        *runtime_granted_permissions,
        extension.permissions_data()->active_permissions());
    current_granted_permissions = union_set.get();
  } else {
    current_granted_permissions =
        &extension.permissions_data()->active_permissions();
  }

  // Unrevokable permissions include granted API permissions, manifest
  // permissions, and host permissions that are always allowed.
  PermissionSet unrevokable_permissions(
      current_granted_permissions->apis().Clone(),
      current_granted_permissions->manifest_permissions().Clone(),
      URLPatternSet(), URLPatternSet());
  {
    // TODO(devlin): We do this pattern of "required + optional" enough. Make it
    // a part of PermissionsParser and stop duplicating the set each time.
    std::unique_ptr<PermissionSet> requested_permissions =
        PermissionSet::CreateUnion(
            PermissionsParser::GetRequiredPermissions(&extension),
            PermissionsParser::GetOptionalPermissions(&extension));
    ExtensionsBrowserClient::Get()->AddAdditionalAllowedHosts(
        *requested_permissions, &unrevokable_permissions);
  }

  // Revokable permissions are, predictably, any in the current set that aren't
  // considered unrevokable.
  return PermissionSet::CreateDifference(*current_granted_permissions,
                                         unrevokable_permissions);
}

std::unique_ptr<const PermissionSet>
PermissionsManager::GetExtensionGrantedPermissions(
    const Extension& extension) const {
  // Some extensions such as policy installed extensions, have active
  // permissions that are always granted and do not store their permissions in
  // `GetGrantedPermissions()`. Instead, retrieve their permissions through
  // their permissions data directly.
  if (!CanAffectExtension(extension)) {
    return extension.permissions_data()->active_permissions().Clone();
  }

  return HasWithheldHostPermissions(extension)
             ? extension_prefs_->GetRuntimeGrantedPermissions(extension.id())
             : extension_prefs_->GetGrantedPermissions(extension.id());
}

void PermissionsManager::AddSiteAccessRequest(
    content::WebContents* web_contents,
    int tab_id,
    const Extension& extension,
    const std::optional<URLPattern>& filter) {
  // Extension must not have granted access to the current site.
  const GURL& url = web_contents->GetLastCommittedURL();
  ExtensionSiteAccess site_access = GetSiteAccess(extension, url);
  CHECK(!site_access.has_site_access);

  // Request will never be active if the extension cannot be granted access to
  // the current site. This includes sites that are restricted to the extension,
  // and sites that were never requested by the extension. Thus, we don't need
  // to add the request.
  std::string error;
  if (extension.permissions_data()->IsPolicyBlockedHost(url) ||
      extension.permissions_data()->IsRestrictedUrl(url, &error)) {
    return;
  }
  if (!site_access.withheld_site_access &&
      !PermissionsParser::GetOptionalPermissions(&extension)
           .HasEffectiveAccessToURL(web_contents->GetLastCommittedURL())) {
    return;
  }

  SiteAccessRequestsHelper* helper =
      GetOrCreateSiteAccessRequestsHelperFor(web_contents, tab_id);

  // Request will never be active if `filter` doesn't match the current origin,
  // since requests are cleared on cross-origin navigations. Thus, we don't need
  // to add the request.
  if (filter.has_value() && !filter.value().MatchesSecurityOrigin(
                                web_contents->GetLastCommittedURL())) {
    // Remove the existent request, if any, since the new request overrides it.
    if (helper->RemoveRequest(extension.id(), /*filter=*/std::nullopt)) {
      for (auto& observer : observers_) {
        observer.OnSiteAccessRequestRemoved(extension.id(), tab_id);
      }
    }
    return;
  }

  if (helper->HasRequest(extension.id())) {
    helper->UpdateRequest(extension, filter);
    for (auto& observer : observers_) {
      observer.OnSiteAccessRequestUpdated(extension.id(), tab_id);
    }
  } else {
    helper->AddRequest(extension, filter);
    for (auto& observer : observers_) {
      observer.OnSiteAccessRequestAdded(extension.id(), tab_id);
    }
  }
}

bool PermissionsManager::RemoveSiteAccessRequest(
    int tab_id,
    const ExtensionId& extension_id,
    const std::optional<URLPattern>& filter) {
  SiteAccessRequestsHelper* helper = GetSiteAccessRequestsHelperFor(tab_id);
  if (!helper) {
    return false;
  }

  bool request_removed = helper->RemoveRequest(extension_id, filter);
  if (!request_removed) {
    return false;
  }

  if (!helper->HasRequests()) {
    DeleteSiteAccessRequestHelperFor(tab_id);
  }

  for (auto& observer : observers_) {
    observer.OnSiteAccessRequestRemoved(extension_id, tab_id);
  }
  return true;
}

void PermissionsManager::UserDismissedSiteAccessRequest(
    content::WebContents* web_contents,
    int tab_id,
    const ExtensionId& extension_id) {
  SiteAccessRequestsHelper* helper = GetSiteAccessRequestsHelperFor(tab_id);
  CHECK(helper);
  helper->UserDismissedRequest(extension_id);

  for (Observer& observer : observers_) {
    observer.OnSiteAccessRequestDismissedByUser(
        extension_id,
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  }
}

bool PermissionsManager::HasActiveSiteAccessRequest(
    int tab_id,
    const ExtensionId& extension_id) {
  SiteAccessRequestsHelper* helper = GetSiteAccessRequestsHelperFor(tab_id);
  return helper && helper->HasActiveRequest(extension_id);
}

void PermissionsManager::AddExtensionToPreviousBroadSiteAccessSet(
    const ExtensionId& extension_id) {
  extensions_with_previous_broad_access_.insert(extension_id);
}

void PermissionsManager::RemoveExtensionFromPreviousBroadSiteAccessSet(
    const ExtensionId& extension_id) {
  extensions_with_previous_broad_access_.erase(extension_id);
}

bool PermissionsManager::HasPreviousBroadSiteAccess(
    const ExtensionId& extension_id) {
  return extensions_with_previous_broad_access_.contains(extension_id);
}

void PermissionsManager::NotifyExtensionPermissionsUpdated(
    const Extension& extension,
    const PermissionSet& permissions,
    UpdateReason reason) {
  std::vector<int> tabs_to_remove;
  for (auto& [tab_id, helper] : requests_helpers_) {
    bool request_removed = helper->RemoveRequestIfGrantedAccess(extension);
    if (!request_removed) {
      continue;
    }

    for (auto& observer : observers_) {
      observer.OnSiteAccessRequestRemoved(extension.id(), tab_id);
    }

    if (!helper->HasRequests()) {
      tabs_to_remove.push_back(tab_id);
    }
  }

  for (auto tab_id : tabs_to_remove) {
    DeleteSiteAccessRequestHelperFor(tab_id);
  }

  for (Observer& observer : observers_) {
    observer.OnExtensionPermissionsUpdated(extension, permissions, reason);
  }
}

void PermissionsManager::NotifyActiveTabPermisssionGranted(
    content::WebContents* web_contents,
    int tab_id,
    const Extension& extension) {
  RemoveSiteAccessRequest(tab_id, extension.id());

  for (Observer& observer : observers_) {
    observer.OnActiveTabPermissionGranted(extension);
  }
}

void PermissionsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PermissionsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PermissionsManager::OnUserPermissionsSettingsChanged() {
  // TODO(http://crbug.com/1268198): AddOrigin() below can fail if the
  // added URLPattern doesn't parse (such as if the schemes are invalid). We
  // need to make sure that origins added to this list only contain schemes that
  // are valid for extensions to act upon (and gracefully handle others).
  URLPatternSet user_blocked_sites;
  for (const auto& site : user_permissions_.restricted_sites)
    user_blocked_sites.AddOrigin(Extension::kValidHostPermissionSchemes, site);
  URLPatternSet user_allowed_sites;
  for (const auto& site : user_permissions_.permitted_sites)
    user_allowed_sites.AddOrigin(Extension::kValidHostPermissionSchemes, site);

  PermissionSet user_allowed_set(APIPermissionSet(), ManifestPermissionSet(),
                                 user_allowed_sites.Clone(),
                                 user_allowed_sites.Clone());

  // Update all installed extensions with the new user permissions. We do this
  // for all installed extensions (and not just enabled extensions) so that
  // entries in the chrome://extensions page for disabled extensions are
  // accurate.
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  auto all_extensions = registry->GenerateInstalledExtensionsSet();
  for (const auto& extension : all_extensions) {
    UpdatePermissionsWithUserSettings(*extension, user_allowed_set);
  }

  // Send the new permissions states to the renderers, including both the
  // updated user host settings and the updated permissions for each extension.
  // Unlike above, we only care about enabled extensions here, since disabled
  // extensions aren't running.
  {
    ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
    for (content::RenderProcessHost::iterator host_iterator(
             content::RenderProcessHost::AllHostsIterator());
         !host_iterator.IsAtEnd(); host_iterator.Advance()) {
      content::RenderProcessHost* host = host_iterator.GetCurrentValue();
      if (host->IsInitializedAndNotDead() &&
          browser_client->IsSameContext(browser_context_,
                                        host->GetBrowserContext())) {
        mojom::Renderer* renderer =
            RendererStartupHelperFactory::GetForBrowserContext(
                host->GetBrowserContext())
                ->GetRenderer(host);
        if (renderer) {
          renderer->UpdateUserHostRestrictions(user_blocked_sites.Clone(),
                                               user_allowed_sites.Clone());
          for (const auto& extension : registry->enabled_extensions()) {
            const PermissionsData* permissions_data =
                extension->permissions_data();
            renderer->UpdatePermissions(
                extension->id(),
                std::move(*permissions_data->active_permissions().Clone()),
                std::move(*permissions_data->withheld_permissions().Clone()),
                permissions_data->policy_blocked_hosts(),
                permissions_data->policy_allowed_hosts(),
                permissions_data->UsesDefaultPolicyHostRestrictions());
          }
        }
      }
    }
  }

  PermissionsData::SetUserHostRestrictions(
      util::GetBrowserContextId(browser_context_),
      std::move(user_blocked_sites), std::move(user_allowed_sites));

  // Notify observers of a permissions change once the changes have taken
  // effect in the network layer.
  NetworkPermissionsUpdater::UpdateAllExtensions(
      *browser_context_,
      base::BindOnce(&PermissionsManager::NotifyUserPermissionSettingsChanged,
                     weak_factory_.GetWeakPtr()));
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

SiteAccessRequestsHelper* PermissionsManager::GetSiteAccessRequestsHelperFor(
    int tab_id) {
  auto it = requests_helpers_.find(tab_id);
  return it == requests_helpers_.end() ? nullptr : it->second.get();
}

SiteAccessRequestsHelper*
PermissionsManager::GetOrCreateSiteAccessRequestsHelperFor(
    content::WebContents* web_contents,
    int tab_id) {
  auto* helper = GetSiteAccessRequestsHelperFor(tab_id);

  if (!helper) {
    auto helper_unique = std::make_unique<SiteAccessRequestsHelper>(
        PassKey(), this, web_contents, tab_id);
    helper = helper_unique.get();
    requests_helpers_.emplace(tab_id, std::move(helper_unique));
  }

  return helper;
}

void PermissionsManager::DeleteSiteAccessRequestHelperFor(int tab_id) {
  requests_helpers_.erase(tab_id);
}

void PermissionsManager::NotifyUserPermissionSettingsChanged() {
  for (auto& observer : observers_) {
    observer.OnUserPermissionsSettingsChanged(GetUserPermissionsSettings());
  }
}

void PermissionsManager::NotifySiteAccessRequestsCleared(int tab_id) {
  for (auto& observer : observers_) {
    observer.OnSiteAccessRequestsCleared(tab_id);
  }
}

void PermissionsManager::NotifyShowAccessRequestsInToolbarChanged(
    const extensions::ExtensionId& extension_id,
    bool can_show_requests) {
  for (auto& observer : observers_) {
    observer.OnShowAccessRequestsInToolbarChanged(extension_id,
                                                  can_show_requests);
  }
}

}  // namespace extensions
