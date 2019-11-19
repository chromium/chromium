// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_throttle.h"

#include <string>

#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/page_transition_types.h"

namespace extensions {

ExtensionNavigationThrottle::ExtensionNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

ExtensionNavigationThrottle::~ExtensionNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
ExtensionNavigationThrottle::WillStartOrRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(web_contents->GetBrowserContext());

  // Is this navigation targeting an extension resource?
  const GURL& url = navigation_handle()->GetURL();
  bool url_has_extension_scheme = url.SchemeIs(kExtensionScheme);
  url::Origin target_origin = url::Origin::Create(url);
  const Extension* target_extension = nullptr;
  if (url_has_extension_scheme) {
    // "chrome-extension://" URL.
    target_extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(url);
  } else if (target_origin.scheme() == kExtensionScheme) {
    // "blob:chrome-extension://" or "filesystem:chrome-extension://" URL.
    DCHECK(url.SchemeIsFileSystem() || url.SchemeIsBlob());
    target_extension =
        registry->enabled_extensions().GetByID(target_origin.host());
  } else {
    // If the navigation is not to a chrome-extension resource, no need to
    // perform any more checks; it's outside of the purview of this throttle.
    return content::NavigationThrottle::PROCEED;
  }

  // If the navigation is to an unknown or disabled extension, block it.
  if (!target_extension) {
    // TODO(nick): This yields an unsatisfying error page; use a different error
    // code once that's supported. https://crbug.com/649869
    return content::NavigationThrottle::BLOCK_REQUEST;
  }

  // Hosted apps don't have any associated resources outside of icons, so
  // block any requests to URLs in their extension origin.
  if (target_extension->is_hosted_app()) {
    base::StringPiece resource_root_relative_path =
        url.path_piece().empty() ? base::StringPiece()
                                 : url.path_piece().substr(1);
    if (!IconsInfo::GetIcons(target_extension)
             .ContainsPath(resource_root_relative_path)) {
      return content::NavigationThrottle::BLOCK_REQUEST;
    }
  }

  // Block all navigations to blob: or filesystem: URLs with extension
  // origin from non-extension processes.  See https://crbug.com/645028 and
  // https://crbug.com/836858.
  bool current_frame_is_extension_process =
      !!registry->enabled_extensions().GetExtensionOrAppByURL(
          navigation_handle()->GetStartingSiteInstance()->GetSiteURL());

  if (!url_has_extension_scheme && !current_frame_is_extension_process) {
    // Relax this restriction for apps that use <webview>.  See
    // https://crbug.com/652077.
    bool has_webview_permission =
        target_extension->permissions_data()->HasAPIPermission(
            APIPermission::kWebView);
    if (!has_webview_permission)
      return content::NavigationThrottle::CANCEL;
  }

  if (navigation_handle()->IsInMainFrame()) {
    guest_view::GuestViewBase* guest =
        guest_view::GuestViewBase::FromWebContents(web_contents);
    if (url_has_extension_scheme && guest) {
      // This only handles top-level navigations. For subresources, is is done
      // in url_request_util::AllowCrossRendererResourceLoad.
      const std::string& owner_extension_id = guest->owner_host();
      const Extension* owner_extension =
          registry->enabled_extensions().GetByID(owner_extension_id);

      std::string partition_domain;
      std::string partition_id;
      bool in_memory = false;
      bool is_guest = WebViewGuest::GetGuestPartitionConfigForSite(
          navigation_handle()->GetStartingSiteInstance()->GetSiteURL(),
          &partition_domain, &partition_id, &in_memory);

      bool allowed = true;
      url_request_util::AllowCrossRendererResourceLoadHelper(
          is_guest, target_extension, owner_extension, partition_id, url.path(),
          navigation_handle()->GetPageTransition(), &allowed);
      if (!allowed)
        return content::NavigationThrottle::BLOCK_REQUEST;
    }
  }

  // Browser-initiated requests are always considered trusted, and thus allowed.
  //
  // Note that GuestView navigations initiated by the embedder also count as a
  // browser-initiated navigation.
  if (!navigation_handle()->IsRendererInitiated())
    return content::NavigationThrottle::PROCEED;

  // All renderer-initiated navigations must have an initiator.
  DCHECK(navigation_handle()->GetInitiatorOrigin().has_value());
  const url::Origin& initiator_origin =
      navigation_handle()->GetInitiatorOrigin().value();

  // Navigations from chrome://, devtools:// or chrome-search:// pages need to
  // be allowed, even if the target |url| is not web-accessible.  See also:
  // - https://crbug.com/662602
  // - similar checks in extensions::ResourceRequestPolicy::CanRequestResource
  if (initiator_origin.scheme() == content::kChromeUIScheme ||
      initiator_origin.scheme() == content::kChromeDevToolsScheme ||
      ExtensionsBrowserClient::Get()->ShouldSchemeBypassNavigationChecks(
          initiator_origin.scheme())) {
    return content::NavigationThrottle::PROCEED;
  }

  // An extension can initiate navigations to any of its resources.
  if (initiator_origin == target_origin)
    return content::NavigationThrottle::PROCEED;

  // Cancel cross-origin-initiator navigations to blob: or filesystem: URLs.
  if (!url_has_extension_scheme)
    return content::NavigationThrottle::CANCEL;

  // Cross-origin-initiator navigations require that the |url| is in the
  // manifest's "web_accessible_resources" section.
  if (!WebAccessibleResourcesInfo::IsResourceWebAccessible(target_extension,
                                                           url.path())) {
    return content::NavigationThrottle::BLOCK_REQUEST;
  }

  // A platform app may not be loaded in an <iframe> by another origin.
  //
  // In fact, platform apps may not have any cross-origin iframes at all;
  // for non-extension origins of |url| this is enforced by means of a
  // Content Security Policy. But CSP is incapable of blocking the
  // chrome-extension scheme. Thus, this case must be handled specially
  // here.
  if (target_extension->is_platform_app())
    return content::NavigationThrottle::CANCEL;

  // A platform app may not load another extension in an <iframe>.
  const Extension* initiator_extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(
          initiator_origin.GetURL());
  if (initiator_extension && initiator_extension->is_platform_app())
    return content::NavigationThrottle::BLOCK_REQUEST;

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
ExtensionNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

content::NavigationThrottle::ThrottleCheckResult
ExtensionNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

const char* ExtensionNavigationThrottle::GetNameForLogging() {
  return "ExtensionNavigationThrottle";
}

}  // namespace extensions
