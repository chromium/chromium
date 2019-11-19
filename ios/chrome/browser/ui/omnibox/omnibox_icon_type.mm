// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_icon_type.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* GetLocationBarSecurityIconTypeAssetName(
    LocationBarSecurityIconType iconType) {
  switch (iconType) {
    case INFO:
      return @"location_bar_info";
    case SECURE:
      return @"location_bar_secure";
    case NOT_SECURE_WARNING:
      return @"location_bar_not_secure_warning";
    case LOCATION_BAR_SECURITY_ICON_TYPE_COUNT:
      NOTREACHED();
      return @"location_bar_info";
  }
}
