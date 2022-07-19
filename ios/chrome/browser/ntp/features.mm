// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/features.h"

#import <Foundation/Foundation.h>

#import "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kBlockNewTabPagePendingLoad{
    "BlockNewTabPagePendingLoad", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableWebChannels{"EnableWebChannels",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableFeedBackgroundRefresh{
    "EnableFeedBackgroundRefresh", base::FEATURE_DISABLED_BY_DEFAULT};

const char kEnableServerDrivenBackgroundRefreshSchedule[] =
    "server_driven_schedule";
const char kEnableRecurringBackgroundRefreshSchedule[] =
    "recurring_refresh_schedule";
const char kBackgroundRefreshIntervalInSeconds[] =
    "refresh_interval_in_seconds";

bool IsWebChannelsEnabled() {
  return base::FeatureList::IsEnabled(kEnableWebChannels);
}

bool IsFeedBackgroundRefreshEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedBackgroundRefresh);
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
