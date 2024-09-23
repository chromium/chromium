// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_FEATURES_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_FEATURES_H_

#import "base/feature_list.h"

#import <Foundation/Foundation.h>

namespace base {
class TimeDelta;
}  // namespace base

// Preference value when a user manually disable the feature.
extern const int kInactiveTabsDisabledByUser;

// Feature flag that sets the tab inactivity threshold.
BASE_DECLARE_FEATURE(kTabInactivityThreshold);

// Feature parameters for Inactive Tabs. If no parameter is set, the default
// (two weeks) threshold will be used.
extern const char kTabInactivityThresholdParameterName[];
extern const char kTabInactivityThresholdOneWeekParam[];
extern const char kTabInactivityThresholdTwoWeeksParam[];
extern const char kTabInactivityThresholdThreeWeeksParam[];
extern const char kTabInactivityThresholdOneMinuteDemoParam[];
extern const char kTabInactivityThresholdImmediateDemoParam[];

// Convenience method for determining if Inactive Tabs is available (it is not
// available on iPad or if not explicitly enabled by Finch).
bool IsInactiveTabsAvailable();

// Convenience method for determining if Inactive Tabs is available and not
// explicitly disabled by the user.
bool IsInactiveTabsEnabled();

// Convenience method for determining the tab inactivity threshold.
// The default is 14 days.
const base::TimeDelta InactiveTabsTimeThreshold();

// Returns true if a user disabled the feature manually.
bool IsInactiveTabsExplictlyDisabledByUser();

// Feature flag for replacing the Inactive Tabs header by a button.
BASE_DECLARE_FEATURE(kInactiveTabButtonRefactoring);

// Whether the inactive tab entry point should be a button.
bool IsInactiveTabButtonRefactoringEnabled();

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_FEATURES_H_
