// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_H_

#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"

#import "ios/chrome/browser/ui/badges/badge_type.h"

// A button that contains a badge icon image.
@interface BadgeButton : ExtendedTouchTargetButton

// Returns a BadgeButton with it's badgeType set to |badgeType|.
+ (instancetype)badgeButtonWithType:(BadgeType)badgeType;

// The badge type of the button.
@property(nonatomic, assign, readonly) BadgeType badgeType;

// Whether the button is in an accepted state.
@property(nonatomic, assign, readonly) BOOL accepted;

// The image to show when FullScreen is on. This is optional, but should be set
// if a unique image should be shown when FullScreen is on. Otherwise, |image|
// will be used when FullScreen is on.
@property(nonatomic, strong) UIImage* fullScreenImage;

// The default image to show when FullScreen is off. This will be used in both
// normal and disabled button states.
@property(nonatomic, strong) UIImage* image;

// Keeps track of the FullScreen mode of the button.
@property(nonatomic, assign) BOOL fullScreenOn;

// Sets the badge color to the accepted color if |accepted| is YES or the
// default color if |accepted| is NO. Will animate change if |animated| is YES.
- (void)setAccepted:(BOOL)accepted animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_H_
