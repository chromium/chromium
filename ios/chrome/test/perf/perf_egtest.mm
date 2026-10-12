// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import <string>

#import "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/perf/perf_app_interface.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

@interface PerfTestCase : ChromeTestCase
@end

@implementation PerfTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  // Match production field trial configuration rather than default test
  // behavior.
  config.additional_args.push_back("--enable-field-trial-config");
  // Disable GWP-ASan so random guard page allocations do not skew
  // `Memory.PhysFootprint` measurements across iterations.
  config.additional_args.push_back(
      "--disable-features=GwpAsanMalloc,GwpAsanPartitionAlloc");
  return config;
}

#pragma mark - Tests

// Measures cold startup latency across cold process relaunches using unbucketed
// millisecond timings from StartupInformation.
- (void)testStartup {
  static constexpr int kColdLaunchIterations = 5;
  static constexpr base::TimeDelta kStartupMetricsTimeout = base::Seconds(10);
  NSMutableArray<NSNumber*>* preMainSamples = [[NSMutableArray alloc] init];
  NSMutableArray<NSNumber*>* fromMainSamples = [[NSMutableArray alloc] init];
  NSMutableArray<NSNumber*>* sceneConnectSamples =
      [[NSMutableArray alloc] init];
  NSMutableArray<NSNumber*>* pageInsSamples = [[NSMutableArray alloc] init];

  for (int i = 0; i < kColdLaunchIterations; ++i) {
    AppLaunchConfiguration config = [self appConfigurationForTestCase];
    config.relaunch_policy = ForceRelaunchByKilling;
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:config];

    __block NSDictionary<NSString*, NSNumber*>* timings = nil;
    GREYCondition* waitForColdStartMetric = [GREYCondition
        conditionWithName:@"Wait for ColdStartFromMain metric"
                    block:^BOOL {
                      timings = [PerfAppInterface
                          currentStartupTimingsInMilliseconds];
                      return timings[@"Startup.ColdStartFromMain"] != nil;
                    }];
    GREYAssertTrue(
        [waitForColdStartMetric
            waitWithTimeout:kStartupMetricsTimeout.InSecondsF()],
        @"Timed out waiting for Startup.ColdStartFromMain on iteration %d.", i);

    NSNumber* preMain = timings[@"Startup.ColdStartPreMain"];
    NSNumber* fromMain = timings[@"Startup.ColdStartFromMain"];
    NSNumber* sceneConnect = timings[@"Startup.TimeFromMainToSceneConnection"];
    double pageIns = [PerfAppInterface currentProcessPageIns];
    if (preMain) {
      [preMainSamples addObject:preMain];
    }
    if (fromMain) {
      [fromMainSamples addObject:fromMain];
    }
    if (sceneConnect) {
      [sceneConnectSamples addObject:sceneConnect];
    }
    if (pageIns >= 0.0) {
      [pageInsSamples addObject:@(pageIns)];
    }
  }

  GREYAssertEqual(fromMainSamples.count,
                  static_cast<NSUInteger>(kColdLaunchIterations),
                  @"Expected ColdStartFromMain samples from all iterations.");
  GREYAssertEqual(preMainSamples.count,
                  static_cast<NSUInteger>(kColdLaunchIterations),
                  @"Expected ColdStartPreMain samples from all iterations.");
  GREYAssertEqual(
      sceneConnectSamples.count, static_cast<NSUInteger>(kColdLaunchIterations),
      @"Expected TimeFromMainToSceneConnection samples from all iterations.");
  GREYAssertEqual(pageInsSamples.count,
                  static_cast<NSUInteger>(kColdLaunchIterations),
                  @"Expected ColdStartPageIns samples from all iterations.");
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:fromMainSamples
              forMetric:@"Startup.ColdStartFromMain"
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.startup"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:preMainSamples
              forMetric:@"Startup.ColdStartPreMain"
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.startup"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:sceneConnectSamples
              forMetric:@"Startup.TimeFromMainToSceneConnection"
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.startup"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:pageInsSamples
              forMetric:@"Startup.ColdStartPageIns"
                   unit:@"count_smallerIsBetter"
              benchmark:@"bling.startup"]);
}

// Measures browser kernel physical memory footprint across tab open/close
// cycles using TASK_VM_INFO phys_footprint.
- (void)testOpenCloseTab {
  static constexpr int kOpenCloseIterations = 5;
  static constexpr base::TimeDelta kMemorySettleDelay = base::Seconds(1);
  NSMutableArray<NSNumber*>* footprintSamples = [[NSMutableArray alloc] init];

  for (int i = 0; i < kOpenCloseIterations; ++i) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
        performAction:grey_tap()];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::TabGridOverflowMenuButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                            TabGridOverflowMenuCloseAllButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                       IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABSANDGROUPS)]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
        performAction:grey_tap()];

    // Allow transient UI and autorelease allocations to settle before reading
    // kernel phys_footprint.
    base::test::ios::SpinRunLoopWithMinDelay(kMemorySettleDelay);

    double footprintBytes = [PerfAppInterface currentProcessPhysFootprintBytes];
    GREYAssertTrue(footprintBytes > 0.0,
                   @"Expected positive TASK_VM_INFO phys_footprint.");
    [footprintSamples addObject:@(footprintBytes)];
  }

  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:footprintSamples
              forMetric:@"Memory.PhysFootprint"
                   unit:@"sizeInBytes_smallerIsBetter"
              benchmark:@"bling.memory"]);
}

// Measures Speedometer 3.1 benchmark score served from third_party/speedometer.
- (void)testSpeedometer {
  if (!self.testServer->Started()) {
    self.testServer->ServeFilesFromSourceDirectory(
        "third_party/speedometer/v3.1");
    GREYAssertTrue(self.testServer->Start(),
                   @"EmbeddedTestServer failed to start.");
  }

  const GURL speedometerURL =
      self.testServer->GetURL("/index.html?startAutomatically=true");
  [ChromeEarlGrey loadURL:speedometerURL];

  NSString* const kGetSpeedometerScoreScript =
      @"(function() {"
      @"  var el = document.getElementById('result-number');"
      @"  return el ? el.textContent.trim() : '';"
      @"})()";

  __block std::string scoreString;
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for Speedometer to complete"
                  block:^BOOL {
                    base::Value result = [ChromeEarlGrey
                        evaluateJavaScript:kGetSpeedometerScoreScript];
                    if (result.is_string() && !result.GetString().empty()) {
                      scoreString = result.GetString();
                      return YES;
                    }
                    return NO;
                  }];

  // Delay initial JS polling so benchmark execution isn't perturbed by main
  // thread evaluation during the core run.
  static constexpr base::TimeDelta kInitialDelay = base::Seconds(45);
  static constexpr base::TimeDelta kPollInterval = base::Seconds(15);
  static constexpr base::TimeDelta kTimeout = base::Minutes(10);

  base::test::ios::SpinRunLoopWithMinDelay(kInitialDelay);
  GREYAssertTrue(
      [condition waitWithTimeout:(kTimeout - kInitialDelay).InSecondsF()
                    pollInterval:kPollInterval.InSecondsF()],
      @"Speedometer failed to finish within timeout.");

  double score = 0.0;
  GREYAssertTrue(base::StringToDouble(scoreString, &score) && score > 0.0,
                 @"Speedometer score must be a positive number.");

  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:@[ @(score) ]
              forMetric:@"Speedometer3.TotalScore"
                   unit:@"unitless_biggerIsBetter"
              benchmark:@"bling.speedometer"]);
}

@end
