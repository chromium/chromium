// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"

#import "base/check.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "components/reading_list/core/offline_url_utils.h"
#import "components/reading_list/core/reading_list_entry.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "net/base/url_util.h"

namespace {
const char kEntryURLQueryParam[] = "entryURL";
const char kReloadURLQueryParam[] = "reload";
}  // namespace

namespace reading_list {

GURL OfflineURLForURL(const GURL& entry_url) {
  DCHECK(entry_url.is_valid());
  GURL page_url(kChromeUIOfflineURL);
  GURL::Replacements replacements;
  page_url = page_url.ReplaceComponents(replacements);
  page_url = net::AppendQueryParameter(page_url, kEntryURLQueryParam,
                                       entry_url.spec());

  return page_url;
}

GURL OfflineReloadURLForURL(const GURL& entry_url) {
  DCHECK(entry_url.is_valid());
  GURL page_url(kChromeUIOfflineURL);
  page_url = net::AppendQueryParameter(page_url, kReloadURLQueryParam,
                                       entry_url.spec());

  return page_url;
}

GURL EntryURLForOfflineURL(const GURL& offline_url) {
  std::string entry_url_string;
  if (net::GetValueForKeyInQuery(offline_url, kEntryURLQueryParam,
                                 &entry_url_string)) {
    GURL entry_url = GURL(entry_url_string);
    if (entry_url.is_valid()) {
      return entry_url;
    }
  }
  return GURL();
}

GURL ReloadURLForOfflineURL(const GURL& offline_url) {
  std::string reload_url_string;
  if (net::GetValueForKeyInQuery(offline_url, kReloadURLQueryParam,
                                 &reload_url_string)) {
    GURL reload_url = GURL(reload_url_string);
    if (reload_url.is_valid()) {
      return reload_url;
    }
  }
  return GURL();
}

bool IsOfflineURL(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme) && url.host() == kChromeUIOfflineHost;
}

bool IsOfflineEntryURL(const GURL& url) {
  return IsOfflineURL(url) && EntryURLForOfflineURL(url).is_valid();
}

bool IsOfflineReloadURL(const GURL& url) {
  return IsOfflineURL(url) && ReloadURLForOfflineURL(url).is_valid();
}
}  // namespace reading_list
