// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_COLOR_PALETTE_TAB_GROUP_COLOR_PALETTE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_COLOR_PALETTE_TAB_GROUP_COLOR_PALETTE_H_

#import <UIKit/UIKit.h>

namespace tab_groups {
enum class TabGroupColorId;
}  // namespace tab_groups

// Defines a color palette for tab groups based on a specific color ID.
@interface TabGroupColorPalette : NSObject

// The tone used for the cells' background.
@property(nonatomic, readonly) UIColor* backgroundColor;

// The tone used for the group tab snapshot's background.
@property(nonatomic, readonly) UIColor* snapshotBackgroundColor;

// The tone used for the empty snapshot bars' background.
@property(nonatomic, readonly) UIColor* barColor;

// The tone used for the dot, the borders, and the NTP button.
// This color remains consistent across light and dark themes.
@property(nonatomic, readonly) UIColor* commonColor;

// The `TabGroupColorId` associated with this palette.
@property(nonatomic, readonly) tab_groups::TabGroupColorId tabGroupColorID;

// Returns the static commonColor for a given color ID without instantiating the
// palette.
+ (UIColor*)commonColor:(tab_groups::TabGroupColorId)tabGroupColorID;

// Returns an array of tones for the gradient background in `TabGroupView`
// and `CreateTabGroupView` based on the color ID.
+ (NSArray<UIColor*>*)gradientBackgroundColors:
    (tab_groups::TabGroupColorId)tabGroupColorID;

// Initializes the palette using a `TabGroupColorId` to retrieve the
// corresponding tones.
- (instancetype)initWithColorId:(tab_groups::TabGroupColorId)tabGroupColorID
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_COLOR_PALETTE_TAB_GROUP_COLOR_PALETTE_H_
