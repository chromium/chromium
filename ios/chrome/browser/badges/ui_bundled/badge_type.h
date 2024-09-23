// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_TYPE_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_TYPE_H_

#import <Foundation/Foundation.h>

// Badge types.
typedef NS_ENUM(NSUInteger, BadgeType) {
  // Badge type for no badge. This is to allow other features to distinguish
  // when a badge is necessary or not. Setting a BadgeModel type to
  // kBadgeTypeNone might result in a crash.
  kBadgeTypeNone = 0,
  // Badge type for the Save Passwords Infobar.
  kBadgeTypePasswordSave = 1,
  // Badge type for the Update Passwords Infobar.
  kBadgeTypePasswordUpdate = 2,
  // Badge type for the Incognito Badge.
  kBadgeTypeIncognito = 3,
  // Badge type for when there are more than one badge to be displayed.
  kBadgeTypeOverflow = 4,
  // Badge type for Save Credit Card Infobar.
  kBadgeTypeSaveCard = 5,
  // Badge type for the Translate Infobar.
  kBadgeTypeTranslate = 6,
  // Badge type for the Save Address Profile Infobar.
  kBadgeTypeSaveAddressProfile = 7,
  // Badge type for the Permissions Infobar with camera icon.
  kBadgeTypePermissionsCamera = 9,
  // Badge type for the Permissions Infobar with microphone icon.
  kBadgeTypePermissionsMicrophone = 10,
  // Badge type for the Parcel Tracking Infobar.
  kBadgeTypeParcelTracking = 11,
};

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_TYPE_H_
