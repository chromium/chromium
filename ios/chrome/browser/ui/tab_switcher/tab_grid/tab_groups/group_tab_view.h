// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_GROUP_TAB_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_GROUP_TAB_VIEW_H_

#import <UIKit/UIKit.h>

// The GroupTabView is a UIView that displays combinations of snapshots,
// favicons and a label displaying the remaining unshowed tabs in a group. It is
// used in 3 different configurations, this view can be reconfigured without the
// need to be recreated.
// - Snapshot configuration: in this configuration, `GroupTabView` is similar to
// TopAlignedImageView. The associated favicon is placed on bottom right on top
// of the snapshot.
// - Favicons configuration: in this configuration, `GroupTabView` will display
// from 1 to 4 "favicon view" disposed on square shape. The "favicon view" is a
// UIImageView with the favicon as its image. The image will be centered. This
// configuration is called only when there is only favicons to display.
// - Favicons with tabs num configuration: When the number of tabs in a group
// exceeds 7, GroupTabView will display the same configuration as the favicons
// one, but the last element is a UILabel with different configurations (+N when
// N<100 and 99+ when N>99, where N represents the number of remaining tabs).
@interface GroupTabView : UIView

- (instancetype)initWithIsCell:(BOOL)isCell NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Configures the `GroupTabView` with a given snapshot/favicon pair, favicon can
// be nil.
- (void)configureWithSnapshot:(UIImage*)snapshot favicon:(UIImage*)favicon;

// Configures the view with the given list of `favicons`.
- (void)configureWithFavicons:(NSArray<UIImage*>*)favicons;

// Configures the view with the given list of favicons and the number of
// elements left. `favicons` must have 3 elements.
- (void)configureWithFavicons:(NSArray<UIImage*>*)favicons
          remainingTabsNumber:(NSInteger)remainingTabsNumber;

// Hides all the views/label and clears their contents (image/attributedText).
- (void)hideAllAttributes;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_GROUP_TAB_VIEW_H_
