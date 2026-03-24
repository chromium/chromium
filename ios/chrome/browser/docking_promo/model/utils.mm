// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/model/utils.h"

#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"

IOSDockingPromoEligibility DockingPromoEligibility(ProfileIOS* profile) {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);

  // If the tracker is unavailable or not initialized, we cannot verify
  // eligibility.
  if (!tracker || !tracker->IsInitialized()) {
    return IOSDockingPromoEligibility::kIneligible;
  }

  // Users with low engagement (Active 1 day or fewer in the last 7 days).
  BOOL isLowEngagementUser = NO;

  // User has launched the app from the icon at least once in the last 7 days.
  BOOL hasRecentIconLaunches = NO;

  for (const auto& [config, count] : tracker->ListEvents(
           feature_engagement::kIPHiOSDockingPromoEligibilityFeature)) {
    if (config.name == feature_engagement::events::kChromeActiveSessionDay) {
      isLowEngagementUser = count <= 1;
    } else if (config.name ==
               feature_engagement::events::kIOSChromeOpenedFromIcon) {
      hasRecentIconLaunches = count > 0;
    }
  }

  if (isLowEngagementUser && !hasRecentIconLaunches) {
    return IOSDockingPromoEligibility::kIsLowEngagementWithNoRecentIconLaunches;
  } else if (isLowEngagementUser) {
    return IOSDockingPromoEligibility::kIsLowEngagementUser;
  } else if (!hasRecentIconLaunches) {
    return IOSDockingPromoEligibility::kHasNoRecentIconLaunches;
  }

  return IOSDockingPromoEligibility::kIneligible;
}
