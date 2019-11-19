// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/child_process_security_policy.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state/login_state.h"
#endif  // defined(OS_CHROMEOS)

using extensions::PermissionsData;

namespace {

// Returns true if the scheme is one we want to allow extensions to have access
// to. Extensions still need specific permissions for a given URL, which is
// covered by CanExtensionAccessURL.

// TODO(karandeepb): This allows more schemes than
// ExtensionWebRequestEventRouter::RequestFiler, which specifies the schemes
// allowed by web request event listeners. Consolidate the two.
bool HasWebRequestScheme(const GURL& url) {
  return (url.SchemeIs(url::kAboutScheme) || url.SchemeIs(url::kFileScheme) ||
          url.SchemeIs(url::kFileSystemScheme) ||
          url.SchemeIs(url::kFtpScheme) || url.SchemeIsHTTPOrHTTPS() ||
          url.SchemeIs(extensions::kExtensionScheme) || url.SchemeIsWSOrWSS());
}

bool g_allow_all_extension_locations_in_public_session = false;

PermissionsData::PageAccess GetHostAccessForURL(
    const extensions::Extension& extension,
    const GURL& url,
    int tab_id) {
  // about: URLs are not covered in host permissions, but are allowed
  // anyway.
  if (url.SchemeIs(url::kAboutScheme) ||
      url::IsSameOriginWith(url, extension.url())) {
    return PermissionsData::PageAccess::kAllowed;
  }

  return extension.permissions_data()->GetPageAccess(url, tab_id,
                                                     nullptr /*error*/);
}

PermissionsData::PageAccess CanExtensionAccessURLInternal(
    extensions::PermissionHelper* permission_helper,
    const std::string& extension_id,
    const GURL& url,
    int tab_id,
    bool crosses_incognito,
    WebRequestPermissions::HostPermissionsCheck host_permissions_check,
    const base::Optional<url::Origin>& initiator,
    const base::Optional<content::ResourceType>& resource_type) {
  const extensions::Extension* extension =
      permission_helper->extension_registry()->enabled_extensions().GetByID(
          extension_id);
  if (!extension)
    return PermissionsData::PageAccess::kDenied;

  // Prevent viewing / modifying requests initiated by a host protected by
  // policy.
  if (initiator &&
      extension->permissions_data()->IsPolicyBlockedHost(initiator->GetURL()))
    return PermissionsData::PageAccess::kDenied;

// When restrictions are enabled in Public Session, allow all URLs for
// webRequests initiated by a regular extension (but don't allow chrome://
// URLs).
#if defined(OS_CHROMEOS)
  if (chromeos::LoginState::IsInitialized() &&
      chromeos::LoginState::Get()->ArePublicSessionRestrictionsEnabled() &&
      extension->is_extension() && !url.SchemeIs("chrome")) {
    // Make sure that the extension is truly installed by policy (the assumption
    // in Public Session is that all extensions are installed by policy).
    CHECK(g_allow_all_extension_locations_in_public_session ||
          extensions::Manifest::IsPolicyLocation(extension->location()));
    return PermissionsData::PageAccess::kAllowed;
  }
#endif

  // Check if this event crosses incognito boundaries when it shouldn't.
  if (crosses_incognito && !permission_helper->CanCrossIncognito(extension))
    return PermissionsData::PageAccess::kDenied;

  switch (host_permissions_check) {
    case WebRequestPermissions::DO_NOT_CHECK_HOST:
      return PermissionsData::PageAccess::kAllowed;
      break;
    case WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL: {
      PermissionsData::PageAccess access =
          GetHostAccessForURL(*extension, url, tab_id);

      bool is_navigation_request =
          resource_type && content::IsResourceTypeFrame(*resource_type);

      // For sub-resource (non-navigation) requests, if access to the host was
      // withheld, check if the extension has access to the initiator. If it
      // does, allow the extension to see the request. This is important for
      // extensions with webRequest to work well with runtime host permissions.
      if (!is_navigation_request &&
          access == PermissionsData::PageAccess::kWithheld) {
        PermissionsData::PageAccess initiator_access =
            initiator
                ? GetHostAccessForURL(*extension, initiator->GetURL(), tab_id)
                : PermissionsData::PageAccess::kDenied;
        if (initiator_access == PermissionsData::PageAccess::kAllowed)
          access = PermissionsData::PageAccess::kAllowed;
      }
      return access;
      break;
    }
    case WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR: {
      PermissionsData::PageAccess request_access =
          GetHostAccessForURL(*extension, url, tab_id);

      bool is_navigation_request =
          resource_type && content::IsResourceTypeFrame(*resource_type);

      // Only require access to the initiator for sub-resource (non-navigation)
      // requests. See crbug.com/918137.
      // TODO(karandeepb): Should service worker navigation preload requests be
      // treated similarly?
      if (is_navigation_request)
        return request_access;

      if (request_access == PermissionsData::PageAccess::kDenied)
        return request_access;

      if (!initiator || initiator->opaque())
        return request_access;

      DCHECK(request_access == PermissionsData::PageAccess::kWithheld ||
             request_access == PermissionsData::PageAccess::kAllowed);

      // Possible remaining states:
      // ----------------------------------------------------
      // | Initiator access| Request access| Expected access|
      // ----------------------------------------------------
      // | Withheld        | Withheld      | Withheld       |
      // | Withheld        | Allowed       | Withheld       |
      // | Allowed         | Withheld      | Allowed        |
      // | Allowed         | Allowed       | Allowed        |
      // | Denied          | *             | Denied         |
      // ----------------------------------------------------

      // Note: The only interesting case is when the access to a sub-resource
      // request is withheld but the access to initiator is allowed. In this
      // case, we allow access to the request. This is important for extensions
      // with webRequest to work well with runtime host permissions. See
      // crbug.com/851722.

      return GetHostAccessForURL(*extension, initiator->GetURL(), tab_id);
      break;
    }
    case WebRequestPermissions::REQUIRE_ALL_URLS:
      return extension->permissions_data()->HasEffectiveAccessToAllHosts()
                 ? PermissionsData::PageAccess::kAllowed
                 : PermissionsData::PageAccess::kDenied;
      break;
  }

  NOTREACHED();
  return PermissionsData::PageAccess::kDenied;
}

// Returns true if |request|.url is of the form clients[0-9]*.google.com.
bool IsSensitiveGoogleClientUrl(const extensions::WebRequestInfo& request) {
  const GURL& url = request.url;

  // TODO(battre) Merge this, CanExtensionAccessURL and
  // PermissionsData::CanAccessPage into one function.
  static constexpr char kGoogleCom[] = "google.com";
  static constexpr char kClient[] = "clients";
  constexpr size_t kGoogleComLength = base::size(kGoogleCom) - 1;
  constexpr size_t kClientLength = base::size(kClient) - 1;

  if (!url.DomainIs(kGoogleCom))
    return false;

  base::StringPiece host = url.host_piece();

  while (host.ends_with("."))
    host.remove_suffix(1u);

  // Check for "clients[0-9]*.google.com" hosts.
  // This protects requests to several internal services such as sync,
  // extension update pings, captive portal detection, fraudulent certificate
  // reporting, autofill and others.
  //
  // These URLs are only protected for requests from the browser, and not for
  // requests from common renderers, because clients*.google.com are also used
  // by websites.
  base::StringPiece::size_type pos = host.rfind(kClient);
  if (pos == base::StringPiece::npos)
    return false;

  if (pos > 0 && host[pos - 1] != '.')
    return false;

  for (base::StringPiece::const_iterator
           i = host.begin() + pos + kClientLength,
           end = host.end() - (kGoogleComLength + 1);
       i != end; ++i) {
    if (!isdigit(*i))
      return false;
  }

  return true;
}

}  // namespace

// static
bool WebRequestPermissions::HideRequest(
    extensions::PermissionHelper* permission_helper,
    const extensions::WebRequestInfo& request) {
  if (!HasWebRequestScheme(request.url))
    return true;

  // Requests from <webview> are never hidden.
  if (request.is_web_view)
    return false;

  bool is_request_from_browser = request.render_process_id == -1;

  if (is_request_from_browser) {
    // Browser initiated service worker script requests (e.g., for update check)
    // are not hidden.
    if (request.is_service_worker_script) {
      DCHECK(request.type == content::ResourceType::kServiceWorker ||
             request.type == content::ResourceType::kScript);
      return false;
    }

    // Hide all non-navigation requests made by the browser. crbug.com/884932.
    if (!request.is_navigation_request)
      return true;

    DCHECK(request.type == content::ResourceType::kMainFrame ||
           request.type == content::ResourceType::kSubFrame ||
           request.type == content::ResourceType::kNavigationPreloadMainFrame ||
           request.type == content::ResourceType::kNavigationPreloadSubFrame);

    // Hide sub-frame requests to clientsX.google.com.
    // TODO(crbug.com/890006): Determine if the code here can be cleaned up
    // since browser initiated non-navigation requests are now hidden from
    // extensions.
    if (request.type != content::ResourceType::kMainFrame &&
        IsSensitiveGoogleClientUrl(request)) {
      return true;
    }
  }

  // Hide requests from the Chrome WebStore App.
  if (!is_request_from_browser &&
      permission_helper->process_map()->Contains(extensions::kWebStoreAppId,
                                                 request.render_process_id)) {
    return true;
  }

  const GURL& url = request.url;

  bool is_request_from_webui_renderer =
      !is_request_from_browser &&
      content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
          request.render_process_id);

  if (is_request_from_webui_renderer) {
#if DCHECK_IS_ON()
    const bool is_network_request =
        url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS();
    if (is_network_request) {
      // WebUI renderers should never be making network requests, but we may
      // make some exceptions for now. See https://crbug.com/829412 for
      // details.
      //
      // The DCHECK helps avoid proliferation of such behavior.
      DCHECK(request.initiator.has_value());
      DCHECK(extensions::ExtensionsBrowserClient::Get()
                 ->IsWebUIAllowedToMakeNetworkRequests(*request.initiator))
          << "Unsupported network request from "
          << request.initiator->GetURL().spec() << " for " << url.spec();
    }
#endif  // DCHECK_IS_ON()

    // In any case, we treat the requests as sensitive to ensure that the Web
    // Request API doesn't see them.
    return true;
  }

  // Allow the extension embedder to hide the request.
  if (permission_helper->ShouldHideBrowserNetworkRequest(request))
    return true;

  // Safebrowsing and Chrome Webstore URLs are always protected, i.e. also
  // for requests from common renderers.
  if (extension_urls::IsWebstoreUpdateUrl(url) ||
      extension_urls::IsBlacklistUpdateUrl(url) ||
      extension_urls::IsSafeBrowsingUrl(url::Origin::Create(url),
                                        url.path_piece()) ||
      (url.DomainIs("chrome.google.com") &&
       base::StartsWith(url.path_piece(), "/webstore",
                        base::CompareCase::SENSITIVE))) {
    return true;
  }

  return false;
}

// static
void WebRequestPermissions::
     AllowAllExtensionLocationsInPublicSessionForTesting(bool value) {
  g_allow_all_extension_locations_in_public_session = value;
}

// static
PermissionsData::PageAccess WebRequestPermissions::CanExtensionAccessURL(
    extensions::PermissionHelper* permission_helper,
    const std::string& extension_id,
    const GURL& url,
    int tab_id,
    bool crosses_incognito,
    HostPermissionsCheck host_permissions_check,
    const base::Optional<url::Origin>& initiator,
    content::ResourceType resource_type) {
  return CanExtensionAccessURLInternal(
      permission_helper, extension_id, url, tab_id, crosses_incognito,
      host_permissions_check, initiator, resource_type);
}

// static
bool WebRequestPermissions::CanExtensionAccessInitiator(
    extensions::PermissionHelper* permission_helper,
    const extensions::ExtensionId extension_id,
    const base::Optional<url::Origin>& initiator,
    int tab_id,
    bool crosses_incognito) {
  if (!initiator)
    return true;

  return CanExtensionAccessURLInternal(
             permission_helper, extension_id, initiator->GetURL(), tab_id,
             crosses_incognito,
             WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
             base::nullopt /* initiator */,
             base::nullopt /* resource_type */) ==
         PermissionsData::PageAccess::kAllowed;
}
