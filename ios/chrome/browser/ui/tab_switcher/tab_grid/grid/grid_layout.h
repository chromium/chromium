// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_LAYOUT_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_LAYOUT_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/flow_layout.h"

// A specialization of FlowLayout that displays items in a grid.
@interface GridLayout : FlowLayout
@end

// A specialization of GridLayout that shows the UI in its "reordering" state,
// with the moving cell enlarged and the non-moving cells transparent.
@interface GridReorderingLayout : GridLayout
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_LAYOUT_H_
