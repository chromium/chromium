// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SITE_ACCESS_REQUESTS_HELPER_H_
#define EXTENSIONS_BROWSER_SITE_ACCESS_REQUESTS_HELPER_H_

#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/url_pattern.h"

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

class PermissionsManager;

// Per-tab helper that stores extension's site access requests and restores
// them on cross-origin navigations.
// This class should only be used by PermissionsManager since it's an
// implementation detail that was pulled out for legibility.
class SiteAccessRequestsHelper : public ExtensionRegistryObserver,
                                 public content::WebContentsObserver {
 public:
  using PassKey = base::PassKey<PermissionsManager>;

  SiteAccessRequestsHelper(PassKey pass_key,
                           PermissionsManager* permissions_manager,
                           content::WebContents* web_contents,
                           int tab_id);
  SiteAccessRequestsHelper(const SiteAccessRequestsHelper&) = delete;
  const SiteAccessRequestsHelper& operator=(const SiteAccessRequestsHelper&) =
      delete;
  ~SiteAccessRequestsHelper() override;

  // Adds `extension` to the set of extensions with site access requests.
  // Request will be matched to `filter`, if existent. Extension must not have
  // granted access to the current site.
  void AddRequest(const Extension& extension,
                  const std::optional<URLPattern>& filter);

  // Updates the site access request entry for `extension`. Request will be
  // matched to `filter, if existent.
  void UpdateRequest(const Extension& extension,
                     const std::optional<URLPattern>& filter);

  // Removes `extension_id` from the set of extensions with site access
  // requests. Request will be matches to `filter`, if existent. Returns whether
  // request was removed.
  bool RemoveRequest(const ExtensionId& extension_id,
                     const std::optional<URLPattern>& filter);

  // Removes `extension` from the set of extensions with site access requests
  // iff extension has granted access to the current site. Returns whether
  // request was removed.
  bool RemoveRequestIfGrantedAccess(const Extension& extension);

  // Adds `extension_id` to the set of extension with site access requests that
  // have been dismissed by the user. Request must be existent in
  // `extensions_with_requests_` for user to be able to dismiss it.
  // An extension's request cannot be undismissed by the user. Requests will be
  // reset on cross-origin navigation, along with their dismissals if existent.
  void UserDismissedRequest(const ExtensionId& extension_id);

  // Returns whether `extension_id` has a site access request for this tab.
  bool HasRequest(const ExtensionId& extension_id) const;

  // Returns whether `extension_id` has a site access request that has not been
  // dismissed by the user and matches the URLPattern filter, if existent.
  bool HasActiveRequest(const ExtensionId& extension_id) const;

  // Returns whether helper has any site access requests.
  bool HasRequests();

 private:
  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // PermissionsManager owns this object, thus `permissions_manager_` will
  // always be valid.
  raw_ptr<PermissionsManager> permissions_manager_;
  raw_ptr<content::WebContents> web_contents_;
  int tab_id_;

  // Extensions that have a site access request on this tab's origin. If pattern
  // is provided, request will only be shown for URLs that match it.
  std::map<ExtensionId, std::optional<URLPattern>> extensions_with_requests_;

  // Extensions that have a site access request for this tab's origin which was
  // dismissed by the user.
  std::set<ExtensionId> extensions_with_requests_dismissed_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SITE_ACCESS_REQUESTS_HELPER_H_
