// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/ui/docking_promo_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/docking_promo/model/utils.h"

namespace {

// Records the action taken by a user on the Docking Promo.
void RecordAction(IOSDockingPromoAction action) {
  base::UmaHistogramEnumeration("IOS.DockingPromo.Action", action);
}

// Records the action taken on the Docking Promo specifically for users
// identified as having low engagement.
void RecordActionLowEngagement(IOSDockingPromoAction action) {
  base::UmaHistogramEnumeration("IOS.DockingPromo.Action.LowEngagement",
                                action);
}

// Records the action taken on the Docking Promo specifically for users who
// have not recently launched the app via the home screen icon.
void RecordActionNoRecentIconLaunches(IOSDockingPromoAction action) {
  base::UmaHistogramEnumeration("IOS.DockingPromo.Action.NoRecentIconLaunches",
                                action);
}

// Records the action taken on the Docking Promo for the specific segment of
// users who both have low engagement and no recent icon launches.
void RecordActionLowEngagementWithNoRecentIconLaunches(
    IOSDockingPromoAction action) {
  base::UmaHistogramEnumeration(
      "IOS.DockingPromo.Action.LowEngagementWithNoRecentIconLaunches", action);
}

}  // namespace

void RecordDockingPromoAction(IOSDockingPromoAction action,
                              IOSDockingPromoEligibility eligibility) {
  RecordAction(action);

  switch (eligibility) {
    case IOSDockingPromoEligibility::kIsLowEngagementUser:
      RecordActionLowEngagement(action);
      break;
    case IOSDockingPromoEligibility::kHasNoRecentIconLaunches:
      RecordActionNoRecentIconLaunches(action);
      break;
    case IOSDockingPromoEligibility::kIsLowEngagementWithNoRecentIconLaunches:
      RecordActionLowEngagementWithNoRecentIconLaunches(action);
      RecordActionLowEngagement(action);
      RecordActionNoRecentIconLaunches(action);
      break;
    case IOSDockingPromoEligibility::kIneligible:
      break;
  }
}

void RecordDockingPromoImpression(IOSDockingPromoEligibility eligibility) {
  base::UmaHistogramEnumeration("IOS.DockingPromo.Impression", eligibility);
}
