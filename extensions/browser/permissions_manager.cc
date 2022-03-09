// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
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
