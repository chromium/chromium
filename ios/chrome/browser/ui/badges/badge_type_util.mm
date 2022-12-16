// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_type_util.h"

#import <ostream>
#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BadgeType BadgeTypeForInfobarType(InfobarType infobar_type) {
  switch (infobar_type) {
    case InfobarType::kInfobarTypePasswordSave:
      return kBadgeTypePasswordSave;
    case InfobarType::kInfobarTypePasswordUpdate:
      return kBadgeTypePasswordUpdate;
    case InfobarType::kInfobarTypeSaveAutofillAddressProfile:
      return kBadgeTypeSaveAddressProfile;
    case InfobarType::kInfobarTypeSaveCard:
      return kBadgeTypeSaveCard;
    case InfobarType::kInfobarTypeTranslate:
      return kBadgeTypeTranslate;
    case InfobarType::kInfobarTypePermissions:
      // Default value; actual value would depend on the value of
      // GetStatesForAllPermissions() of the currently active WebState, and be
      // overridden when used.
      return kBadgeTypePermissionsCamera;
    default:
      return kBadgeTypeNone;
  }
}

InfobarType InfobarTypeForBadgeType(BadgeType badge_type) {
  switch (badge_type) {
    case kBadgeTypePasswordSave:
      return InfobarType::kInfobarTypePasswordSave;
    case kBadgeTypePasswordUpdate:
      return InfobarType::kInfobarTypePasswordUpdate;
    case kBadgeTypeSaveAddressProfile:
      return InfobarType::kInfobarTypeSaveAutofillAddressProfile;
    case kBadgeTypeSaveCard:
      return InfobarType::kInfobarTypeSaveCard;
    case kBadgeTypeTranslate:
      return InfobarType::kInfobarTypeTranslate;
    case kBadgeTypePermissionsCamera:
      // Falls through.
    case kBadgeTypePermissionsMicrophone:
      return InfobarType::kInfobarTypePermissions;
    default:
      NOTREACHED() << "Unsupported badge type.";
      return InfobarType::kInfobarTypeConfirm;
  }
}
