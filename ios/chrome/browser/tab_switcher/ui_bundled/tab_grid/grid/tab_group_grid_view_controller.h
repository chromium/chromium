// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_TAB_GROUP_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_TAB_GROUP_GRID_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/ui/base_grid_view_controller+subclassing.h"

@protocol GridViewDelegate;

// A view controller that contains a grid of tabs from the same group.
@interface TabGroupGridViewController : BaseGridViewController

// Group's title.
@property(nonatomic, copy) NSString* groupTitle;

// Group's color.
@property(nonatomic, copy) UIColor* groupColor;

// Whether this tab group is shared with other users.
@property(nonatomic, assign) BOOL shared;

// View delegate is informed of user interactions in the grid UI.
@property(nonatomic, weak) id<GridViewDelegate> viewDelegate;

// The text in the activity summary cell.
@property(nonatomic, copy) NSString* activitySummaryCellText;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_TAB_GROUP_GRID_VIEW_CONTROLLER_H_
