// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include <string_view>

#include "base/auto_reset.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "extensions/common/constants.h"
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

namespace {

const GURL* g_item_snippet_url_for_test_ = nullptr;

}  // namespace

const char kChromeWebstoreBaseURL[] = "https://chrome.google.com/webstore";
const char kNewChromeWebstoreBaseURL[] = "https://chromewebstore.google.com/";
const char kExtensionsDocsWhatsNewURL[] =
    "https://developer.chrome.com/docs/extensions/whats-new";
const char kChromeWebstoreUpdateURL[] =
    "https://clients2.google.com/service/update2/crx";
const char kChromeWebstoreApiURL[] = "https://chromewebstore.googleapis.com/";

const char kAppMenuUtmSource[] = "ext_app_menu";
const char kExtensionsMenuUtmSource[] = "ext_extensions_menu";
const char kExtensionsSidebarUtmSource[] = "ext_sidebar";
const char kCustomActionIphUtmSource[] = "ext_zero_state_promo_generic_iph";
const char kCustomUiChipIphV1UtmSource[] = "ext_zero_state_promo_chips_iph";
const char kCustomUiChipIphV2UtmSource[] = "ext_zero_state_promo_chips_iph_v2";
const char kCustomUiChipIphV3UtmSource[] = "ext_zero_state_promo_chips_iph_v3";
const char kCustomUiPlainLinkIphUtmSource[] = "ext_zero_state_promo_links_iph";
const char kNtpPromo1pUtmSource[] = "ext_ntp_promo_1p";
const char kNtpPromo2pUtmSource[] = "ext_ntp_promo_2p";
const char kNtpPromoSlUtmSource[] = "ext_ntp_promo_sl";

GURL GetWebstoreLaunchURL() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetWebstoreBaseURL();
  return GURL(kChromeWebstoreBaseURL);
}

GURL GetDocsWhatsNewURL() {
  return GURL(kExtensionsDocsWhatsNewURL);
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

GURL GetWebstoreExtensionsCategoryURL() {
  GURL base_url = GetNewWebstoreLaunchURL();
  CHECK_EQ(base_url.path(), "/")
      << "GURL::Resolve() won't work with a URL with a path.";
  return base_url.Resolve("category/extensions");
}

std::string GetWebstoreItemDetailURLPrefix() {
  return GetNewWebstoreLaunchURL().spec() + "detail/";
}

GURL GetWebstoreItemSnippetURL(const extensions::ExtensionId& extension_id) {
  if (g_item_snippet_url_for_test_) {
    // Return `<base URL><extension_id>`. There is no suffix if the URL is
    // overridden by a test.
    return GURL(base::StringPrintf("%s%s",
                                   g_item_snippet_url_for_test_->spec().c_str(),
                                   extension_id.c_str()));
  }

  // Return `<base URL><extension_id><suffix>`.
  return GURL(kChromeWebstoreApiURL)
      .Resolve(base::StringPrintf("v2/items/%s:fetchItemSnippet",
                                  extension_id.c_str()));
}

GURL GetWebstoreBlockStatusURL() {
  return GURL(kChromeWebstoreApiURL)
      .Resolve(base::StringPrintf(
          "v2/items:batchFetchItemBlockStatusForEnterprise"));
}

base::AutoReset<const GURL*> SetItemSnippetURLForTesting(const GURL* test_url) {
  return base::AutoReset<const GURL*>(&g_item_snippet_url_for_test_, test_url);
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
  return GURL(base::StringPrintf("%sdetail/%s/report?utm_source=%s",
                                 GetNewWebstoreLaunchURL().spec().c_str(),
                                 extension_id.c_str(), referrer_id.c_str()));
}

GURL GetNewWebstoreItemRecommendationsUrl(
    const extensions::ExtensionId& extension_id) {
  return GURL(base::StringPrintf("%sdetail/%s/related-recommendations",
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
  return (update_url.host() == store_url.host() &&
          update_url.path() == store_url.path());
}

bool IsWebstoreApiUrl(const GURL& url) {
  url::Origin origin = url::Origin::Create(url);
  return origin.IsSameOriginWith(GURL(kChromeWebstoreApiURL));
}

bool IsBlocklistUpdateUrl(const GURL& url) {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->IsBlocklistUpdateURL(url);
  return false;
}

bool IsSafeBrowsingUrl(const GURL& url) {
  url::Origin origin = url::Origin::Create(url);
  std::string_view path = url.path();
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
