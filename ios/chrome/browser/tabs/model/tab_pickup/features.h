// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_PICKUP_FEATURES_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_PICKUP_FEATURES_H_

#import <Foundation/Foundation.h>

#import "base/feature_list.h"

namespace base {
class TimeDelta;
}  // namespace base

// Feature flag that sets the tab pickup threshold.
BASE_DECLARE_FEATURE(kTabPickupThreshold);

// Flag that adds delay between tab pickup banners.
BASE_DECLARE_FEATURE(kTabPickupMinimumDelay);

// Feature parameters for the tab pickup feature. If no parameter is set, the
// default (10 minutes) threshold will be used.
extern const char kTabPickupThresholdParameterName[];
extern const char kTabPickupThresholdTenMinutesParam[];
extern const char kTabPickupThresholdOneHourParam[];
extern const char kTabPickupThresholdTwoHoursParam[];
extern const char kTabPickupNoFaviconParam[];

// Convenience method for determining if the tab pickup feature is available.
bool IsTabPickupEnabled();

// Convenience method for determining if the tab pickup minimum delay flag is
// enabled.
bool IsTabPickupMinimumDelayEnabled();

// Convenience method for determining if the tab pickup feature has been
// disabled by the user.
bool IsTabPickupDisabledByUser();

// Convenience method for determining if the tab pickup favicon should be
// displayed.
bool IsTabPickupFaviconAvaible();

// Convenience method for determining the tab pickup threshold.
// The default is 10 minutes.
const base::TimeDelta TabPickupTimeThreshold();

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TAB_PICKUP_FEATURES_H_
