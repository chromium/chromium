// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PERMISSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_
#define EXTENSIONS_BROWSER_PERMISSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_

#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"
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
  void GrantIfRequested(const Extension* extension);

  // Clears any tab-specific permissions for an extension with `id` if it has
  // been granted (otherwise does nothing) on `tab_id_` and notifies renderers.
  void ClearActiveExtensionAndNotify(const ExtensionId& id);

  // Clears tab-specific permissions for all extensions. Used only for testing.
  void RevokeForTesting();

 private:
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

  // The tab ID for this tab.
  const int tab_id_;

  // Extensions with the activeTab permission that have been granted
  // tab-specific permissions until the next navigation/refresh.
  ExtensionSet granted_extensions_;

  // Listen to extension unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PERMISSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_
