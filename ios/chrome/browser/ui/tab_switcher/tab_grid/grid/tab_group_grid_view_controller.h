// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUP_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUP_GRID_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+subclassing.h"

@protocol GridViewDelegate;

// A view controller that contains a grid of tabs from the same group.
@interface TabGroupGridViewController : BaseGridViewController

// Group's title.
@property(nonatomic, copy) NSString* groupTitle;
// Group's color.
@property(nonatomic, copy) UIColor* groupColor;

// View delegate is informed of user interactions in the grid UI.
@property(nonatomic, weak) id<GridViewDelegate> viewDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUP_GRID_VIEW_CONTROLLER_H_
