// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_CELL_H_

#import <UIKit/UIKit.h>

// Represents a log in the recent activity in a shared tab group.
@interface RecentActivityLogCell : UITableViewCell

// The cell user's icon imageView on the left end.
@property(nonatomic, readonly, strong) UIImageView* iconImageView;
// The cell favicon imageView on the right end.
@property(nonatomic, readonly, strong) UIImageView* faviconImageView;
// The cell title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;
// The cell detail text.
@property(nonatomic, readonly, strong) UILabel* descriptionLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_RECENT_ACTIVITY_LOG_CELL_H_
