// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PERMISSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_
#define EXTENSIONS_BROWSER_PERMISSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class Extension;

// Responsible for granting and revoking tab-specific permissions to extensions
// with the activeTab or tabCapture permission.
class ActiveTabPermissionGranter
    : public content::WebContentsObserver,
      public extensions::ExtensionRegistryObserver,
      public content::WebContentsUserData<ActiveTabPermissionGranter> {
 public:
  ActiveTabPermissionGranter(const ActiveTabPermissionGranter&) = delete;
  ActiveTabPermissionGranter& operator=(const ActiveTabPermissionGranter&) =
      delete;

  ~ActiveTabPermissionGranter() override;

  // If `extension` has the activeTab or tabCapture permission, grants
  // tab-specific permissions to it until the next page navigation or refresh.
  //
  // `on_granted` is called (asynchronously) after the grant has taken effect
  // everywhere, including in the network service (where CORS and ORB depend
  // on the grant).  Callers that notify the extension about the user action
  // that caused the grant (e.g. callers that dispatch `action.onClicked`)
  // should do this from `on_granted`.  This way the extension can use the
  // grant right away.  `on_granted` is called also if there is nothing to
  // grant, or if the grant is revoked before it takes effect.
  void GrantIfRequested(const Extension* extension,
                        base::OnceClosure on_granted);

  // Returns true if `extension` has been granted tab-specific permissions.
  bool IsGranted(const Extension* extension) const;

  // Clears any tab-specific permissions for an extension with `id` if it has
  // been granted (otherwise does nothing) on `tab_id_` and notifies renderers.
  void ClearActiveExtensionAndNotify(const ExtensionId& id);

  // Clears tab-specific permissions for all extensions. Used only for testing.
  void RevokeForTesting();

 private:
  // A grant that hasn't yet taken effect in the Network Service.
  struct PendingGrant {
    PendingGrant();
    PendingGrant(PendingGrant&&);
    PendingGrant& operator=(PendingGrant&&);
    ~PendingGrant();

    // ID of the Network Service update that the grant is waiting for.
    int update_id = 0;

    // Callbacks to run after the grant takes effect.
    std::vector<base::OnceClosure> on_granted_callbacks;
  };

  friend class content::WebContentsUserData<ActiveTabPermissionGranter>;

  FRIEND_TEST_ALL_PREFIXES(ExtensionActionRunnerFencedFrameBrowserTest,
                           FencedFrameDoesNotClearActiveExtensions);

  ActiveTabPermissionGranter(content::WebContents* web_contents,
                             int tab_id,
                             content::BrowserContext* browser_context);

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Clears any tab-specific permissions for all extensions on `tab_id_` and
  // notifies renderers.
  void ClearGrantedExtensionsAndNotify();

  // Clears any tab-specific permissions for all extensions in
  // `granted_extensions_to_remove` on `tab_id_` and notifies renderers.
  void ClearGrantedExtensionsAndNotify(
      const ExtensionSet& granted_extensions_to_remove);

  // Notifies renderers (and `ScriptInjectionTracker`) that `new_hosts` have
  // been granted to `extension`.
  void NotifyRenderers(const Extension& extension,
                       const URLPatternSet& new_hosts);

  // Called after the Network Service has processed the update with ID
  // `update_id` (sent when granting permissions to `extension`).  Notifies
  // observers and runs `on_granted` callbacks (unless the grant has been
  // revoked in the meantime).
  void OnNetworkServiceUpdated(scoped_refptr<const Extension> extension,
                               int update_id);

  // Runs (asynchronously) the `on_granted` callbacks of the grant to the
  // extension with `id`, if the grant hasn't taken effect yet.
  void CancelPendingGrant(const ExtensionId& id);

  // Notifies observers that the permission has been granted.  Called after
  // the Network Service has been updated, so that observers can rely on CORS
  // and ORB taking the new permissions into account.
  void NotifyObservers(const Extension& extension);

  // The tab ID for this tab.
  const int tab_id_;

  // Extensions with the activeTab permission that have been granted
  // tab-specific permissions until the next navigation/refresh.
  ExtensionSet granted_extensions_;

  // Grants (in `granted_extensions_`) that haven't yet taken effect in the
  // Network Service.
  std::map<ExtensionId, PendingGrant> pending_grants_;

  // The last `PendingGrant::update_id` that has been used.
  int last_update_id_ = 0;

  // Listen to extension unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::WeakPtrFactory<ActiveTabPermissionGranter> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PERMISSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_
