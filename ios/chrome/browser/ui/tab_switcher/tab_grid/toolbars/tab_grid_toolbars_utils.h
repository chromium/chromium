// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_UTILS_H_

#import <UIKit/UIKit.h>

// Returns a Toolbar background to be displayed when the content of the TabGrid
// is scrolled to the edge of the toolbar.
UIView* CreateTabGridScrolledToEdgeBackground();

// Returns a Toolbar background to be displayed when the content of the TabGrid
// is scrolled past the edge of the toolbar (content displayed below the
// toolbar).
UIView* CreateTabGridOverContentBackground();

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_UTILS_H_
