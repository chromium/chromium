// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/follow_features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kEnableFollowIPHExpParams,
             "EnableFollowIPHExpParams",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kDailyVisitMin[] = "DailyVisitMin";

const char kNumVisitMin[] = "NumVisitMin";

const char kVisitHistoryDuration[] = "VisitHistoryDuration";

const char kVisitHistoryExclusiveDuration[] = "VisitHistoryExclusiveDuration";

const char kShowFollowIPHAfterLoaded[] = "ShowFollowIPHAfterLoaded";

double GetDailyVisitMin() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFollowIPHExpParams, kDailyVisitMin, 3 /*default to 3 times*/);
}

double GetNumVisitMin() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFollowIPHExpParams, kNumVisitMin, 3 /*default to 3 times*/);
}

base::TimeDelta GetVisitHistoryDuration() {
  double days = base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFollowIPHExpParams, kVisitHistoryDuration,
      28 /*default to 28 days*/);

  return base::Days(days);
}

base::TimeDelta GetVisitHistoryExclusiveDuration() {
  double hours = base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFollowIPHExpParams, kVisitHistoryExclusiveDuration,
      1 /*default to 1 hour*/);

  return base::Hours(hours);
}

base::TimeDelta GetShowFollowIPHAfterLoaded() {
  double seconds = base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFollowIPHExpParams, kShowFollowIPHAfterLoaded,
      3 /*default to 3 seconds*/);

  return base::Seconds(seconds);
}
