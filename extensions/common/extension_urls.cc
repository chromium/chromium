// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include <string_view>

#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extensions_client.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

bool IsSourceFromAnExtension(const std::u16string& source) {
  return GURL(source).SchemeIs(kExtensionScheme) ||
         base::StartsWith(source, u"extensions::",
                          base::CompareCase::SENSITIVE);
}

}  // namespace extensions

namespace extension_urls {

const char kChromeWebstoreBaseURL[] = "https://chrome.google.com/webstore";
const char kNewChromeWebstoreBaseURL[] = "https://chromewebstore.google.com/";
const char kChromeWebstoreUpdateURL[] =
    "https://clients2.google.com/service/update2/crx";

const char kAppMenuUtmSource[] = "ext_app_menu";
const char kExtensionsMenuUtmSource[] = "ext_extensions_menu";
const char kExtensionsSidebarUtmSource[] = "ext_sidebar";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kGetMostChromeUtmSource[] = "gtmooc";
#endif

GURL GetWebstoreLaunchURL() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetWebstoreBaseURL();
  return GURL(kChromeWebstoreBaseURL);
}

GURL GetNewWebstoreLaunchURL() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetNewWebstoreBaseURL();
  return GURL(kNewChromeWebstoreBaseURL);
}

GURL AppendUtmSource(const GURL& url, std::string_view utm_source_value) {
  return net::AppendQueryParameter(url, "utm_source", utm_source_value);
}

// TODO(csharrison,devlin): Migrate the following methods to return
// GURLs.
// TODO(devlin): Try to use GURL methods like Resolve instead of string
// concatenation.
std::string GetWebstoreExtensionsCategoryURL() {
  // TODO(crbug.com/40073814): Refactor this check into
  // extension_urls::GetWebstoreLaunchURL() and fix tests relying on it.
  if (base::FeatureList::IsEnabled(extensions_features::kNewWebstoreURL)) {
    return GetNewWebstoreLaunchURL().spec() + "category/extensions";
  }
  return GetWebstoreLaunchURL().spec() + "/category/extensions";
}

std::string GetWebstoreItemDetailURLPrefix() {
  return GetWebstoreLaunchURL().spec() + "/detail/";
}

GURL GetWebstoreItemJsonDataURL(const extensions::ExtensionId& extension_id) {
  return GURL(GetWebstoreLaunchURL().spec() + "/inlineinstall/detail/" +
              extension_id);
}

GURL GetWebstoreItemSnippetURL(const std::string& extension_id) {
  return GURL(base::StringPrintf(
      "https://chromewebstore.googleapis.com/v2/items/%s:fetchItemSnippet",
      extension_id.c_str()));
}

GURL GetDefaultWebstoreUpdateUrl() {
  return GURL(kChromeWebstoreUpdateURL);
}

GURL GetWebstoreUpdateUrl() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetWebstoreUpdateURL();
  return GetDefaultWebstoreUpdateUrl();
}

GURL GetWebstoreReportAbuseUrl(const extensions::ExtensionId& extension_id,
                               const std::string& referrer_id) {
  return GURL(base::StringPrintf("%s/report/%s?utm_source=%s",
                                 GetWebstoreLaunchURL().spec().c_str(),
                                 extension_id.c_str(), referrer_id.c_str()));
}

GURL GetNewWebstoreItemRecommendationsUrl(
    const extensions::ExtensionId& extension_id) {
  return GURL(base::StringPrintf("%s/detail/%s/related-recommendations",
                                 GetNewWebstoreLaunchURL().spec().c_str(),
                                 extension_id.c_str()));
}

bool IsWebstoreDomain(const GURL& url) {
  return url.DomainIs(GetWebstoreLaunchURL().host()) ||
         url.DomainIs(GetNewWebstoreLaunchURL().host());
}

bool IsWebstoreOrigin(const url::Origin& origin) {
  return origin.IsSameOriginWith(GetWebstoreLaunchURL()) ||
         origin.IsSameOriginWith(GetNewWebstoreLaunchURL());
}

bool IsWebstoreUpdateUrl(const GURL& update_url) {
  GURL store_url = GetWebstoreUpdateUrl();
  return (update_url.host_piece() == store_url.host_piece() &&
          update_url.path_piece() == store_url.path_piece());
}

bool IsBlocklistUpdateUrl(const GURL& url) {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->IsBlocklistUpdateURL(url);
  return false;
}

bool IsSafeBrowsingUrl(const GURL& url) {
  url::Origin origin = url::Origin::Create(url);
  std::string_view path = url.path_piece();
  return origin.DomainIs("sb-ssl.google.com") ||
         origin.DomainIs("safebrowsing.googleapis.com") ||
         (origin.DomainIs("safebrowsing.google.com") &&
          base::StartsWith(path, "/safebrowsing",
                           base::CompareCase::SENSITIVE)) ||
         (safe_browsing::hash_realtime_utils::
              IsHashRealTimeLookupEligibleInSession() &&
          url == safe_browsing::kHashPrefixRealTimeLookupsRelayUrl.Get());
}

}  // namespace extension_urls
