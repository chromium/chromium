// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_FEATURES_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_FEATURES_H_

#import <Foundation/Foundation.h>

#import "base/feature_list.h"

namespace base {
class TimeDelta;
}  // namespace base

class PrefService;

// Preference value when a user manually disable the feature.
extern const int kInactiveTabsDisabledByUser;

// Feature flag that enables the inactive tabs on iPad.
BASE_DECLARE_FEATURE(kInactiveTabsIPadFeature);

// Convenience method for determining if Inactive Tabs is available (it is not
// available on iPad or if not explicitly enabled by Finch).
bool IsInactiveTabsAvailable();

// Convenience method for determining if Inactive Tabs is available and not
// explicitly disabled by the user.
bool IsInactiveTabsEnabled(PrefService* prefs);
bool IsInactiveTabsEnabled(int raw_threshold_value);

// Returns true if a user disabled the feature manually.
bool IsInactiveTabsExplicitlyDisabledByUser(PrefService* prefs);
bool IsInactiveTabsExplicitlyDisabledByUser(int raw_threshold_value);

// Convenience method for determining the tab inactivity threshold.
// The default is 21 days.
const base::TimeDelta InactiveTabsTimeThreshold(PrefService* prefs);

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_FEATURES_H_
