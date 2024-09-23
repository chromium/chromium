// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_EMPTY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

// Protocol defining the interface of the view displayed when the grid is empty.
@protocol GridEmptyView

// Insets of the inner ScrollView.
@property(nonatomic, assign) UIEdgeInsets scrollViewContentInsets;
// Active page of the tab grid. The active page is the page that
// contains the most recent active tab.
@property(nonatomic, assign) TabGridPage activePage;
// The current mode of the empty grid.
@property(nonatomic, assign) TabGridMode tabGridMode;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_EMPTY_VIEW_H_
