// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/features.h"

#import <Foundation/Foundation.h>

#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/system_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kBlockNewTabPagePendingLoad{
    "BlockNewTabPagePendingLoad", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableWebChannels{"EnableWebChannels",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableFeedBackgroundRefresh{
    "EnableFeedBackgroundRefresh", base::FEATURE_DISABLED_BY_DEFAULT};

// Key for NSUserDefaults containing a bool indicating whether the next run
// should enable feed backround refresh. This is used because registering for
// background refreshes must happen early in app initialization and FeatureList
// is not yet available. Changing the `kEnableFeedBackgroundRefresh` feature
// will always take effect after two cold starts after the feature has been
// changed on the server (once for the finch configuration, and another for
// reading the stored value from NSUserDefaults).
NSString* const kEnableFeedBackgroundRefreshForNextColdStart =
    @"EnableFeedBackgroundRefreshForNextColdStart";

const char kEnableFollowingFeedBackgroundRefresh[] =
    "EnableFollowingFeedBackgroundRefresh";

const char kEnableServerDrivenBackgroundRefreshSchedule[] =
    "EnableServerDrivenBackgroundRefreshSchedule";
const char kEnableRecurringBackgroundRefreshSchedule[] =
    "EnableRecurringBackgroundRefreshSchedule";
const char kBackgroundRefreshIntervalInSeconds[] =
    "BackgroundRefreshIntervalInSeconds";
const char kBackgroundRefreshMaxAgeInSeconds[] =
    "BackgroundRefreshMaxAgeInSeconds";

bool IsWebChannelsEnabled() {
  return base::FeatureList::IsEnabled(kEnableWebChannels);
}

bool IsFeedBackgroundRefreshEnabled() {
  static bool feedBackgroundRefreshEnabled =
      [[NSUserDefaults standardUserDefaults]
          boolForKey:kEnableFeedBackgroundRefreshForNextColdStart];
  return feedBackgroundRefreshEnabled;
}

void SaveFeedBackgroundRefreshEnabledForNextColdStart() {
  DCHECK(base::FeatureList::GetInstance());
  [[NSUserDefaults standardUserDefaults]
      setBool:base::FeatureList::IsEnabled(kEnableFeedBackgroundRefresh)
       forKey:kEnableFeedBackgroundRefreshForNextColdStart];
}

bool IsFollowingFeedBackgroundRefreshEnabled() {
  if (experimental_flags::IsForceBackgroundRefreshForFollowingFeedEnabled()) {
    return YES;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedBackgroundRefresh, kEnableFollowingFeedBackgroundRefresh,
      /*default=*/false);
}

bool IsServerDrivenBackgroundRefreshScheduleEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedBackgroundRefresh,
      kEnableServerDrivenBackgroundRefreshSchedule, /*default=*/false);
}

bool IsRecurringBackgroundRefreshScheduleEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedBackgroundRefresh, kEnableRecurringBackgroundRefreshSchedule,
      /*default=*/false);
}

double GetBackgroundRefreshIntervalInSeconds() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFeedBackgroundRefresh, kBackgroundRefreshIntervalInSeconds,
      /*default=*/60 * 60);
}

double GetBackgroundRefreshMaxAgeInSeconds() {
  if (experimental_flags::GetBackgroundRefreshMaxAgeInSeconds() > 0) {
    return experimental_flags::GetBackgroundRefreshMaxAgeInSeconds();
  }
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFeedBackgroundRefresh, kBackgroundRefreshMaxAgeInSeconds,
      /*default=*/0);
}
