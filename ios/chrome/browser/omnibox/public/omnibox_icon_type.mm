// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/public/omnibox_icon_type.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

Symbol GetLocationBarSecuritySymbol(LocationBarSecurityIconType iconType) {
  switch (iconType) {
    case LocationBarSecurityIconType::NONE:
      return SymbolNone;
    case LocationBarSecurityIconType::INFO:
      return SymbolInfoCircle;
    case LocationBarSecurityIconType::SECURE:
      return SymbolSecureLocationBar;
    case LocationBarSecurityIconType::NOT_SECURE_WARNING:
      return SymbolWarningFill;
    case LocationBarSecurityIconType::DANGEROUS:
      return SymbolDangerousOmnibox;
    case LocationBarSecurityIconType::LOCATION_BAR_SECURITY_ICON_TYPE_COUNT:
      NOTREACHED();
  }
}
