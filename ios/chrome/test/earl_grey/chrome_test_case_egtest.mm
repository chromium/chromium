// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/background_launch_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

// Tests for ChromeTestCase lifecycle and AppLaunchManager observer handling.
@interface ChromeTestCaseTestCase : ChromeTestCase
@end

@implementation ChromeTestCaseTestCase

// Tests that relaunching with startup paused at the background stage succeeds
// even after a previous test's -tearDown was interrupted when the host app was
// not running.
- (void)testRelaunchPausedAtBackgroundStageAfterInterruptedTearDown {
  ChromeTestCase* failedTest = [[ChromeTestCase alloc] init];
  [[AppLaunchManager sharedManager]
      addObserver:(id<AppLaunchManagerObserver>)failedTest];
  // XCUIApplication and XCTAssertTrue are used directly because neither
  // EarlGrey nor AppLaunchManager provides a primitive to terminate the host
  // application without immediately relaunching it, and GREYAssert* macros call
  // GREYWaitForAppToIdle() over eDistantObject which fails when the host
  // application is terminated.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  [app terminate];
  XCTAssertTrue([app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  @try {
    [failedTest tearDown];
  } @catch (NSException* exception) {
  }

  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      std::string("--") + test_switches::kPauseStartupAtBackgroundStage);

  // Temporarily remove `self` as an `AppLaunchManager` observer because
  // `ChromeTestCase` runs post-launch hooks that require a foreground scene
  // and attached profile, which do not exist while paused at `Background`.
  [[AppLaunchManager sharedManager]
      removeObserver:(id<AppLaunchManagerObserver>)self];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [[AppLaunchManager sharedManager]
      addObserver:(id<AppLaunchManagerObserver>)self];

  [BackgroundLaunchAppInterface unblockStartupAndForeground];
  [ChromeEarlGrey waitForMainTabCount:1];
}

@end
