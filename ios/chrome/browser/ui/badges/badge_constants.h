// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// A11y identifiers so that automation can tap on BadgeButtons
extern NSString* const kBadgeButtonSavePasswordAccessibilityIdentifier;
extern NSString* const kBadgeButtonSavePasswordAcceptedAccessibilityIdentifier;
extern NSString* const kBadgeButtonUpdatePasswordAccessibilityIdentifier;
extern NSString* const
    kBadgeButtonUpdatePasswordAccpetedAccessibilityIdentifier;
extern NSString* const kBadgeButtonIncognitoAccessibilityIdentifier;
extern NSString* const kBadgeButtonOverflowAccessibilityIdentifier;
extern NSString* const kBadgeButtonSaveCardAccessibilityIdentifier;
extern NSString* const kBadgeButtonSaveCardAcceptedAccessibilityIdentifier;
extern NSString* const kBadgeButtonTranslateAccessibilityIdentifier;
extern NSString* const kBadgeButtonTranslateAcceptedAccessibilityIdentifier;

// A11y identifier for the Badge Popup Menu Table View.
extern NSString* const kBadgePopupMenuTableViewAccessibilityIdentifier;

// A11y identifier for the unread indicator above the displayed badge.
extern NSString* const kBadgeUnreadIndicatorAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_CONSTANTS_H_
