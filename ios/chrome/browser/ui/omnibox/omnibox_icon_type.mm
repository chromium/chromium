// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_icon_type.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

NSString* GetLocationBarSecuritySymbolName(
    LocationBarSecurityIconType iconType) {
  switch (iconType) {
    case LocationBarSecurityIconType::NONE:
      return nil;
    case LocationBarSecurityIconType::INFO:
      return kInfoCircleSymbol;
    case LocationBarSecurityIconType::SECURE:
      return kSecureLocationBarSymbol;
    case LocationBarSecurityIconType::NOT_SECURE_WARNING:
      return kWarningFillSymbol;
    case LocationBarSecurityIconType::DANGEROUS:
      return kDangerousOmniboxSymbol;
    case LocationBarSecurityIconType::LOCATION_BAR_SECURITY_ICON_TYPE_COUNT:
      NOTREACHED_IN_MIGRATION();
      return kInfoCircleSymbol;
  }
}
