// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_CELL_H_

#import <UIKit/UIKit.h>

@class TabGroupsPanelFaviconGrid;
@class TabGroupsPanelItem;

// Represents a synced tab group in the Tab Groups panel.
@interface TabGroupsPanelCell : UICollectionViewCell

// Subviews to configure.
@property(nonatomic, strong, readonly) TabGroupsPanelFaviconGrid* faviconsGrid;
@property(nonatomic, strong, readonly) UIView* dot;
@property(nonatomic, strong, readonly) UILabel* titleLabel;
@property(nonatomic, strong, readonly) UILabel* subtitleLabel;

// Associated item, identifying the represented tab group.
@property(nonatomic, strong) TabGroupsPanelItem* item;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_CELL_H_
