// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_icon_type.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"

#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Specific symbol name for the location bar.
NSString* kSecureLocationBarSymbol = @"lock.fill";

}  // namespace

NSString* GetLocationBarSecurityIconTypeAssetName(
    LocationBarSecurityIconType iconType) {
  switch (iconType) {
    case INFO:
      return @"location_bar_connection_info";
    case SECURE:
      return @"location_bar_connection_secure";
    case NOT_SECURE_WARNING:
      return @"location_bar_connection_dangerous";
    case LOCATION_BAR_SECURITY_ICON_TYPE_COUNT:
      NOTREACHED();
      return @"location_bar_connection_info";
  }
}

NSString* GetLocationBarSecuritySymbolName(
    LocationBarSecurityIconType iconType) {
  switch (iconType) {
    case INFO:
      return kInfoCircleSymbol;
    case SECURE:
      return kSecureLocationBarSymbol;
    case NOT_SECURE_WARNING:
      return kWarningFillSymbol;
    case LOCATION_BAR_SECURITY_ICON_TYPE_COUNT:
      NOTREACHED();
      return kInfoCircleSymbol;
  }
}
