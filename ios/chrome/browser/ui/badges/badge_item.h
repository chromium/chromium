// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/badges/badge_type.h"

// States for the InfobarBadge.
typedef NS_OPTIONS(NSUInteger, BadgeState) {
  // The badge has not been accepted nor has it been read.
  BadgeStateNone = 0,
  // This property is set if it is read (i.e. the menu is opened, if it is set
  // as the displayed badge, or if the user has accepted the badge action).
  // Not set if the user has not seen the badge yet (e.g. the badge is in the
  // overflow menu and the user has yet to open the menu).
  BadgeStateRead = 1 << 0,
  // The badge's banner is currently being presented.
  BadgeStatePresented = 1 << 1,
  // The Infobar Badge is accepted. e.g. The Infobar was accepted/confirmed, and
  // the Infobar action has taken place.
  BadgeStateAccepted = 1 << 2,
};

// Holds properties and values the UI needs to configure a badge button.
@protocol BadgeItem

// The type of the badge.
- (BadgeType)badgeType;
// Whether the badge should be displayed in the fullScreenBadge position. If
// YES, it will be displayed in both FullScreen and non FullScreen.
@property(nonatomic, assign, readonly) BOOL fullScreen;
// Some badges may not be tappable if there is no action associated with it.
@property(nonatomic, assign, readonly, getter=isTappable) BOOL tappable;
// The BadgeState of the badge.
@property(nonatomic, assign) BadgeState badgeState;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_ITEM_H_
