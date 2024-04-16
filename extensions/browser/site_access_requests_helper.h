// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SITE_ACCESS_REQUESTS_HELPER_H_
#define EXTENSIONS_BROWSER_SITE_ACCESS_REQUESTS_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

class PermissionsManager;

// Per-tab helper that stores extension's site access requests and restores
// them on cross-origin navigations.
// This class should only be used by PermissionsManager since it's an
// implementation detail that was pulled out for legibility.
class SiteAccessRequestsHelper : public content::WebContentsObserver {
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
  // Extension must not have granted access to the current site.
  void AddRequest(const Extension& extension);

  // Removes `extension_id` from the set of extensions with site access
  // requests.
  void RemoveRequest(const ExtensionId& extension_id);

  // Removes `extension` from the set of extensions with site access requests
  // iff extension has granted access to the current site.
  void RemoveRequestIfGrantedAccess(const Extension& extension);

  // Returns whether `extension_id` has a site access request.
  bool HasRequest(const ExtensionId& extension_id);

  // Returns whether helper has any site access requests.
  bool HasRequests();

 private:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // TODO(crbug.com/330588494): Remove site access request, if existent, for
  // unloaded extension.

  // PermissionsManager owns this object, thus `permissions_manager_` will
  // always be valid.
  raw_ptr<PermissionsManager> permissions_manager_;
  raw_ptr<content::WebContents> web_contents_;
  int tab_id_;

  // Extensions that have a site access request for this tab's origin.
  std::set<ExtensionId> requesting_extensions_;

  // TODO(crbug.com/330588494): Moves dismissed extensions from TabHelper.
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SITE_ACCESS_REQUESTS_HELPER_H_
