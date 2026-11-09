// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_util.h"

#import "base/notreached.h"

std::optional<LocationBarBadgeType> LocationBarBadgeTypeForInfobarType(
    InfobarType infobar_type) {
  switch (infobar_type) {
    case InfobarType::kInfobarTypeTranslate:
      return LocationBarBadgeType::kTranslate;
    case InfobarType::kInfobarTypePermissions:
      // Default value; actual value depends on `GetStatesForAllPermissions()`
      // of the active `WebState` and is overridden when used.
      return LocationBarBadgeType::kPermissionsCamera;
    case InfobarType::kInfobarTypeReaderMode:
      return LocationBarBadgeType::kReaderMode;
    case InfobarType::kInfobarTypePasswordSave:
    case InfobarType::kInfobarTypePasswordUpdate:
    case InfobarType::kInfobarTypeSaveCard:
    case InfobarType::kInfobarTypeSaveAutofillAddressProfile:
    case InfobarType::kInfobarTypeConfirm:
    case InfobarType::kInfobarTypeTailoredSecurityService:
    case InfobarType::kInfobarTypeSyncError:
    case InfobarType::kInfobarTypeEnhancedSafeBrowsing:
    case InfobarType::kInfobarTypeSignin:
    case InfobarType::kInfobarTypeCollaborationGroup:
    case InfobarType::kInfobarTypeCollaborationOutOfDate:
    case InfobarType::kInfobarTypeSaveCvc:
    case InfobarType::kInfobarTypeAutofillAiSaveEntity:
    case InfobarType::kInfobarTypeContextualCue:
      return std::nullopt;
  }
  NOTREACHED();
}

bool IsContextualPanelEntryPointBadgeType(LocationBarBadgeType badge_type) {
  switch (badge_type) {
    case LocationBarBadgeType::kContextualPanelEntryPointSample:
    case LocationBarBadgeType::kReaderMode:
      return true;
    case LocationBarBadgeType::kNone:
    case LocationBarBadgeType::kPasswordSave:
    case LocationBarBadgeType::kPasswordUpdate:
    case LocationBarBadgeType::kIncognito:
    case LocationBarBadgeType::kOverflow:
    case LocationBarBadgeType::kSaveCard:
    case LocationBarBadgeType::kTranslate:
    case LocationBarBadgeType::kSaveAddressProfile:
    case LocationBarBadgeType::kPermissionsCamera:
    case LocationBarBadgeType::kPermissionsMicrophone:
    case LocationBarBadgeType::kGeminiContextualCueChip:
      return false;
  }
  NOTREACHED();
}

bool IsBadgeFromFactory(LocationBarBadgeType badge_type) {
  switch (badge_type) {
    case LocationBarBadgeType::kPasswordSave:
    case LocationBarBadgeType::kPasswordUpdate:
    case LocationBarBadgeType::kIncognito:
    case LocationBarBadgeType::kOverflow:
    case LocationBarBadgeType::kSaveCard:
    case LocationBarBadgeType::kTranslate:
    case LocationBarBadgeType::kSaveAddressProfile:
    case LocationBarBadgeType::kPermissionsCamera:
    case LocationBarBadgeType::kPermissionsMicrophone:
      return true;
    case LocationBarBadgeType::kNone:
    case LocationBarBadgeType::kContextualPanelEntryPointSample:
    case LocationBarBadgeType::kReaderMode:
    case LocationBarBadgeType::kGeminiContextualCueChip:
      return false;
  }
  NOTREACHED();
}
