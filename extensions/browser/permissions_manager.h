// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PERMISSIONS_MANAGER_H_
#define EXTENSIONS_BROWSER_PERMISSIONS_MANAGER_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/site_access_requests_helper.h"
#include "extensions/common/extension_id.h"
#include "url/origin.h"

class ExtensionsMenuViewController;
class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

class ExtensionPrefs;
class Extension;
class PermissionSet;

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

  // The extension's requested site access for an extension.
  struct ExtensionSiteAccess {
    // The extension has access to the current domain.
    bool has_site_access = false;
    // The extension requested access to the current domain, but it was
    // withheld.
    bool withheld_site_access = false;
    // The extension has access to all sites (or a pattern sufficiently broad
    // as to be functionally similar, such as https://*.com/*). Note that since
    // this includes "broad" patterns, this may be true even if
    // `has_site_access` is false.
    bool has_all_sites_access = false;
    // The extension wants access to all sites (or a pattern sufficiently broad
    // as to be functionally similar, such as https://*.com/*). Note that since
    // this includes "broad" patterns, this may be true even if
    // `withheld_site_access` is false.
    bool withheld_all_sites_access = false;
  };

  // The user's selected site access for an extension. Users will not be able to
  // change this for enterprise installed extensions.
  enum class UserSiteAccess {
    kOnClick,
    kOnSite,
    kOnAllSites,
  };

  // The user's selected site setting for a given site.
  enum class UserSiteSetting {
    // All extensions that request access are granted access in the site.
    kGrantAllExtensions,
    // All extensions that request access have withheld access in the site.
    kBlockAllExtensions,
    // Each extension that requests access can have its site access customized
    // in the site.
    kCustomizeByExtension,
  };

  enum class UpdateReason {
    // Permissions were added to the extension.
    kAdded,
    // Permissions were removed from the extension.
    kRemoved,
    // Policy that affects permissions was updated.
    kPolicy,
  };

  class Observer {
   public:
    // Called when `user_permissions_` have been updated for an extension.
    virtual void OnUserPermissionsSettingsChanged(
        const UserPermissionsSettings& settings) {}

    // Called when permissions have been updated for an extension.
    virtual void OnExtensionPermissionsUpdated(const Extension& extension,
                                               const PermissionSet& permissions,
                                               UpdateReason reason) {}

    // Called when `extension` was granted active tab permission.
    virtual void OnActiveTabPermissionGranted(const Extension& extension) {}

    // Called when an extension's ability to show site access requests in the
    // toolbar has been updated.
    virtual void OnShowAccessRequestsInToolbarChanged(
        const extensions::ExtensionId& extension_id,
        bool can_show_requests) {}

    // Called when `extension_id` added a site access request for `tab_id`.
    virtual void OnSiteAccessRequestAdded(const ExtensionId& extension_id,
                                          int tab_id) {}

    // Called when `extension_id` updated a site access request for `tab_id`.
    virtual void OnSiteAccessRequestUpdated(const ExtensionId& extension_id,
                                            int tab_id) {}

    // Called when `extension_id` removed a site access request for `tab_id`.
    virtual void OnSiteAccessRequestRemoved(const ExtensionId& extension_id,
                                            int tab_id) {}

    // Called when site access requests where cleared for `tab_id`.
    virtual void OnSiteAccessRequestsCleared(int tab_id) {}

    // Called when `extension_id` has dismissed site access requests in
    // `origin`.
    virtual void OnSiteAccessRequestDismissedByUser(
        const ExtensionId& extension_id,
        const url::Origin& origin) {}
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

  //  Updates the user site settings for the given `origin` to be
  //  `site_settings`.
  void UpdateUserSiteSetting(const url::Origin& origin,
                             UserSiteSetting site_setting);

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

  // Returns the user's selected site access for `extension` in `gurl`.
  // This can only be called if the url is not restricted, and if the user can
  // configure site access for the extension (which excludes things like policy
  // extensions) or if the extension has active tab permission.
  UserSiteAccess GetUserSiteAccess(const Extension& extension,
                                   const GURL& gurl) const;

  // Returns the current access level for the extension on the specified `url`.
  ExtensionSiteAccess GetSiteAccess(const Extension& extension,
                                    const GURL& url) const;

  // Returns true if the associated extension can be affected by
  // runtime host permissions.
  bool CanAffectExtension(const Extension& extension) const;

  // Returns whether the user can select the `site_access` option for
  // `extension` in `url`.
  bool CanUserSelectSiteAccess(const Extension& extension,
                               const GURL& gurl,
                               UserSiteAccess site_access) const;

  // Returns whether the `extension` has requested host permissions, either
  // required or optional.
  bool HasRequestedHostPermissions(const Extension& extension) const;

  // Returns true if the extension has been explicitly granted permission to run
  // on the origin of `url`. This will return true if any permission includes
  // access to the origin of |url|, even if the permission includes others
  // (such as *://*.com/*) or is restricted to a path (that is, an extension
  // with permission for https://google.com/maps will return true for
  // https://google.com). Note: This checks any runtime-granted permissions,
  // which includes both granted optional permissions and permissions granted
  // through the runtime host permissions feature.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  bool HasGrantedHostPermission(const Extension& extension,
                                const GURL& url) const;

  // Returns true if the `extension` has runtime granted permission patterns
  // that are sufficiently broad enough to be functionally similar to all sites
  // access.
  bool HasBroadGrantedHostPermissions(const Extension& extension);

  // Returns whether Chrome has withheld host permissions from the extension.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  bool HasWithheldHostPermissions(const Extension& extension) const;

  // Returns whether the `extension` has requested activeTab, either as a
  // required or optional permission.
  bool HasRequestedActiveTab(const Extension& extension) const;

  // Returns true if this extension uses the activeTab permission and would
  // probably be able to to access the given `url`. The actual checks when an
  // activeTab extension tries to run are a little more complicated and can be
  // seen in ExtensionActionRunner and ActiveTabPermissionGranter.
  // Note: The rare cases where this gets it wrong should only be for false
  // positives, where it reports that the extension wants access but it can't
  // actually be given access when it tries to run.
  bool HasActiveTabAndCanAccess(const Extension& extension,
                                const GURL& url) const;

  // Returns the effective list of runtime-granted/desired-active permissions
  // for a given `extension` from its prefs. ExtensionPrefs doesn't store the
  // valid schemes for URLPatterns, which results in the chrome:-scheme being
  // included for <all_urls> when retrieving it directly from the prefs; this
  // then causes CHECKs to fail when validating that permissions being revoked
  // are present (see https://crbug.com/930062). Returns null if there are no
  // stored runtime-granted/desired-active permissions.
  // TODO(crbug.com/41441259): ExtensionPrefs should return
  // properly-bounded permissions.
  std::unique_ptr<PermissionSet> GetRuntimePermissionsFromPrefs(
      const Extension& extension) const;
  std::unique_ptr<PermissionSet> GetDesiredActivePermissionsFromPrefs(
      const Extension& extension) const;

  // Returns the set of permissions that the `extension` wants to have active at
  // this time. This does *not* take into account user-granted or runtime-
  // withheld permissions.
  std::unique_ptr<PermissionSet> GetBoundedExtensionDesiredPermissions(
      const Extension& extension) const;

  // Returns the set of permissions that should be granted to the given
  // `extension` according to the runtime-granted permissions and current
  // preferences, omitting host permissions if the extension supports it and
  // the user has withheld permissions.
  std::unique_ptr<PermissionSet> GetEffectivePermissionsToGrant(
      const Extension& extension,
      const PermissionSet& desired_permissions) const;

  // Returns the subset of active permissions which can be withheld for a given
  // `extension`.
  std::unique_ptr<const PermissionSet> GetRevokablePermissions(
      const Extension& extension) const;

  // Returns the current set of granted permissions for the extension. Note that
  // permissions that are specified but withheld will not be returned.
  std::unique_ptr<const PermissionSet> GetExtensionGrantedPermissions(
      const Extension& extension) const;

  // Adds site access request with an optional `filter` for `extension` in
  // `web_contents` with `tab_id`. Extension must have site access withheld for
  // request to be added.
  void AddSiteAccessRequest(
      content::WebContents* web_contents,
      int tab_id,
      const Extension& extension,
      const std::optional<URLPattern>& filter = std::nullopt);

  // Removes site access request for `extension` in `tab_id` with an optional
  // `filter`, if existent. Returns whether the request was removed.
  bool RemoveSiteAccessRequest(
      int tab_id,
      const ExtensionId& extension_id,
      const std::optional<URLPattern>& filter = std::nullopt);

  // Dismisses site access request for `extension` in `tab_id`. Request must be
  // existent for user to be able to dismiss it.
  void UserDismissedSiteAccessRequest(content::WebContents* web_contents,
                                      int tab_id,
                                      const ExtensionId& extension_id);

  // Returns whether `tab_id` has an active site access request for
  // `extension_id`.
  bool HasActiveSiteAccessRequest(int tab_id, const ExtensionId& extension_id);

  // Adds `extension_id` to the `extensions_with_previous_broad_access` set.
  void AddExtensionToPreviousBroadSiteAccessSet(
      const ExtensionId& extension_id);

  // Removes `extension_id` from the `extensions_with_previous_broad_access`
  // set, if existent.
  void RemoveExtensionFromPreviousBroadSiteAccessSet(
      const ExtensionId& extension_id);

  // Returns whether `extension_id` is in the
  // `extensions_with_previous_broad_access` set.
  bool HasPreviousBroadSiteAccess(const ExtensionId& extension_id);

  // Notifies `observers_` that the permissions have been updated for an
  // extension.
  void NotifyExtensionPermissionsUpdated(const Extension& extension,
                                         const PermissionSet& permissions,
                                         UpdateReason reason);

  // Notifies `observers_` that `extension` has been granted active tab
  // permission for `web_contents` on `tab_id`.
  void NotifyActiveTabPermisssionGranted(content::WebContents* web_contents,
                                         int tab_id,
                                         const Extension& extension);

  // Notifies `observers_`that show access requests in toolbar pref changed.
  void NotifyShowAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests);

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  using PassKey = base::PassKey<PermissionsManager>;
  friend class SiteAccessRequestsHelper;

  // Called whenever `user_permissions_` have changed.
  void OnUserPermissionsSettingsChanged();

  // Removes `origin` from the list of sites the user has allowed all
  // extensions to run on and saves the change to `extension_prefs_`. Returns if
  // the site has been removed.
  bool RemovePermittedSiteAndUpdatePrefs(const url::Origin& origin);

  // Removes `origin` from the list of sites the user has blocked all
  // extensions from running on and saves the change to `extension_prefs_`.
  // Returns if the site has been removed.
  bool RemoveRestrictedSiteAndUpdatePrefs(const url::Origin& origin);

  // Updates the given `extension` with the new `user_permitted_set` of sites
  // all extensions are allowed to run on. Note that this only updates the
  // permissions in the browser; updates must then be sent separately to the
  // renderer and network service.
  void UpdatePermissionsWithUserSettings(
      const Extension& extension,
      const PermissionSet& user_permitted_set);

  // Returns the site access requests helper for `tab_id` or nullptr if it
  // doesn't exist.
  SiteAccessRequestsHelper* GetSiteAccessRequestsHelperFor(int tab_id);

  // Returns the site access requests helper for `tab_id`. If the helper doesn't
  // exist for such tab, it creates a new one.
  SiteAccessRequestsHelper* GetOrCreateSiteAccessRequestsHelperFor(
      content::WebContents* web_contents,
      int tab_id);

  // Deletes helper corresponding to `tab_id` by removing its entry from
  // `requests_helper_`.
  void DeleteSiteAccessRequestHelperFor(int tab_id);

  // Notifies `observers_` that user permissions have changed.
  void NotifyUserPermissionSettingsChanged();

  // Notifies `observers_` that site access requests were cleared on `tab_id`.
  void NotifySiteAccessRequestsCleared(int tab_id);

  base::ObserverList<Observer>::Unchecked observers_;

  // The associated browser context.
  const raw_ptr<content::BrowserContext> browser_context_;

  const raw_ptr<ExtensionPrefs> extension_prefs_;
  UserPermissionsSettings user_permissions_;

  // Helpers that store and manage the site access requests per tab.
  std::map<int, std::unique_ptr<SiteAccessRequestsHelper>> requests_helpers_;

  // Stores extensions whose site access was updated using the extensions
  // menu and previously had broad site access. This is done to preserve the
  // previous site access state when toggling on the extension's site access
  // using ExtensionsMenuViewController.
  // The set only reflects site access changes made in the extensions menu. An
  // extension's site access could be changed elsewhere (e.g
  // chrome://extensions) but wouldn't be added/removed to/from this set. This
  // is ok, since the main goal is to represent the last explicit state in
  // the extensions menu.
  std::set<ExtensionId> extensions_with_previous_broad_access_;

  base::WeakPtrFactory<PermissionsManager> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PERMISSIONS_MANAGER_H_
