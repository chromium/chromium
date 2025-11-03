// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/parsed_share_extension_entry.h"

#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

bool IsEntryURL(app_group::ShareExtensionItemType type) {
  return type == app_group::READING_LIST_ITEM ||
         type == app_group::BOOKMARK_ITEM ||
         type == app_group::OPEN_IN_CHROME_ITEM ||
         type == app_group::OPEN_IN_CHROME_INCOGNITO_ITEM;
}

}  // namespace

@implementation ParsedShareExtensionEntry

- (BOOL)parsedEntryIsValid {
  GURL gurl = net::GURLWithNSURL(self.url);

  BOOL isURL = IsEntryURL(self.type);
  BOOL isURLValid = gurl.is_valid() && gurl.SchemeIsHTTPOrHTTPS();

  return self.source && self.date && ((isURL && isURLValid) || !isURL);
}

@end
