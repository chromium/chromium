// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBAR_SCROLLING_BACKGROUND_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBAR_SCROLLING_BACKGROUND_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

// A scrolling segmented toolbar background that has a background for each tab
// grid page. Its content offset must be manually synchronized with the tab
// grid's content scroll view to visually highlight the active tab.
@interface TabGridToolbarScrollingBackground : UIScrollView

// Updates the segments according to the appropriate page and scroll state.
- (void)updateBackgroundsForPage:(TabGridPage)page
            scrolledToEdgeHidden:(BOOL)scrolledToEdge
    scrolledBackgroundViewHidden:(BOOL)scrolledBackgroundViewHidden;
// Hides incognito toolbar according to `hidden` by setting the alpha value.
- (void)hideIncognitoToolbarBackground:(BOOL)hidden;
@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBAR_SCROLLING_BACKGROUND_H_
