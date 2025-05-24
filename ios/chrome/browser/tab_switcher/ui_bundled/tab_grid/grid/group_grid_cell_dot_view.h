// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GROUP_GRID_CELL_DOT_VIEW_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GROUP_GRID_CELL_DOT_VIEW_H_

#import <UIKit/UIKit.h>

// A view that can either represent a colorful dot or the facepile for a group
// in the grid.
@interface GroupGridCellDotView : UIView

// The color of the dot when there is no facepile.
@property(nonatomic, strong) UIColor* color;
// The facepile to be added to the view.
@property(nonatomic, strong) UIView* facePile;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GROUP_GRID_CELL_DOT_VIEW_H_
