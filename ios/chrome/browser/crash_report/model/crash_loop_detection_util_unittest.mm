// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
// The key used to store the count in the implementation.
NSString* const kAppStartupAttemptCountKey = @"AppStartupFailureCount";

typedef PlatformTest CrashLoopDetectionUtilTest;

TEST_F(CrashLoopDetectionUtilTest, FullCycle) {
  crash_util::ResetFailedStartupAttemptCountForTests();

  // Simulate one prior crash.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger:1 forKey:kAppStartupAttemptCountKey];

  EXPECT_EQ(1, crash_util::GetFailedStartupAttemptCount());

  crash_util::IncrementFailedStartupAttemptCount(false);

  // It should still report 1, since it's reporting failures prior to this
  // launch.
  EXPECT_EQ(1, crash_util::GetFailedStartupAttemptCount());
  // ... but under the hood the value should now be 2.
  EXPECT_EQ(2, [defaults integerForKey:kAppStartupAttemptCountKey]);

  // If it's mistakenly incerement again, nothing should change.
  crash_util::IncrementFailedStartupAttemptCount(false);
  EXPECT_EQ(2, [defaults integerForKey:kAppStartupAttemptCountKey]);

  // After a reset it should be 0 internally, but the same via the API.
  crash_util::ResetFailedStartupAttemptCount();
  EXPECT_EQ(1, crash_util::GetFailedStartupAttemptCount());
  EXPECT_EQ(0, [defaults integerForKey:kAppStartupAttemptCountKey]);
}

}  // namespace
