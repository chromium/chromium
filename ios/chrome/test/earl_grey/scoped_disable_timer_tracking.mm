// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/scoped_disable_timer_tracking.h"

#import "ios/testing/earl_grey/earl_grey_test.h"

// Helper class to disable EarlGrey's NSTimer tracking.
// TODO(crbug.com/40138424): This is a workaround that should be removed once a
// proper fix lands in EarlGrey.
ScopedDisableTimerTracking::ScopedDisableTimerTracking() {
  original_interval_ =
      GREY_CONFIG_DOUBLE(kGREYConfigKeyNSTimerMaxTrackableInterval);
  [[GREYConfiguration sharedConfiguration]
          setValue:@0
      forConfigKey:kGREYConfigKeyNSTimerMaxTrackableInterval];
}

ScopedDisableTimerTracking::~ScopedDisableTimerTracking() {
  [[GREYConfiguration sharedConfiguration]
          setValue:[NSNumber numberWithDouble:original_interval_]
      forConfigKey:kGREYConfigKeyNSTimerMaxTrackableInterval];
}
