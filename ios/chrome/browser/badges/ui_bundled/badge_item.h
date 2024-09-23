// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_ITEM_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/infobars/model/badge_state.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_type.h"

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

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_ITEM_H_
