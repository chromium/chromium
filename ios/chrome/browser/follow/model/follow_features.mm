// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/follow_features.h"

#import "base/metrics/field_trial_params.h"

double GetDailyVisitMin() {
  return 3 /*default to 3 times*/;
}

double GetNumVisitMin() {
  return 3 /*default to 3 times*/;
}

base::TimeDelta GetVisitHistoryDuration() {
  return base::Days(28 /*default to 28 days*/);
}

base::TimeDelta GetVisitHistoryExclusiveDuration() {
  return base::Hours(1 /*default to 1 hour*/);
}

base::TimeDelta GetShowFollowIPHAfterLoaded() {
  return base::Seconds(3 /*default to 3 seconds*/);
}
