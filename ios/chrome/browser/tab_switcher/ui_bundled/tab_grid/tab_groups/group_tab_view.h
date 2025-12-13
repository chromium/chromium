// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_GROUP_TAB_VIEW_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_GROUP_TAB_VIEW_H_

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, EmptyThumbnailLayoutType);

// The GroupTabView is a UIView that displays combinations of snapshots,
// favicons, and a label indicating the number of remaining unsent tabs in a
// group. This view can be reused and reconfigured for different display modes
// without needing to be recreated. When reusing an instance,
// `hideAllAttribute:s` must be called to clear its previous content.
//
// - Snapshot configuration: in this configuration, `GroupTabView` is similar to
// TopAlignedImageView. The associated favicon is placed on bottom right on top
// of the snapshot.
//
// - Favicons configuration: in this configuration, `GroupTabView` will display
// from 1 to 4 "favicon view" disposed on square shape. The "favicon view" is a
// UIImageView with the favicon as its image. The image will be centered. This
// configuration is called only when there is only favicons to display.
//
// - Favicons with tabs num configuration: When the number of tabs in a group
// exceeds 7, GroupTabView will display the same configuration as the favicons
// one, but the last element is a UILabel with different configurations (+N when
// N<100 and 99+ when N>99, where N represents the number of remaining tabs).
@interface GroupTabView : UIView

// The current layout configuration that should be used by the empty thumbnail.
@property(nonatomic, assign) EmptyThumbnailLayoutType layoutType;

- (instancetype)initWithIsCell:(BOOL)isCell NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Configures the `GroupTabView` with a given snapshot/favicon pair, favicon can
// be nil.
- (void)configureWithSnapshot:(UIImage*)snapshot favicon:(UIImage*)favicon;

// Configures the view with the given `favicon`.
- (void)configureWithFavicon:(UIImage*)favicon
                faviconIndex:(NSInteger)faviconIndex;

// Configures the view with the given `favicon` and the number of
// elements left.
- (void)configureWithFavicon:(UIImage*)favicon
                faviconIndex:(NSInteger)faviconIndex
         remainingTabsNumber:(NSInteger)remainingTabsNumber;

// Hides all the views/label and clears their contents (image/attributedText).
- (void)hideAllAttributes;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_GROUP_TAB_VIEW_H_
