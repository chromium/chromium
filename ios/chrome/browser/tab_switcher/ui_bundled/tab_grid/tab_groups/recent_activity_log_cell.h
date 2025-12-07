// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_CELL_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_CELL_H_

#import <UIKit/UIKit.h>

@class FaviconView;

// Represents a log in the recent activity in a shared tab group.
@interface RecentActivityLogCell : UITableViewCell

// The cell favicon imageView on the trailing edge.
@property(nonatomic, readonly, strong) FaviconView* faviconView;
// The cell title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;
// The cell detail text.
@property(nonatomic, readonly, strong) UILabel* descriptionLabel;

// Unique identifier for the cell.
@property(nonatomic, readonly) NSString* uniqueIdentifier;

// Sets the avatar for the cell
- (void)setAvatar:(UIView*)avatar;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_CELL_H_
