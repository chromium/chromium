// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"

#import "ios/chrome/browser/first_run/model/first_run.h"

namespace {

/// FirstRunRecency key, should match the one in `system_flags`.
NSString* kFirstRunRecencyKey = @"FirstRunRecency";

}  // namespace

void ForceFirstRunRecency(NSInteger number_of_days) {
  FirstRun::RemoveSentinel();
  base::File::Error file_error;
  startup_metric_utils::FirstRunSentinelCreationResult sentinel_created =
      FirstRun::CreateSentinel(&file_error);
  CHECK(sentinel_created ==
        startup_metric_utils::FirstRunSentinelCreationResult::kSuccess);
  FirstRun::LoadSentinelInfo();
  FirstRun::ClearStateForTesting();
  FirstRun::IsChromeFirstRun();
  [[NSUserDefaults standardUserDefaults] setInteger:number_of_days
                                             forKey:kFirstRunRecencyKey];
}

void ResetFirstRunSentinel() {
  if (FirstRun::RemoveSentinel()) {
    FirstRun::LoadSentinelInfo();
    FirstRun::ClearStateForTesting();
    FirstRun::IsChromeFirstRun();
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kFirstRunRecencyKey];
  }
}
