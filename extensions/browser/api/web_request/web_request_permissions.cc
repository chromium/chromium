// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include <string_view>

#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "url/gurl.h"
#include "url/origin.h"

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
          url.SchemeIs(extensions::kExtensionScheme) || url.SchemeIsWSOrWSS() ||
          url.SchemeIs(url::kUuidInPackageScheme));
}

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

bool IsWebRequestResourceTypeFrame(
    extensions::WebRequestResourceType web_request_type) {
  return web_request_type == extensions::WebRequestResourceType::MAIN_FRAME ||
         web_request_type == extensions::WebRequestResourceType::SUB_FRAME;
}

PermissionsData::PageAccess CanExtensionAccessURLInternal(
    extensions::PermissionHelper* permission_helper,
    const extensions::ExtensionId& extension_id,
    const GURL& url,
    int tab_id,
    bool crosses_incognito,
    WebRequestPermissions::HostPermissionsCheck host_permissions_check,
    const std::optional<url::Origin>& initiator,
    const std::optional<extensions::WebRequestResourceType>& web_request_type) {
  const extensions::Extension* extension =
      permission_helper->extension_registry()->enabled_extensions().GetByID(
          extension_id);
  if (!extension) {
    return PermissionsData::PageAccess::kDenied;
  }

  // Prevent viewing / modifying requests initiated by a host protected by
  // policy.
  if (initiator &&
      extension->permissions_data()->IsPolicyBlockedHost(initiator->GetURL())) {
    return PermissionsData::PageAccess::kDenied;
  }

  // Check if this event crosses incognito boundaries when it shouldn't.
  if (crosses_incognito && !permission_helper->CanCrossIncognito(extension)) {
    return PermissionsData::PageAccess::kDenied;
  }

  switch (host_permissions_check) {
    case WebRequestPermissions::DO_NOT_CHECK_HOST:
      return PermissionsData::PageAccess::kAllowed;
    case WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL: {
      PermissionsData::PageAccess access =
          GetHostAccessForURL(*extension, url, tab_id);

      bool is_navigation_request =
          web_request_type && IsWebRequestResourceTypeFrame(*web_request_type);

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
        if (initiator_access == PermissionsData::PageAccess::kAllowed) {
          access = PermissionsData::PageAccess::kAllowed;
        }
      }
      return access;
    }
    case WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR: {
      PermissionsData::PageAccess request_access =
          GetHostAccessForURL(*extension, url, tab_id);

      bool is_navigation_request =
          web_request_type && IsWebRequestResourceTypeFrame(*web_request_type);

      // Only require access to the initiator for sub-resource (non-navigation)
      // requests. See crbug.com/918137.
      // TODO(karandeepb): Should service worker navigation preload requests be
      // treated similarly?
      if (is_navigation_request) {
        return request_access;
      }

      if (request_access == PermissionsData::PageAccess::kDenied) {
        return request_access;
      }

      if (!initiator || initiator->opaque()) {
        return request_access;
      }

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
    }
    case WebRequestPermissions::REQUIRE_ALL_URLS:
      return extension->permissions_data()
                     ->active_permissions()
                     .HasEffectiveAccessToAllHosts()
                 ? PermissionsData::PageAccess::kAllowed
                 : PermissionsData::PageAccess::kDenied;
  }

  NOTREACHED_IN_MIGRATION();
  return PermissionsData::PageAccess::kDenied;
}

// Returns true if |request|.url is of the form clients[0-9]*.google.com.
bool IsSensitiveGoogleClientUrl(const extensions::WebRequestInfo& request) {
  const GURL& url = request.url;

  // TODO(battre) Merge this, CanExtensionAccessURL and
  // PermissionsData::CanAccessPage into one function.
  static constexpr char kGoogleCom[] = "google.com";
  static constexpr char kClient[] = "clients";
  constexpr size_t kGoogleComLength = std::size(kGoogleCom) - 1;
  constexpr size_t kClientLength = std::size(kClient) - 1;

  if (!url.DomainIs(kGoogleCom)) {
    return false;
  }

  std::string_view host = url.host_piece();

  while (base::EndsWith(host, ".")) {
    host.remove_suffix(1u);
  }

  // Check for "clients[0-9]*.google.com" hosts.
  // This protects requests to several internal services such as sync,
  // extension update pings, captive portal detection, fraudulent certificate
  // reporting, autofill and others.
  //
  // These URLs are only protected for requests from the browser, and not for
  // requests from common renderers, because clients*.google.com are also used
  // by websites.
  std::string_view::size_type pos = host.rfind(kClient);
  if (pos == std::string_view::npos) {
    return false;
  }

  if (pos > 0 && host[pos - 1] != '.') {
    return false;
  }

  for (std::string_view::const_iterator
           i = host.begin() + pos + kClientLength,
           end = host.end() - (kGoogleComLength + 1);
       i != end; ++i) {
    if (!absl::ascii_isdigit(static_cast<unsigned char>(*i))) {
      return false;
    }
  }

  return true;
}

bool IsMainFrameNavigationRequest(const extensions::WebRequestInfo& request) {
  return request.is_navigation_request &&
         request.web_request_type ==
             extensions::WebRequestResourceType::MAIN_FRAME;
}

}  // namespace

// static
bool WebRequestPermissions::HideRequest(
    extensions::PermissionHelper* permission_helper,
    const extensions::WebRequestInfo& request) {
  if (!HasWebRequestScheme(request.url)) {
    return true;
  }

  // Requests from <webview> are never hidden.
  if (request.is_web_view) {
    return false;
  }

  bool is_request_from_browser = request.render_process_id == -1;

  if (is_request_from_browser) {
    // Browser initiated service worker script requests (e.g., for update check)
    // are not hidden.
    if (request.is_service_worker_script) {
      DCHECK(request.web_request_type ==
             extensions::WebRequestResourceType::SCRIPT);
      return false;
    }

    // Hide all non-navigation requests made by the browser. crbug.com/884932.
    if (!request.is_navigation_request) {
      return true;
    }

    DCHECK(request.web_request_type ==
               extensions::WebRequestResourceType::MAIN_FRAME ||
           request.web_request_type ==
               extensions::WebRequestResourceType::SUB_FRAME ||
           request.web_request_type ==
               extensions::WebRequestResourceType::OBJECT);

    // Hide sub-frame requests to clientsX.google.com.
    // TODO(crbug.com/40595750): Determine if the code here can be cleaned up
    // since browser initiated non-navigation requests are now hidden from
    // extensions.
    if (request.web_request_type !=
            extensions::WebRequestResourceType::MAIN_FRAME &&
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

  // Hide requests initiated by the new Webstore domain, including requests that
  // may be on subframes which have an opaque origin.
  if (request.initiator) {
    const auto& request_tuple_or_precursor_tuple =
        request.initiator->GetTupleOrPrecursorTupleIfOpaque();
    if (request_tuple_or_precursor_tuple ==
        url::SchemeHostPort(extension_urls::GetNewWebstoreLaunchURL())) {
      return true;
    }
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

  // Requests from chrome-untrusted:// are generally sensitive (because they
  // are considered part of browser UI).
  //
  // Main frame navigations from chrome-untrusted:// to non-WebUI origins are an
  // exception: These requests are inspectable by the Web Request API (e.g. by a
  // content filtering extension) and therefore allowlisted.
  if (request.initiator.has_value() &&
      request.initiator->scheme() == content::kChromeUIUntrustedScheme) {
    // The call to `HasWebRequestScheme()` with an early exit at the top already
    // ensures that request.url does not point to a chrome-untrusted:// URL.
    // Therefore, it's not necessary to check the scheme of request.url again.
    bool allowlist = IsMainFrameNavigationRequest(request);

    if (!allowlist) {
      return true;
    }
  }

  // Allow the extension embedder to hide the request.
  if (permission_helper->ShouldHideBrowserNetworkRequest(request)) {
    return true;
  }

  // Safebrowsing and Chrome Webstore URLs are always protected, i.e. also
  // for requests from common renderers.
  // TODO(crbug.com/40235977): it would be nice to be able to just use
  // extension_urls::IsWebstoreDomain for the last two checks here, but the old
  // webstore check specifically requires the path to be checked, not just the
  // domain. However once the old webstore is turned down we can change it over
  // during that cleanup.
  if (extension_urls::IsWebstoreUpdateUrl(url) ||
      extension_urls::IsBlocklistUpdateUrl(url) ||
      extension_urls::IsSafeBrowsingUrl(url) ||
      (url.DomainIs("chrome.google.com") &&
       base::StartsWith(url.path_piece(), "/webstore",
                        base::CompareCase::SENSITIVE)) ||
      url.DomainIs(extension_urls::GetNewWebstoreLaunchURL().host())) {
    return true;
  }

  return false;
}

// static
PermissionsData::PageAccess WebRequestPermissions::CanExtensionAccessURL(
    extensions::PermissionHelper* permission_helper,
    const extensions::ExtensionId& extension_id,
    const GURL& url,
    int tab_id,
    bool crosses_incognito,
    HostPermissionsCheck host_permissions_check,
    const std::optional<url::Origin>& initiator,
    extensions::WebRequestResourceType web_request_type) {
  return CanExtensionAccessURLInternal(
      permission_helper, extension_id, url, tab_id, crosses_incognito,
      host_permissions_check, initiator, web_request_type);
}

// static
bool WebRequestPermissions::CanExtensionAccessInitiator(
    extensions::PermissionHelper* permission_helper,
    const extensions::ExtensionId extension_id,
    const std::optional<url::Origin>& initiator,
    int tab_id,
    bool crosses_incognito) {
  if (!initiator) {
    return true;
  }

  return CanExtensionAccessURLInternal(
             permission_helper, extension_id, initiator->GetURL(), tab_id,
             crosses_incognito,
             WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
             std::nullopt /* initiator */, std::nullopt /* resource_type */) ==
         PermissionsData::PageAccess::kAllowed;
}
