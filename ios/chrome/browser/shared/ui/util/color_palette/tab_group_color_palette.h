// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_COLOR_PALETTE_TAB_GROUP_COLOR_PALETTE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_COLOR_PALETTE_TAB_GROUP_COLOR_PALETTE_H_

#import <UIKit/UIKit.h>

namespace tab_groups {
enum class TabGroupColorId;
}  // namespace tab_groups

// Defines a color palette for tab groups derived from a seed color.
@interface TabGroupColorPalette : NSObject

// Initializes from a tabGroupColorId to generate a color palette for the grid
// cell and group views.
- (instancetype)initWithColorId:(tab_groups::TabGroupColorId)tabGroupColorId
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The seed color used for generating the palette in the sRGB space.
@property(nonatomic, readonly) UIColor* seedColor;

// A variant of the seed color for the cells' background.
@property(nonatomic, readonly) UIColor* backgroundColor;

// A variant of the seed color for the group tab snapshot's background.
@property(nonatomic, readonly) UIColor* snapshotBackgroundColor;

// A variant of the seed color for the empty snapshot bars' background.
@property(nonatomic, readonly) UIColor* barColor;

// A variant of the seed color for the dot, the borders, and the NTP button
// which is the same in light and dark theme.
@property(nonatomic, readonly) UIColor* commonColor;

+ (UIColor*)commonColor:(tab_groups::TabGroupColorId)tab_group_color_id;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_COLOR_PALETTE_TAB_GROUP_COLOR_PALETTE_H_
