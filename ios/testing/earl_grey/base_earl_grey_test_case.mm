// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/base_earl_grey_test_case.h"

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"
#import "ios/testing/earl_grey/coverage_utils.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(BaseEarlGreyTestCaseAppInterface)
#endif  // defined(CHROME_EARL_GREY_2)

namespace {

// If true, +setUpForTestCase will be called from -setUp.  This flag is used to
// ensure that +setUpForTestCase is called exactly once per unique XCTestCase
// and is reset in +tearDown.
bool g_needs_set_up_for_test_case = true;

}  // namespace

@implementation BaseEarlGreyTestCase

+ (void)setUpForTestCase {
}

// Invoked upon starting each test method in a test case.
// Launches the app under test if necessary.
- (void)setUp {
  [super setUp];

#if defined(CHROME_EARL_GREY_2)
  [self launchAppForTestMethod];
  [self handleSystemAlertIfVisible];

  NSString* logFormat = @"*********************************\nStarting test: %@";
  [BaseEarlGreyTestCaseAppInterface
      logMessage:[NSString stringWithFormat:logFormat, self.name]];

  // Calling XCTFail before the application is launched does not assert
  // properly, so failing upon detection of overriding +setUp is delayed until
  // here. See +setUp below for details on why overriding +setUp causes a
  // failure.
  [self failIfSetUpIsOverridden];
#endif

  if (g_needs_set_up_for_test_case) {
    g_needs_set_up_for_test_case = false;
    [CoverageUtils configureCoverageReportPath];
    [[self class] setUpForTestCase];
  }
}

+ (void)tearDown {
  g_needs_set_up_for_test_case = true;
  [super tearDown];
}

// Handles system alerts if any are present, closing them to unblock the UI.
- (void)handleSystemAlertIfVisible {
#if defined(CHROME_EARL_GREY_2)
  NSError* systemAlertFoundError = nil;
  [[EarlGrey selectElementWithMatcher:grey_systemAlertViewShown()]
      assertWithMatcher:grey_nil()
                  error:&systemAlertFoundError];

  if (systemAlertFoundError) {
    NSError* alertGetTextError = nil;
    NSString* alertText =
        [self grey_systemAlertTextWithError:&alertGetTextError];
    GREYAssertNil(alertGetTextError, @"Error getting alert text.\n%@",
                  alertGetTextError);

    // If the system alert is of a known type, accept it.
    // Otherwise, reject it, as unknown types include alerts which are not
    // desirable to accept, including OS upgrades.
    if ([self grey_systemAlertType] != GREYSystemAlertTypeUnknown) {
      DLOG(WARNING) << "Accepting iOS system alert: "
                    << base::SysNSStringToUTF8(alertText);

      NSError* acceptAlertError = nil;
      [self grey_acceptSystemDialogWithError:&acceptAlertError];
      GREYAssertNil(acceptAlertError, @"Error accepting system alert.\n%@",
                    acceptAlertError);
    } else {
      DLOG(WARNING) << "Denying iOS system alert of unknown type: "
                    << base::SysNSStringToUTF8(alertText);

      NSError* denyAlertError = nil;
      [self grey_denySystemDialogWithError:&denyAlertError];
      GREYAssertNil(denyAlertError, @"Error denying system alert.\n%@",
                    denyAlertError);
    }
  }
#endif  // CHROME_EARL_GREY_2
}

- (void)launchAppForTestMethod {
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithFeaturesEnabled:{}
      disabled:{}
      forceRestart:NO];
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
