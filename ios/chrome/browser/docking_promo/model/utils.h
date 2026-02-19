// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_MODEL_UTILS_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_MODEL_UTILS_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/time/time.h"

@class SceneState;
class ProfileIOS;

// Categorizes users based on their eligibility for the Docking Promo,
// combining engagement levels and recent icon launch behavior.
// LINT.IfChange
enum class IOSDockingPromoEligibility {
  kIneligible = 0,
  kIsLowEngagementUser = 1,
  kHasNoRecentIconLaunches = 2,
  kIsLowEngagementWithNoRecentIconLaunches = 3,
  kMaxValue = kIsLowEngagementWithNoRecentIconLaunches,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml)

// Returns the docking promo eligibility for `profile`. The promo is shown to
// users with low engagement or those who haven't recently launched the app
// from the home screen icon.
IOSDockingPromoEligibility DockingPromoEligibility(ProfileIOS* profile);

// For testing only.
// Returns YES if the Docking Promo is forced for display via Chrome
// Experimental Settings.
BOOL IsDockingPromoForcedForDisplay();

// Returns whether the user eligibility criteria to show the Docking Promo have
// been met, namely:
//
// 1. Chrome is likely not the default browser,
//
// AND
//
// 2. For users no older than 2 days, whether they're active on their first day,
// but not their second day, (and/or) for users no older than 14 days, whether
// they've been inactive for 3 consecutive (or more) days.
BOOL CanShowDockingPromo(base::TimeDelta time_since_last_foreground);

// Returns the minimum time since the last app foregrounding using
// `foregroundScenes`.
std::optional<base::TimeDelta> MinTimeSinceLastForeground(
    NSArray<SceneState*>* foregroundScenes);

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_MODEL_UTILS_H_
