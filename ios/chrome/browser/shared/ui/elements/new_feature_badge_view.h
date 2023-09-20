// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_NEW_FEATURE_BADGE_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_NEW_FEATURE_BADGE_VIEW_H_

#import <UIKit/UIKit.h>

// View composed of a blue seal icon and a white single-letter label centered on
// top. Used as a badge to indicate a new feature. There is no padding. The size
// of the view is the same as the size of the icon.
@interface NewFeatureBadgeView : UIView

// Designated initializer to create a NewFeatureBadgeView. `badgeSize` is the
// size (width and height) of the badge. `fontSize` is the font size of the
// centered label.
- (instancetype)initWithBadgeSize:(CGFloat)badgeSize
                         fontSize:(CGFloat)fontSize NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Sets the color of the badge. When the NewFeatureBadgeView is initialized, the
// badge is given the Blue600 color by default.
- (void)setBadgeColor:(UIColor*)color;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_NEW_FEATURE_BADGE_VIEW_H_
