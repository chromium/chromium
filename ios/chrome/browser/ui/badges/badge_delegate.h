// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_DELEGATE_H_

#include "ios/chrome/browser/ui/badges/badge_type.h"

// Protocol to communicate Badge actions to the mediator.
@protocol BadgeDelegate

// Badge types for menu items that should be displayed in the overflow menu.
@property(nonatomic, strong, readonly)
    NSArray<NSNumber*>* badgeTypesForOverflowMenu;

// Action when a Reading List badge is tapped.
- (void)addToReadingListBadgeButtonTapped:(id)sender;

// Action when a Passwords badge is tapped.
- (void)passwordsBadgeButtonTapped:(id)sender;

// Action when a Save Address Profile badge is tapped.
- (void)saveAddressProfileBadgeButtonTapped:(id)sender;

// Action when a Save Card badge is tapped.
- (void)saveCardBadgeButtonTapped:(id)sender;

// Action when a Translate badge is tapped.
- (void)translateBadgeButtonTapped:(id)sender;

// Action when the Permissions badge is tapped.
- (void)permissionsBadgeButtonTapped:(id)sender;

// Action when the overflow badge is tapped.
- (void)overflowBadgeButtonTapped:(id)sender;

// Show the infobar modal for the respective `badgeType` when the new overflow
// menu is tapped.
- (void)showModalForBadgeType:(BadgeType)badgeType;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_DELEGATE_H_
