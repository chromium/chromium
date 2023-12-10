// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_METRICS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_METRICS_H_

// Key of the UMA IOS.TabSwitcher.PageChangeInteraction histogram.
extern const char kUMATabSwitcherPageChangeInteractionHistogram[];

// Values of the UMA IOS.TabSwitcher.PageChangeInteraction histogram.
// LINT.IfChange
enum class TabSwitcherPageChangeInteraction {
  kNone = 0,
  kScrollDrag = 1,
  kControlTap = 2,
  kControlDrag = 3,
  kItemDrag = 4,
  kAccessibilitySwipe = 5,
  kMaxValue = kAccessibilitySwipe,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Records the number of Tabs closed after a bulk or a "Close All" operation.
void RecordTabGridCloseTabsCount(int count);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_METRICS_H_
