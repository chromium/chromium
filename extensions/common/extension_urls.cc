// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/constants.h"
#include "extensions/common/extensions_client.h"
#include "net/base/escape.h"
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
const char kChromeWebstoreUpdateURL[] =
    "https://clients2.google.com/service/update2/crx";

GURL GetWebstoreLaunchURL() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetWebstoreBaseURL();
  return GURL(kChromeWebstoreBaseURL);
}

// TODO(csharrison,devlin): Migrate the following methods to return
// GURLs.
// TODO(devlin): Try to use GURL methods like Resolve instead of string
// concatenation.
std::string GetWebstoreExtensionsCategoryURL() {
  return GetWebstoreLaunchURL().spec() + "/category/extensions";
}

std::string GetWebstoreItemDetailURLPrefix() {
  return GetWebstoreLaunchURL().spec() + "/detail/";
}

GURL GetWebstoreItemJsonDataURL(const std::string& extension_id) {
  return GURL(GetWebstoreLaunchURL().spec() + "/inlineinstall/detail/" +
              extension_id);
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

GURL GetWebstoreReportAbuseUrl(const std::string& extension_id,
                               const std::string& referrer_id) {
  return GURL(base::StringPrintf("%s/report/%s?utm_source=%s",
                                 GetWebstoreLaunchURL().spec().c_str(),
                                 extension_id.c_str(), referrer_id.c_str()));
}

bool IsWebstoreUpdateUrl(const GURL& update_url) {
  GURL store_url = GetWebstoreUpdateUrl();
  return (update_url.host_piece() == store_url.host_piece() &&
          update_url.path_piece() == store_url.path_piece());
}

bool IsBlacklistUpdateUrl(const GURL& url) {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->IsBlacklistUpdateURL(url);
  return false;
}

bool IsSafeBrowsingUrl(const url::Origin& origin, base::StringPiece path) {
  return origin.DomainIs("sb-ssl.google.com") ||
         origin.DomainIs("safebrowsing.googleapis.com") ||
         (origin.DomainIs("safebrowsing.google.com") &&
          base::StartsWith(path, "/safebrowsing",
                           base::CompareCase::SENSITIVE));
}

}  // namespace extension_urls
