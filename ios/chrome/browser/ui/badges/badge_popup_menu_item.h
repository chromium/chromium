// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_POPUP_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_POPUP_MENU_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

#import "ios/chrome/browser/ui/badges/badge_type.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"

// Item for a badge menu item.
@interface BadgePopupMenuItem : TableViewItem <PopupMenuItem>

- (instancetype)initWithBadgeType:(BadgeType)badgeType
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Associated cell for the BadgePopupMenuItem.
@interface BadgePopupMenuCell : TableViewCell

// Title label for the cell.
@property(nonatomic, strong) UILabel* titleLabel;

// Set the image that will display the badge.
- (void)setBadgeImage:(UIImage*)badgeImage;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_POPUP_MENU_ITEM_H_
