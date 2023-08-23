// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/model/app_launching_state.h"

#import "base/test/ios/wait_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using AppLaunchingStateTest = PlatformTest;

// Tests that updateWithLaunchRequest counts the number of consecutive launches
// correctly and also reset when the time between launches is more than the
// predefined max allowed time between consecutive launches.
TEST_F(AppLaunchingStateTest, TestUpdateWithLaunchRequest) {
  AppLaunchingState* state = [[AppLaunchingState alloc] init];
  EXPECT_EQ(kDefaultMaxSecondsBetweenConsecutiveExternalAppLaunches,
            [AppLaunchingState maxSecondsBetweenConsecutiveLaunches]);
  double maxSecondsBetweenLaunches = 0.25;
  [AppLaunchingState
      setMaxSecondsBetweenConsecutiveLaunches:maxSecondsBetweenLaunches];

  EXPECT_EQ(0, state.consecutiveLaunchesCount);
  [state updateWithLaunchRequest];
  EXPECT_EQ(1, state.consecutiveLaunchesCount);
  [state updateWithLaunchRequest];
  EXPECT_EQ(2, state.consecutiveLaunchesCount);
  [state updateWithLaunchRequest];
  EXPECT_EQ(3, state.consecutiveLaunchesCount);
  // Wait for more than `maxSecondsBetweenLaunches`.
  base::test::ios::SpinRunLoopWithMinDelay(
      base::Seconds(maxSecondsBetweenLaunches + 0.1));
  // consecutiveLaunchesCount should reset.
  [state updateWithLaunchRequest];
  EXPECT_EQ(1, state.consecutiveLaunchesCount);
  [state updateWithLaunchRequest];
  EXPECT_EQ(2, state.consecutiveLaunchesCount);
  // reset back to the default value.
  [AppLaunchingState
      setMaxSecondsBetweenConsecutiveLaunches:
          kDefaultMaxSecondsBetweenConsecutiveExternalAppLaunches];
}
