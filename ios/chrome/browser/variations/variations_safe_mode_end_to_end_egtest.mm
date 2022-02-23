// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>
#include <objc/runtime.h>

#include <memory>

#include "base/base_switches.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/variations_test_utils.h"

#import "ios/chrome/browser/variations/variations_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/scoped_allow_crash_on_startup.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
std::unique_ptr<ScopedAllowCrashOnStartup> gAllowCrashOnStartup;
}  // namespace

@interface VariationsSafeModeEndToEndTestCase : ChromeTestCase
@end

@implementation VariationsSafeModeEndToEndTestCase

#pragma mark - Helpers

// Returns an AppLaunchConfiguration that shuts down Chrome cleanly (if it is
// already running) and relaunches it with the extended safe mode field trial
// enabled. Disabling the testing config means that the only field trials after
// the relaunch, if any, are client-side field trials.
//
// Change the |allow_crash_on_startup| field of the returned config to afford
// the app an opportunity to crash on restart.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args = {
      "--disable-field-trial-config", "--fake-variations-channel=canary",
      base::StrCat({"--", ::switches::kForceFieldTrials,
                    "=*",  // * -> Force active on startup.
                    variations::kExtendedSafeModeTrial, "/",
                    variations::kSignalAndWriteViaFileUtilGroup, "/"})};
  return config;
}

// Returns an AppLaunchConfiguration that shuts down Chrome cleanly (if it is
// already running) and relaunches it with no additional flags or settings.
- (AppLaunchConfiguration)appConfigurationForCleanRestart {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

// Checks that the variations crash streak is |value|.
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
  if (![[AppLaunchManager sharedManager] appIsLaunched]) {
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:config];
  }
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
  // |ChromeTestCase:isStartupTest| must be true before calling [super setUp] in
  // order to avoid opening a new tab on startup.
  [[self class] testForStartup];

  [super setUp];
  [self resetAppState:[self appConfigurationForTestCase]];
}

- (void)tearDown {
  [self resetAppState:[self appConfigurationForCleanRestart]];
  [super tearDown];
}

#pragma mark - Tests

// Tests that three seed-driven crashes trigger variations safe mode.
//
// Corresponds to FieldTrialTest.SafeModeEndToEndTest in
// variations_safe_mode_browsertest.cc.
// Disabled test due to multiple builder failures.
// TODO(crbug.com/1298274): re-enable the test with fix.
- (void)DISABLED_testVariationsSafeModeEndToEnd {
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    // TODO(crbug.com/1297123): Disabled on iPad device
    EARL_GREY_TEST_SKIPPED(@"Test is failing on iPad devices");
  }
#endif
  AppLaunchConfiguration config = [self appConfigurationForTestCase];

  // Set the safe seed value. Then restart and validate that the safe seed is
  // set, but not active.
  [VariationsAppInterface setTestSafeSeedAndSignature];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  XCTAssertTrue([[AppLaunchManager sharedManager] appIsLaunched],
                @"App should be launched.");
  [self checkCrashStreakValue:0];
  GREYAssertTrue([VariationsAppInterface hasSafeSeed],
                 @"The variations safe seed pref should be set.");
  GREYAssertFalse([VariationsAppInterface fieldTrialExistsForTestSeed],
                  @"Safe seed field trials should not be active.");

  // Set a Finch seed value that enables a crashing feature.
  [VariationsAppInterface setCrashingRegularSeedAndSignature];

  // Pretend chrome has repeatedly crashed, just one away from safe mode.
  [VariationsAppInterface setCrashValue:variations::kCrashStreakThreshold - 1];

  // The next restart should crash, hitting the crash streak threshold.
  config.maybe_crash_on_startup = true;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  XCTAssertFalse([[AppLaunchManager sharedManager] appIsLaunched],
                 @"App should have crashed.");

  // Subsequent restarts should succeed. Verify that Chrome fell back to
  // variations safe mode by checking that there is a field trial for the test
  // safe seed's study.
  config.maybe_crash_on_startup = false;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  XCTAssertTrue([[AppLaunchManager sharedManager] appIsLaunched],
                @"App should be launched.");
  GREYAssertTrue([VariationsAppInterface hasSafeSeed],
                 @"The variations safe seed pref should be set.");
  GREYAssertTrue([VariationsAppInterface fieldTrialExistsForTestSeed],
                 @"There should be field trials from |kTestSeedData|.");
  [self checkCrashStreakValue:variations::kCrashStreakThreshold];
}

@end
