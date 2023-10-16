// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_HOME_FEATURES_H_
#define IOS_CHROME_BROWSER_NTP_HOME_FEATURES_H_

#import "base/feature_list.h"

namespace base {
class TimeDelta;
}  // namespace base

// Feature to choose between the old Zine feed or the new Discover feed in the
// Bling new tab page.
// Use IsDiscoverFeedEnabled() instead of this constant directly.
// TODO(crbug.com/1385512): Remove this.
BASE_DECLARE_FEATURE(kDiscoverFeedInNtp);

// Feature to use one NTP for all tabs in a Browser.
BASE_DECLARE_FEATURE(kSingleNtp);

// Feature for the Magic Stack.
BASE_DECLARE_FEATURE(kMagicStack);

// Feature that contains the feed in a module.
BASE_DECLARE_FEATURE(kEnableFeedContainment);

// Feature that enables tab resumption.
BASE_DECLARE_FEATURE(kTabResumption);

// A parameter to indicate whether the Most Visited Tiles should be in the Magic
// Stack.
extern const char kMagicStackMostVisitedModuleParam[];

// A parameter representing how much to reduce the NTP top space margin. If it
// is negative, it will increase the top space margin.
extern const char kReducedSpaceParam[];

// A parameter representing whether modules should not be added to the Magic
// Stack if their content is irrelevant.
extern const char kHideIrrelevantModulesParam[];

// A parameter representing how many days before showing the compacted Set Up
// List module in the Magic Stack.
extern const char kSetUpListCompactedTimeThresholdDays[];

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
// TODO(crbug.com/1385512): Remove this.
extern const char kDiscoverFeedIsNativeUIEnabled[];

// Feature parameters for the tab resumption feature. If no parameter is set,
// the default (most recent tab only) will be used.
extern const char kTabResumptionParameterName[];
extern const char kTabResumptionMostRecentTabOnlyParam[];
extern const char kTabResumptionAllTabsParam[];
extern const char kTabResumptionAllTabsOneDayThresholdParam[];

// Whether the Discover feed is enabled instead of the Zine feed.
// TODO(crbug.com/1385512): Remove this.
bool IsDiscoverFeedEnabled();

// Whether the Magic Stack should be shown.
bool IsMagicStackEnabled();

// Whether the feed is contained in a Home module.
bool IsFeedContainmentEnabled();

// Whether the tab resumption feature is enabled.
bool IsTabResumptionEnabled();

// Whether the tab resumption feature is enabled for most recent tab only.
bool IsTabResumptionEnabledForMostRecentTabOnly();

// Convenience method for determining the tab resumption time threshold for
// X-Devices tabs only.
const base::TimeDelta TabResumptionForXDevicesTimeThreshold();

// Whether the Most Visited Sites should be put into the Magic Stack.
bool ShouldPutMostVisitedSitesInMagicStack();

// How much the NTP top margin should be reduced by for the Magic Stack design.
double ReducedNTPTopMarginSpaceForMagicStack();

// Whether modules should not be added to the Magic Stack if their content is
// irrelevant.
bool ShouldHideIrrelevantModules();

// How many days before showing the Compacted Set Up List module configuration
// in the Magic Stack.
int TimeUntilShowingCompactedSetUpList();

#endif  // IOS_CHROME_BROWSER_NTP_HOME_FEATURES_H_
