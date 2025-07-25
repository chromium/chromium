// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/parsed_share_extension_entry.h"

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

bool IsEntryURL(ShareExtensionItemReceived type) {
  switch (type) {
    case ShareExtensionItemReceived::kRradinglistEntry:
    case ShareExtensionItemReceived::kBookmarkEntry:
    case ShareExtensionItemReceived::kOpenInChromeEntry:
    case ShareExtensionItemReceived::kOpenInChromeIncognitoEntry:
      return true;
    case ShareExtensionItemReceived::kShareExtensionItemReceivedNone:
    case ShareExtensionItemReceived::kInvalidEntry:
    case ShareExtensionItemReceived::kCancelledEntry:
    case ShareExtensionItemReceived::kImageSearchEntry:
    case ShareExtensionItemReceived::kTextSearchEntry:
    case ShareExtensionItemReceived::kIncognitoImageSearchEntry:
    case ShareExtensionItemReceived::kIncognitoTextSearchEntry:
    case ShareExtensionItemReceived::kShareExtensionItemReceivedCount:
      return false;
  }
}

}  // namespace

@implementation ParsedShareExtensionEntry

- (BOOL)parsedEntryIsValid {
  GURL gurl = net::GURLWithNSURL(self.url);

  BOOL isURL = IsEntryURL(self.type);
  BOOL isURLValid = gurl.is_valid() && gurl.SchemeIsHTTPOrHTTPS();

  return self.source && self.date &&
         self.type !=
             ShareExtensionItemReceived::kShareExtensionItemReceivedNone &&
         ((isURL && isURLValid) || !isURL);
}

@end
