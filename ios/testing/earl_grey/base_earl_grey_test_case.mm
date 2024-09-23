// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/base_earl_grey_test_case.h"

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#import "base/feature_list.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"
#import "ios/testing/earl_grey/coverage_utils.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/system_alert_handler.h"

#if DCHECK_IS_ON()
#import "ui/display/screen_base.h"
#endif

namespace {

// If true, +setUpForTestCase will be called from -setUp.  This flag is used to
// ensure that +setUpForTestCase is called exactly once per unique XCTestCase
// and is reset in +tearDown.
bool g_needs_set_up_for_test_case = true;

}  // namespace

@implementation BaseEarlGreyTestCase

+ (void)setUpForTestCase {
}

+ (void)setUp {
  NSArray<NSString*>* blockedURLs = @[
    @".*app-measurement\\.com.*",
    @".*google\\.com.*",
    @".*app-analytics-services\\.com.*",
  ];
  [[GREYConfiguration sharedConfiguration]
          setValue:blockedURLs
      forConfigKey:kGREYConfigKeyBlockedURLRegex];

  // Configuration for not tracking hidden animations. By default, all hidden
  // animations are tracked, and these sometimes cause flake. Set to YES so
  // tracking *should not* happen for hidden animations.
  [[GREYConfiguration sharedConfiguration]
          setValue:@YES
      forConfigKey:kGREYConfigKeyIgnoreHiddenAnimations];
}

// Invoked upon starting each test method in a test case.
// Launches the app under test if necessary.
- (void)setUp {
  [super setUp];

  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];
  [SystemAlertHandler handleSystemAlertIfVisible];

  NSString* logFormat = @"*********************************\nStarting test: %@";
  [BaseEarlGreyTestCaseAppInterface
      logMessage:[NSString stringWithFormat:logFormat, self.name]];

  // Calling XCTFail before the application is launched does not assert
  // properly, so failing upon detection of overriding +setUp is delayed until
  // here. See +setUp below for details on why overriding +setUp causes a
  // failure.
  [self failIfSetUpIsOverridden];

  if (g_needs_set_up_for_test_case) {
    g_needs_set_up_for_test_case = false;
    [[self class] setUpForTestCase];
  }
}

+ (void)tearDown {
#if DCHECK_IS_ON()
  // The same screen object is shared across multiple test runs on IOS build.
  // Make sure that all display observers are removed at the end of each
  // test.
  if (display::Screen::HasScreen()) {
    display::ScreenBase* screen =
        static_cast<display::ScreenBase*>(display::Screen::GetScreen());
    DCHECK(!screen->HasDisplayObservers());
  }
#endif
  g_needs_set_up_for_test_case = true;
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  return AppLaunchConfiguration();
}

#pragma mark - Private

// Prevents tests inheriting from this class from putting logic in +setUp.
// +setUp will be called before the application is launched,
// and thus is not suitable for most test case setup. Inheriting tests should
// migrate their +setUp logic to use the equivalent -setUpForTestCase.
- (void)failIfSetUpIsOverridden {
  if ([[BaseEarlGreyTestCase class] methodForSelector:@selector(setUp)] !=
      [[self class] methodForSelector:@selector(setUp)]) {
    XCTFail(@"EG2 test class %@ inheriting from BaseEarlGreyTestCase "
            @"should not override +setUp, as it is called before the "
            @"test application is launched. Please convert your "
            @"+setUp method to +setUpForTestCase.",
            NSStringFromClass([self class]));
  }
}

@end
