// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>
#import <objc/runtime.h>

#import <memory>

#import "base/base_switches.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/strcat.h"
#import "components/metrics/metrics_service.h"
#import "components/prefs/json_pref_store.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/pref_service_factory.h"
#import "components/variations/pref_names.h"
#import "components/variations/service/safe_seed_manager.h"
#import "components/variations/variations_test_utils.h"

#import "ios/chrome/browser/variations/model/variations_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/scoped_allow_crash_on_startup.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {
std::unique_ptr<ScopedAllowCrashOnStartup> gAllowCrashOnStartup;
}  // namespace

@interface VariationsSafeModeEndToEndTestCase : ChromeTestCase
@end

@implementation VariationsSafeModeEndToEndTestCase

#pragma mark - Helpers

// Returns an AppLaunchConfiguration that shuts down Chrome cleanly (if it is
// already running) and relaunches it. Disabling the testing config means that
// the only field trials after the relaunch, if any, are client-side field
// trials.
//
// Change the `allow_crash_on_startup` field of the returned config to afford
// the app an opportunity to crash on restart.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  // Assign the test environment to be on the Canary channel. This ensures
  // compatibility with the crashing study in the seed.
  config.additional_args = {"--disable-field-trial-config",
                            "--fake-variations-channel=canary"};
  return config;
}

// Returns an AppLaunchConfiguration that shuts down Chrome cleanly (if it is
// already running) and relaunches it with no additional flags or settings.
- (AppLaunchConfiguration)appConfigurationForCleanRestart {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

// Checks that the variations crash streak is `value`.
- (void)checkCrashStreakValue:(int)value {
  int actualStreak = [VariationsAppInterface crashStreak];
  GREYAssertEqual(actualStreak, value,
                  @"Expected a crash streak of %d, but got %d", value,
                  actualStreak);
}

// Restarts the app and ensures there's no variations/crash state active.
- (void)resetAppState:(AppLaunchConfiguration)config {
  // Clear local state variations prefs since local state is persisted between
  // EG tests and restart Chrome. This is to avoid flakiness caused by tests
  // that may have run previously and to avoid introducing flakiness in tests
  // that might run after.
  //
  // See crbug.com/1069086.
  [VariationsAppInterface clearVariationsPrefs];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Validate app state:
  //   * App is running
  //   * No safe seed value in local state
  //   * No evidence of safe seed settings in local state.
  //   * No active crash streak
  XCTAssertTrue([[AppLaunchManager sharedManager] appIsLaunched],
                @"App should be launched.");
  GREYAssertFalse([VariationsAppInterface hasSafeSeed], @"No safe seed.");
  GREYAssertFalse([VariationsAppInterface fieldTrialExistsForTestSeed],
                  @"No field trial from test seed.");
  [self checkCrashStreakValue:0];
}

#pragma mark - Lifecycle

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  gAllowCrashOnStartup = std::make_unique<ScopedAllowCrashOnStartup>();
}

+ (void)tearDown {
  gAllowCrashOnStartup.reset();
  [super tearDown];
}

- (void)setUp {
  // `ChromeTestCase:isStartupTest` must be true before calling [super setUp] in
  // order to avoid opening a new tab on startup. While not strictly necessary,
  // this let's the test run a little faster.
  [[self class] testForStartup];

  [super setUp];
  [self resetAppState:[self appConfigurationForTestCase]];
  self.continueAfterFailure = YES;
}

- (void)tearDown {
  [self resetAppState:[self appConfigurationForCleanRestart]];
  [super tearDown];
}

#pragma mark - Tests

// Tests that three seed-driven crashes trigger variations safe mode.
//
// Corresponds to VariationsSafeModeEndToEndBrowserTest.ExtendedSafeSeedEndToEnd
// in variations_safe_mode_browsertest.cc.
- (void)testVariationsSafeModeEndToEnd {
// TODO(crbug.com/40215027): Test fails on iOS 17.5+ iPad devices.
#if !TARGET_IPHONE_SIMULATOR
  if (@available(iOS 17.5, *)) {
    if ([ChromeEarlGrey isIPadIdiom]) {
      EARL_GREY_TEST_DISABLED(@"This test fails on iOS 17.5+ iPad device.");
    }
  }
#endif
  AppLaunchConfiguration config = [self appConfigurationForTestCase];

  // Set the safe seed value. Validate that the seed is set but not active.
  [VariationsAppInterface setTestSafeSeedAndSignature];
  GREYAssertTrue([VariationsAppInterface hasSafeSeed],
                 @"The variations safe seed pref should be set.");
  GREYAssertFalse([VariationsAppInterface fieldTrialExistsForTestSeed],
                  @"Safe seed field trials should not be active.");

  // Set a Finch seed value that enables a crashing feature.
  [VariationsAppInterface setCrashingRegularSeedAndSignature];

  // Pretend chrome has repeatedly crashed, just one away from safe mode.
  int penultimateCrash = variations::kCrashStreakSafeSeedThreshold - 1;
  [VariationsAppInterface setCrashValue:penultimateCrash];
  [self checkCrashStreakValue:penultimateCrash];

  // The next restart should crash, hitting the crash streak threshold.
  NSLog(@"Next start should crash on startup...");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  XCTAssertFalse([[AppLaunchManager sharedManager] appIsLaunched],
                 @"App should have crashed on startup");

  NSLog(@"Next start should enter safe mode...");
  // Subsequent restarts should succeed. Verify that Chrome fell back to
  // variations safe mode by checking that there is a field trial for the test
  // safe seed's study.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  XCTAssertTrue([[AppLaunchManager sharedManager] appIsLaunched],
                @"App should be launched.");
  GREYAssertTrue([VariationsAppInterface hasSafeSeed],
                 @"The variations safe seed pref should be set.");
  GREYAssertTrue([VariationsAppInterface fieldTrialExistsForTestSeed],
                 @"There should be field trials from kTestSeedData.");
  [self checkCrashStreakValue:variations::kCrashStreakSafeSeedThreshold];
}

@end
