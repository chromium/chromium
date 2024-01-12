// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_TAB_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_TAB_VIEW_H_

#import <UIKit/UIKit.h>

// The GroupTabView is a UIView that displays combinations of snapshots,
// favicons and a label displaying the remaining unshowed tabs in a group. It is
// used in 3 different configurations, this view can be reconfigured without the
// need to be recreated.
// -Snapshot configuration: in this configuration `GroupTabView` is similar to
// TopAlignedImageView.
// -Favicon configuration: in this configuration `GroupTabView` will display a
// faviconView in its center, the faviconView is a UIImageView with the favicon
// as its image, the faviconView will share the same constraints as the
// GroupTabView itself with a height and width ratios of60%.
// -Tabs num configuration: When the number of tabs in a group exceeds 7,
// GroupTabView will display a UILabel with different configurations (+N when N
// <100 and 99+ when N>99, where N represents the number of remaining tabs).
@interface GroupTabView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Configures the `GroupTabView` with a given snapshot/favicon pair, favicon can
// be nil.
- (void)configureWithSnapshot:(UIImage*)snapshot favicon:(UIImage*)favicon;

// Configures the `GroupTabView` with a given favicon, favicon can be nil.
- (void)configureWithFavicon:(UIImage*)favicon;

// Configures the `GroupTabView` with a greater than 0 `remainingTabsNumber`.
- (void)configureWithRemainingTabsNumber:(NSInteger)remainingTabsNumber;

// Hides all the views/label and clears their contents (image/attributedText).
- (void)hideAllAttributes;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_TAB_VIEW_H_
