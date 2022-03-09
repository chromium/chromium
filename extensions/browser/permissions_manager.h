// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PERMISSIONS_MANAGER_H_
#define EXTENSIONS_BROWSER_PERMISSIONS_MANAGER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

class ExtensionPrefs;

// Class for managing user-scoped extension permissions.
// Includes blocking all extensions from running on a site and automatically
// running all extensions on a site.
class PermissionsManager : public KeyedService {
 public:
  // A struct storing the user-specified settings that apply to all extensions,
  // past, present, or future.
  // We use url::Origin here (rather than URLPatternSet) because permission
  // grants (and restrictions) are only meaningful at an origin level. It's not
  // possible to, say, block an extension from running on google.com/maps while
  // still allowing it to run on google.com/search.
  // Note: Policy extensions and component extensions can bypass these
  // settings.
  struct UserPermissionsSettings {
    UserPermissionsSettings();
    ~UserPermissionsSettings();
    UserPermissionsSettings(const UserPermissionsSettings& other) = delete;
    UserPermissionsSettings& operator=(UserPermissionsSettings& other) = delete;

    // Sites the user has blocked all extensions from running on.
    std::set<url::Origin> restricted_sites;

    // Sites the user has allowed all extensions to run on.
    std::set<url::Origin> permitted_sites;
  };

  // The user's site setting for a given site.
  enum class UserSiteSetting {
    // All extensions that request access are granted access in the site.
    kGrantAllExtensions,
    // All extensions that request access have withheld access in the site.
    kBlockAllExtensions,
    // Each extension that requests access can have its site access customized
    // in the site.
    kCustomizeByExtension,
  };

  class Observer {
   public:
    virtual void UserPermissionsSettingsChanged(
        const UserPermissionsSettings& settings) {}
  };

  explicit PermissionsManager(content::BrowserContext* browser_context);
  ~PermissionsManager() override;
  PermissionsManager(const PermissionsManager&) = delete;
  const PermissionsManager& operator=(const PermissionsManager&) = delete;

  // Retrieves the PermissionsManager for a given `browser_context`.
  static PermissionsManager* Get(content::BrowserContext* browser_context);

  // Retrieves the factory instance for the PermissionsManager.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Registers the user preference that stores user permissions.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Adds `origin` to the list of sites the user has blocked all
  // extensions from running on. If `origin` is in permitted_sites, it will
  // remove it from such list.
  void AddUserRestrictedSite(const url::Origin& origin);

  // Removes `origin` from the list of sites the user has blocked all
  // extensions from running on and notifies observers.
  void RemoveUserRestrictedSite(const url::Origin& origin);

  // Adds `origin` to the list of sites the user has allowed all
  // extensions to run on. If `origin` is in restricted_sites, it will remove it
  // from such list.
  void AddUserPermittedSite(const url::Origin& origin);

  // Removes `origin` from the list of sites the user has allowed all
  // extensions to run on and notifies observers.
  void RemoveUserPermittedSite(const url::Origin& origin);

  // Returns the user's permission settings.
  const UserPermissionsSettings& GetUserPermissionsSettings() const;

  // Returns the user's site setting for `origin`.
  UserSiteSetting GetUserSiteSetting(const url::Origin& origin) const;

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Called whenever `user_permissions_` have changed.
  void SignalUserPermissionsSettingsChanged() const;

  // Removes `origin` from the list of sites the user has allowed all
  // extensions to run on and saves the change to `extension_prefs_`. Returns if
  // the site has been removed.
  bool RemovePermittedSiteAndUpdatePrefs(const url::Origin& origin);

  // Removes `origin` from the list of sites the user has blocked all
  // extensions from running on and saves the change to `extension_prefs_`.
  // Returns if the site has been removed.
  bool RemoveRestrictedSiteAndUpdatePrefs(const url::Origin& origin);

  base::ObserverList<Observer>::Unchecked observers_;
  ExtensionPrefs* const extension_prefs_;
  UserPermissionsSettings user_permissions_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PERMISSIONS_MANAGER_H_
