// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_FEATURES_H_

#import "base/feature_list.h"

namespace base {
class TimeDelta;
}  // namespace base

// Feature flag that sets the tab inactivity threshold.
BASE_DECLARE_FEATURE(kTabInactivityThreshold);

// Feature parameters for Inactive Tabs. If no parameter is set, the default
// (two weeks) threshold will be used.
extern const char kTabInactivityThresholdParameterName[];
extern const char kTabInactivityThresholdOneWeekParam[];
extern const char kTabInactivityThresholdTwoWeeksParam[];
extern const char kTabInactivityThresholdThreeWeeksParam[];

// Convenience method for determining if Inactive Tabs is enabled.
bool IsInactiveTabsEnabled();

// Convenience method for determining the tab inactivity threshold.
base::TimeDelta TabInactivityThreshold();

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_FEATURES_H_
