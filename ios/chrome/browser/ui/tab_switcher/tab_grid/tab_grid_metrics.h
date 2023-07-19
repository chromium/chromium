// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_METRICS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_METRICS_H_

// Key of the UMA IOS.TabSwitcher.PageChangeInteraction histogram.
extern const char kUMATabSwitcherPageChangeInteractionHistogram[];

// Values of the UMA IOS.TabSwitcher.PageChangeInteraction histogram.
enum class TabSwitcherPageChangeInteraction {
  kNone = 0,
  kScrollDrag = 1,
  kControlTap = 2,
  kControlDrag = 3,
  kItemDrag = 4,
  kMaxValue = kItemDrag,
};

// Key of the UMA IOS.TabGrid.CloseTabs histogram.
extern const char kTabGridCloseMultipleTabsHistogram[];

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_METRICS_H_
