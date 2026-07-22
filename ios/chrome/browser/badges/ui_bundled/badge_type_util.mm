// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/badge_type_util.h"

#import <ostream>

#import "base/feature_list.h"
#import "base/notreached.h"
#import "ios/chrome/browser/badges/model/features.h"

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
    case InfobarType::kInfobarTypeReaderMode:
      return kBadgeTypeReaderMode;
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
    case kBadgeTypeReaderMode:
      return InfobarType::kInfobarTypeReaderMode;
    default:
      NOTREACHED() << "Unsupported badge type.";
  }
}

bool IsBadgeSupportedForInfobarType(InfobarType infobar_type) {
  if (!base::FeatureList::IsEnabled(kAutofillBadgeRemoval)) {
    return BadgeTypeForInfobarType(infobar_type) != kBadgeTypeNone;
  }
  // TODO(crbug.com/440366193): Remove this ad hoc logic once we can fully
  // cleanup the autofill and password badges code once we are done
  // experimenting.
  switch (infobar_type) {
    case InfobarType::kInfobarTypePasswordSave:
    case InfobarType::kInfobarTypePasswordUpdate:
    case InfobarType::kInfobarTypeSaveCard:
    case InfobarType::kInfobarTypeSaveAutofillAddressProfile:
    case InfobarType::kInfobarTypeAutofillAiSaveEntity:
      // Special case where we dynamically want to exclude the badge for
      // certain infobars while still keeping a badge type for the infobar
      // in BadgeTypeForInfobarType(). This ad hoc logic is temporary the
      // time we sunset these badges.
      return false;
    case InfobarType::kInfobarTypeConfirm:
    case InfobarType::kInfobarTypeTranslate:
    case InfobarType::kInfobarTypePermissions:
    case InfobarType::kInfobarTypeTailoredSecurityService:
    case InfobarType::kInfobarTypeSyncError:
    case InfobarType::kInfobarTypeEnhancedSafeBrowsing:
    case InfobarType::kInfobarTypeSignin:
    case InfobarType::kInfobarTypeCollaborationGroup:
    case InfobarType::kInfobarTypeCollaborationOutOfDate:
    case InfobarType::kInfobarTypeSaveCvc:
    case InfobarType::kInfobarTypeReaderMode:
    case InfobarType::kInfobarTypeFormsAiPrivateInference:
      return BadgeTypeForInfobarType(infobar_type) != kBadgeTypeNone;
  }
}
