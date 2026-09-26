// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <tuple>

#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/test/earl_grey/background_launch_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Histogram constants.
NSString* const kColdStartFromMain = @"Startup.ColdStartFromMain";
NSString* const kSceneConnection = @"Startup.TimeFromMainToSceneConnection";
NSString* const kLaunchReason = @"Startup.IOSLaunchReason";
NSString* const kLaunchReasonForegrounded =
    @"Startup.IOSLaunchReason.Foregrounded";

}  // namespace

// Test case to verify startup and cold-start metrics across foreground and
// background launch scenarios.
@interface StartupMetricsTestCase : BaseEarlGreyTestCase
@end

@implementation StartupMetricsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Ensure the app is relaunched clean from process start for each test method
  // to isolate test scenarios.
  config.relaunch_policy = ForceRelaunchByKilling;
  config.additional_args.push_back(
      std::string("--") + test_switches::kPauseStartupAtBackgroundStage);
  return config;
}

- (void)setUp {
  [super setUp];
  [self addTeardownBlock:^{
    std::ignore = [MetricsAppInterface releaseHistogramTester];
  }];
}

// Tests that a cold foreground launch records cold start duration metrics and
// records the foreground launch reason.
- (void)testForegroundBaselineLaunch {
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");

  [BackgroundLaunchAppInterface unblockStartupAndForeground];
  [ChromeEarlGrey waitForMainTabCount:1];

  const int foregroundReason = static_cast<int>(IOSLaunchReason::kForeground);
  GREYAssertNil([MetricsAppInterface expectCount:1
                                       forBucket:foregroundReason
                                    forHistogram:kLaunchReason],
                @"Startup.IOSLaunchReason should record kForeground.");

  GREYAssertNil(
      [MetricsAppInterface expectCount:1
                             forBucket:foregroundReason
                          forHistogram:kLaunchReasonForegrounded],
      @"Startup.IOSLaunchReason.Foregrounded should record kForeground.");
  GREYAssertNil([MetricsAppInterface expectTotalCount:1
                                         forHistogram:kColdStartFromMain],
                @"Startup.ColdStartFromMain should be recorded on foreground "
                @"launch.");
  GREYAssertNil([MetricsAppInterface expectTotalCount:1
                                         forHistogram:kSceneConnection],
                @"Startup.TimeFromMainToSceneConnection should be recorded.");

  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
}

// Tests that a simulated background refresh launch omits cold start duration
// metrics and records the background refresh launch reason.
- (void)testBackgroundRefreshWakeup {
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");

  [BackgroundLaunchAppInterface simulateBackgroundRefresh];

  const int backgroundRefreshReason =
      static_cast<int>(IOSLaunchReason::kBackgroundRefresh);
  GREYAssertNil([MetricsAppInterface expectCount:1
                                       forBucket:backgroundRefreshReason
                                    forHistogram:kLaunchReason],
                @"Startup.IOSLaunchReason should record kBackgroundRefresh.");

  [BackgroundLaunchAppInterface unblockStartupAndForeground];
  [ChromeEarlGrey waitForMainTabCount:1];

  GREYAssertNil([MetricsAppInterface expectCount:1
                                       forBucket:backgroundRefreshReason
                                    forHistogram:kLaunchReasonForegrounded],
                @"Startup.IOSLaunchReason.Foregrounded should record "
                @"kBackgroundRefresh.");
  GREYAssertNil([MetricsAppInterface expectTotalCount:0
                                         forHistogram:kColdStartFromMain],
                @"Startup.ColdStartFromMain should be omitted for background "
                @"launch.");

  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
}

// Tests that a simulated background URL session launch omits cold start
// duration metrics and records the background URL session launch reason.
- (void)testBackgroundURLSessionWakeup {
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");

  [BackgroundLaunchAppInterface simulateBackgroundURLSession];

  const int urlSessionReason =
      static_cast<int>(IOSLaunchReason::kBackgroundURLSession);
  GREYAssertNil(
      [MetricsAppInterface expectCount:1
                             forBucket:urlSessionReason
                          forHistogram:kLaunchReason],
      @"Startup.IOSLaunchReason should record kBackgroundURLSession.");

  [BackgroundLaunchAppInterface unblockStartupAndForeground];
  [ChromeEarlGrey waitForMainTabCount:1];

  GREYAssertNil([MetricsAppInterface expectCount:1
                                       forBucket:urlSessionReason
                                    forHistogram:kLaunchReasonForegrounded],
                @"Startup.IOSLaunchReason.Foregrounded should record "
                @"kBackgroundURLSession.");
  GREYAssertNil([MetricsAppInterface expectTotalCount:0
                                         forHistogram:kColdStartFromMain],
                @"Startup.ColdStartFromMain should be omitted for background "
                @"launch.");

  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
}

// Tests that a simulated background silent notification launch omits cold start
// duration metrics and records the background silent notification launch
// reason.
- (void)testBackgroundSilentNotificationWakeup {
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");

  [BackgroundLaunchAppInterface simulateBackgroundSilentNotification];

  const int silentNotificationReason =
      static_cast<int>(IOSLaunchReason::kBackgroundSilentNotification);
  GREYAssertNil(
      [MetricsAppInterface expectCount:1
                             forBucket:silentNotificationReason
                          forHistogram:kLaunchReason],
      @"Startup.IOSLaunchReason should record kBackgroundSilentNotification.");

  [BackgroundLaunchAppInterface unblockStartupAndForeground];
  [ChromeEarlGrey waitForMainTabCount:1];

  GREYAssertNil([MetricsAppInterface expectCount:1
                                       forBucket:silentNotificationReason
                                    forHistogram:kLaunchReasonForegrounded],
                @"Startup.IOSLaunchReason.Foregrounded should record "
                @"kBackgroundSilentNotification.");
  GREYAssertNil([MetricsAppInterface expectTotalCount:0
                                         forHistogram:kColdStartFromMain],
                @"Startup.ColdStartFromMain should be omitted for background "
                @"launch.");

  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
}

@end
