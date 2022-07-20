// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/network_permissions_updater.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/pref_types.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
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

UpdatedExtensionPermissionsInfo::UpdatedExtensionPermissionsInfo(
    const Extension* extension,
    const PermissionSet& permissions,
    Reason reason)
    : reason(reason), extension(extension), permissions(permissions) {}

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
  OnUserPermissionsSettingsChanged();
}

void PermissionsManager::RemoveUserRestrictedSite(const url::Origin& origin) {
  if (RemoveRestrictedSiteAndUpdatePrefs(origin))
    OnUserPermissionsSettingsChanged();
}

void PermissionsManager::AddUserPermittedSite(const url::Origin& origin) {
  if (base::Contains(user_permissions_.permitted_sites, origin))
    return;

  // Origin cannot be both restricted and permitted.
  RemoveRestrictedSiteAndUpdatePrefs(origin);

  user_permissions_.permitted_sites.insert(origin);
  AddSiteToPrefs(extension_prefs_, kPermittedSites, origin);

  OnUserPermissionsSettingsChanged();
}

void PermissionsManager::UpdatePermissionsWithUserSettings(
    const Extension& extension,
    const PermissionSet& user_permitted_set) {
  // If either user cannot withhold permissions from the extension (as is the
  // case for e.g. policy-installed extensions) or the user has not withheld
  // any permissions for the extension, then we don't need to do anything - the
  // extension already has all its requested permissions.
  if (!util::CanWithholdPermissionsFromExtension(extension) ||
      !HasWithheldHostPermissions(extension.id())) {
    return;
  }

  // TODO(https://crbug.com/1268198): Much of this logic is similar to the
  // logic from PermissionsUpdater::InitializePermissions(). We should have the
  // PermissionsUpdater use this (or replace it with this, though there's
  // some complications there).

  // Otherwise, calculate out the new set of permissions for the extension.
  // This calculation is:
  // 1) Take the set of all allowed permissions. This is the union of
  //    runtime-granted permissions (where the user said "this extension may run
  //    on this site") and `user_permitted_set` (sites the user allows any
  //    extension to run on).
  std::unique_ptr<const PermissionSet> allowed_permissions =
      PermissionSet::CreateUnion(user_permitted_set,
                                 *GetRuntimePermissionsFromPrefs(extension));

  // 2) Calculate the set of extension-desired permissions, which is the set of
  //    permissions the extension most recently set for itself (this may be
  //    different than granted, as extensions can remove permissions from
  //    themselves via chrome.permissions.remove() (which removes the
  //    permission from the active set, but not the granted set).
  std::unique_ptr<const PermissionSet> bounded_desired =
      GetBoundedExtensionDesiredPermissions(extension);

  // 3) Add in any always-approved hosts that shouldn't be removed (such as
  //    chrome://favicon).
  allowed_permissions =
      ExtensionsBrowserClient::Get()->AddAdditionalAllowedHosts(
          *bounded_desired, *allowed_permissions);

  // 4) Finalize the allowed set. Since we don't allow withholding of API and
  //    manifest permissions, the allowed set always contains all (bounded)
  //    requested API and manifest permissions.
  allowed_permissions = std::make_unique<const PermissionSet>(
      bounded_desired->apis().Clone(),
      bounded_desired->manifest_permissions().Clone(),
      allowed_permissions->explicit_hosts().Clone(),
      allowed_permissions->scriptable_hosts().Clone());

  // 5) Calculate the new active and withheld permissions. The active
  //    permissions are the intersection of all permissions the extension is
  //    allowed to have with all permissions the extension elected to have.
  //    Said differently, we grant a permission if both the extension and the
  //    user approved it.
  //    Withheld permissions are any required permissions that are not in the
  //    new active set.
  std::unique_ptr<const PermissionSet> new_active =
      PermissionSet::CreateIntersection(
          *allowed_permissions, *bounded_desired,
          URLPatternSet::IntersectionBehavior::kDetailed);
  std::unique_ptr<const PermissionSet> new_withheld =
      PermissionSet::CreateDifference(
          PermissionsParser::GetRequiredPermissions(&extension), *new_active);

  // Set the new permissions on the extension.
  extension.permissions_data()->SetPermissions(std::move(new_active),
                                               std::move(new_withheld));
}

void PermissionsManager::RemoveUserPermittedSite(const url::Origin& origin) {
  if (RemovePermittedSiteAndUpdatePrefs(origin))
    OnUserPermissionsSettingsChanged();
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

std::unique_ptr<const PermissionSet>
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
  std::unique_ptr<const PermissionSet> desired_active_permissions =
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
  std::unique_ptr<const PermissionSet> requested_permissions =
      PermissionSet::CreateUnion(
          required_permissions,
          PermissionsParser::GetOptionalPermissions(&extension));

  // Now, take the intersection of the requested permissions and the stored
  // permissions. This filters out any previously-stored permissions that are
  // no longer used (which we continue to store in prefs in case the extension
  // wants them back in the future).
  std::unique_ptr<const PermissionSet> bounded_desired =
      PermissionSet::CreateIntersection(*desired_active_permissions,
                                        *requested_permissions);

  // Additionally, we ensure that all "required" permissions are included in
  // this desired set (to guard against any pref corruption - this ensures at
  // least everything is in a "sane" state).
  // TODO(https://crbug.com/1341118): Maddeningly, the order of the arguments
  // passed to CreateUnion() here is *important*. Passing `bounded_desired` as
  // the first param results in the valid schemes being removed.
  bounded_desired =
      PermissionSet::CreateUnion(required_permissions, *bounded_desired);

  return bounded_desired;
}

std::unique_ptr<const PermissionSet>
PermissionsManager::GetEffectivePermissionsToGrant(
    const Extension& extension,
    const PermissionSet& desired_permissions) const {
  if (!util::CanWithholdPermissionsFromExtension(extension)) {
    // The withhold creation flag should never have been set in cases where
    // withholding isn't allowed.
    DCHECK(!(extension.creation_flags() & Extension::WITHHOLD_PERMISSIONS));
    return desired_permissions.Clone();
  }

  if (desired_permissions.effective_hosts().is_empty())
    return desired_permissions.Clone();  // No hosts to withhold.

  // Determine if we should withhold host permissions. This is different for
  // extensions that are being newly-installed and extensions that have already
  // been installed; this is indicated by the extension creation flags.
  bool should_withhold = false;
  if (extension.creation_flags() & Extension::WITHHOLD_PERMISSIONS)
    should_withhold = true;
  else
    should_withhold = HasWithheldHostPermissions(extension.id());

  if (!should_withhold)
    return desired_permissions.Clone();

  // Otherwise, permissions should be withheld according to the user-granted
  // permission set.

  // Determine the permissions granted by the user at runtime. If none are found
  // in prefs, default it to an empty set.
  std::unique_ptr<const PermissionSet> granted_permissions =
      GetRuntimePermissionsFromPrefs(extension);
  if (!granted_permissions)
    granted_permissions = std::make_unique<PermissionSet>();

  // Add any additional hosts that should be auto-granted.
  granted_permissions =
      ExtensionsBrowserClient::Get()->AddAdditionalAllowedHosts(
          desired_permissions, *granted_permissions);

  URLPatternSet granted_scriptable_hosts =
      granted_permissions->scriptable_hosts().Clone();
  URLPatternSet granted_explicit_hosts =
      granted_permissions->explicit_hosts().Clone();

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    // Also add any hosts the user indicated extensions may always run on.
    URLPatternSet user_allowed_sites;
    for (const auto& site : user_permissions_.permitted_sites) {
      user_allowed_sites.AddOrigin(Extension::kValidHostPermissionSchemes,
                                   site);
    }

    granted_scriptable_hosts.AddPatterns(user_allowed_sites);
    granted_explicit_hosts.AddPatterns(user_allowed_sites);
  }

  // Host permissions may be withheld. The resulting set is the intersection of
  // the hosts the extension desires and the user has approved or should always
  // be granted.
  URLPatternSet new_scriptable_hosts = URLPatternSet::CreateIntersection(
      desired_permissions.scriptable_hosts(), granted_scriptable_hosts,
      URLPatternSet::IntersectionBehavior::kDetailed);
  URLPatternSet new_explicit_hosts = URLPatternSet::CreateIntersection(
      desired_permissions.explicit_hosts(), granted_explicit_hosts,
      URLPatternSet::IntersectionBehavior::kDetailed);

  // The total resulting permissions set includes the new host permissions and
  // the originally-requested API and manifest permissions (which are never
  // currently withheld).
  return std::make_unique<PermissionSet>(
      desired_permissions.apis().Clone(),
      desired_permissions.manifest_permissions().Clone(),
      std::move(new_explicit_hosts), std::move(new_scriptable_hosts));
}

void PermissionsManager::NotifyExtensionPermissionsUpdated(
    const UpdatedExtensionPermissionsInfo& info) {
  for (Observer& observer : observers_) {
    observer.OnExtensionPermissionsUpdated(info);
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
  for (const auto& extension : *all_extensions) {
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
      base::BindOnce(&PermissionsManager::NotifyObserversOfChange,
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

void PermissionsManager::NotifyObserversOfChange() {
  for (auto& observer : observers_)
    observer.OnUserPermissionsSettingsChanged(GetUserPermissionsSettings());
}

}  // namespace extensions
