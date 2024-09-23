// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// A11y identifiers so that automation can tap on BadgeButtons.
extern NSString* const kBadgeButtonSavePasswordAccessibilityIdentifier;
extern NSString* const kBadgeButtonSavePasswordAcceptedAccessibilityIdentifier;
extern NSString* const kBadgeButtonUpdatePasswordAccessibilityIdentifier;
extern NSString* const
    kBadgeButtonUpdatePasswordAccpetedAccessibilityIdentifier;
extern NSString* const kBadgeButtonIncognitoAccessibilityIdentifier;
extern NSString* const kBadgeButtonOverflowAccessibilityIdentifier;
extern NSString* const kBadgeButtonSaveAddressProfileAccessibilityIdentifier;
extern NSString* const
    kBadgeButtonSaveAddressProfileAcceptedAccessibilityIdentifier;
extern NSString* const kBadgeButtonSaveCardAccessibilityIdentifier;
extern NSString* const kBadgeButtonSaveCardAcceptedAccessibilityIdentifier;
extern NSString* const kBadgeButtonTranslateAccessibilityIdentifier;
extern NSString* const kBadgeButtonTranslateAcceptedAccessibilityIdentifier;
extern NSString* const kBadgeButtonPermissionsCameraAccessibilityIdentifier;
extern NSString* const
    kBadgeButtonPermissionsCameraAcceptedAccessibilityIdentifier;
extern NSString* const kBadgeButtonPermissionsMicrophoneAccessibilityIdentifier;
extern NSString* const
    kBadgeButtonPermissionsMicrophoneAcceptedAccessibilityIdentifier;
extern NSString* const kBadgeButtonParcelTrackingAccessibilityIdentifier;
extern NSString* const
    kBadgeButtonParcelTrackingAcceptedAccessibilityIdentifier;

// A11y identifier for the unread indicator above the displayed badge.
extern NSString* const kBadgeUnreadIndicatorAccessibilityIdentifier;

// Action identifiers for the new overflow menu.
extern NSString* const kBadgeButtonSavePasswordActionIdentifier;
extern NSString* const kBadgeButtonUpdatePasswordActionIdentifier;
extern NSString* const kBadgeButtonSaveAddressProfileActionIdentifier;
extern NSString* const kBadgeButtonSaveCardActionIdentifier;
extern NSString* const kBadgeButtonTranslateActionIdentifier;
extern NSString* const kBadgeButtonPermissionsActionIdentifier;
extern NSString* const kBadgeButtonParcelTrackingActionIdentifier;

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_CONSTANTS_H_
