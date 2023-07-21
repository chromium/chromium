// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"

#import "base/metrics/histogram_functions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Key of the UMA IOS.TabGrid.CloseTabs histogram.
const char kTabGridCloseMultipleTabsHistogram[] = "IOS.TabGrid.CloseTabs";
}  // namespace

// Key of the UMA IOS.TabSwitcher.PageChangeInteraction histogram.
const char kUMATabSwitcherPageChangeInteractionHistogram[] =
    "IOS.TabSwitcher.PageChangeInteraction";

void RecordTabGridCloseTabsCount(int count) {
  base::UmaHistogramCounts100(kTabGridCloseMultipleTabsHistogram, count);
}
