// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "build/branding_buildflags.h"
#import "ios/chrome/browser/variations/model/variations_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface VariationsSafeModeTestCase : ChromeTestCase
@end

@implementation VariationsSafeModeTestCase

#pragma mark - Helpers

// Returns an AppLaunchConfiguration that shuts down Chrome cleanly and
// relaunches it without using the field trial testing config. Shutting down
// cleanly flushes local state. Disabling the testing config means that the only
// field trials after the relaunch, if any, are client-side field trials.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args = {"--disable-field-trial-config"};
  return config;
}

// Returns an AppLaunchConfiguration that crashes and relaunches Chrome. The
// config also disables the use of the field trial testing config so that
// VariationsFieldTrialCreator::CreateTrialsFromSeed() executes and determines
// whether to use variations safe mode. See the comment above
// appConfigForPersistingPrefs for more info on disabling the testing config.
- (AppLaunchConfiguration)appConfigurationForCrashing {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByKilling;
  config.additional_args = {"--disable-field-trial-config"};
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

// Checks that the variations failed fetch streak is `value`.
- (void)checkFailedFetchStreakValue:(int)value {
  int actualStreak = [VariationsAppInterface failedFetchStreak];
  GREYAssertEqual(actualStreak, value,
                  @"Expected a failed fetch streak of %d, but got %d", value,
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

- (void)setUp {
  [super setUp];
  [self resetAppState:[self appConfigurationForTestCase]];
}

- (void)tearDown {
  [self resetAppState:[self appConfigurationForCleanRestart]];
  [super tearDown];
}

#pragma mark - Tests

// The tests in this file should roughly correspond to the tests in
// chrome/browser/metrics/variations/variations_safe_mode_browsertest.cc.
// "Roughly" because there is a significant difference between the tests.
//
// The browser tests use a HistogramTester to check if Chrome is running in safe
// mode while the EG tests use the existence of a field trial associated with
// the test safe seed's sole study. HistogramTesters are not an option in EG
// tests because ensureAppLaunchedWithConfiguration() both shuts down and
// relaunches Chrome. It is not possible to initialize a HistogramTester until
// after the startup code under test has executed. Initializing a
// HistogramTester before shutting down Chrome isn't helpful because the
// tester is not persisted across sessions.

// Tests that three crashes trigger variations safe mode.
//
// Corresponds to VariationsSafeModeBrowserTest.ThreeCrashesTriggerSafeMode in
// variations_safe_mode_browsertest.cc.
// TODO(crbug.com/40073772): Test fails on official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_testThreeCrashesTriggerSafeMode \
  DISABLED_testThreeCrashesTriggerSafeMode
#else
#define MAYBE_testThreeCrashesTriggerSafeMode testThreeCrashesTriggerSafeMode
#endif
- (void)MAYBE_testThreeCrashesTriggerSafeMode {
  [VariationsAppInterface setTestSafeSeedAndSignature];

  // Persist the local state pref changes made above and in setUp().
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];

  // Verify that (i) the crash and failed fetch streaks were reset, (ii) the
  // safe seed was persisted, and (iii) there is no field trial associated with
  // the test safe seed's sole study. There should be a field trial associated
  // with the study only after variations safe mode is triggered.
  [self checkCrashStreakValue:0];
  [self checkFailedFetchStreakValue:0];
  GREYAssertTrue([VariationsAppInterface hasSafeSeed],
                 @"The variations safe seed pref should be set.");
  GREYAssertFalse([VariationsAppInterface fieldTrialExistsForTestSeed],
                  @"There should be no field trials from kTestSeedData.");

  // Crash the app three times since a crash streak of three or more triggers
  // variations safe mode. Also, verify the crash streak and the field trial
  // after crashes.
  AppLaunchConfiguration config = [self appConfigurationForCrashing];
  // First crash.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [self checkCrashStreakValue:1];
  GREYAssertFalse([VariationsAppInterface fieldTrialExistsForTestSeed],
                  @"There should be no field trials from kTestSeedData.");
  // Second crash.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [self checkCrashStreakValue:2];
  GREYAssertFalse([VariationsAppInterface fieldTrialExistsForTestSeed],
                  @"There should be no field trials from kTestSeedData.");
  // Third crash.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [self checkCrashStreakValue:3];
  GREYAssertTrue([VariationsAppInterface hasSafeSeed],
                 @"The variations safe seed pref should be set.");
  // Verify that Chrome fell back to variations safe mode by checking that there
  // is a field trial for the test safe seed's study.
  GREYAssertTrue([VariationsAppInterface fieldTrialExistsForTestSeed],
                 @"There should be field trials from kTestSeedData.");
}

// Tests that variations seed fetch failures trigger variations safe mode.
//
// Corresponds to VariationsSafeModeBrowserTest.FetchFailuresTriggerSafeMode in
// variations_safe_mode_browsertest.cc.
- (void)testFetchFailuresTriggerSafeMode {
  [VariationsAppInterface setTestSafeSeedAndSignature];
  // The fetch failure streak threshold for triggering safe mode is 25.
  [VariationsAppInterface setFetchFailureValue:25];

  // Verify that there is no field trial associated with the test safe seed's
  // sole study.
  GREYAssertFalse([VariationsAppInterface fieldTrialExistsForTestSeed],
                  @"There should be no field trials from kTestSeedData.");

  // Persist the local state pref changes made above and in setUp().
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];

  // Verify that (i) the crash streak was reset, (ii) the failed fetch streak
  // and the safe seed were persisted, and (iii) safe mode was triggered.
  [self checkCrashStreakValue:0];
  [self checkFailedFetchStreakValue:25];
  GREYAssertTrue([VariationsAppInterface hasSafeSeed],
                 @"The variations safe seed pref should be set.");
  // Verify that Chrome fell back to variations safe mode by checking that there
  // is a field trial for the test safe seed's study.
  GREYAssertTrue([VariationsAppInterface fieldTrialExistsForTestSeed],
                 @"There should be field trials from kTestSeedData.");
}

// Tests that variations safe mode is not triggered.
//
// Corresponds to VariationsSafeModeBrowserTest.DoNotTriggerSafeMode in
// variations_safe_mode_browsertest.cc.
// TODO(crbug.com/40073772): Test fails on official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_testDoNotTriggerSafeMode DISABLED_testDoNotTriggerSafeMode
#else
#define MAYBE_testDoNotTriggerSafeMode testDoNotTriggerSafeMode
#endif
- (void)MAYBE_testDoNotTriggerSafeMode {
  [VariationsAppInterface setTestSafeSeedAndSignature];
  // Neither a crash streak of 2 nor a fetch failure streak of 24 will trigger
  // variations safe mode in the next session.
  int crashes = 2;
  int fetchFailures = 24;
  [VariationsAppInterface setCrashValue:crashes];
  [VariationsAppInterface setFetchFailureValue:fetchFailures];

  // Verify that there is no field trial associated with the test safe seed's
  // sole study.
  GREYAssertFalse([VariationsAppInterface fieldTrialExistsForTestSeed],
                  @"There should be no field trials from kTestSeedData.");

  // Persist the local state pref changes made above and in setUp().
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];

  // Verify that (i) the crash and failed fetch streaks are as expected, (ii)
  // the safe seed was stored, and (iii) safe mode was not triggered.
  [self checkCrashStreakValue:2];
  [self checkFailedFetchStreakValue:24];
  GREYAssertTrue([VariationsAppInterface hasSafeSeed],
                 @"The variations safe seed pref should be set.");
  // Verify that Chrome did not fall back to variations safe mode by checking
  // that there isn't a field trial for the test safe seed's study.
  GREYAssertFalse([VariationsAppInterface fieldTrialExistsForTestSeed],
                  @"There should be no field trials from kTestSeedData.");
}

@end
