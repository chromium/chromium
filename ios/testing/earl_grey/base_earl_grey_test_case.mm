// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/base_earl_grey_test_case.h"

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"
#import "ios/testing/earl_grey/coverage_utils.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
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
  // TODO(crbug.com/1316613): app-measurement.com network request started
  // causing EG synchronization timeouts since iOS 15.4 on simulators. Remove
  // when the root cause is fixed.
  NSArray<NSString*>* blockedURLs = @[
    @"https://app-measurement.com/.*",
  ];
  [[GREYConfiguration sharedConfiguration]
          setValue:blockedURLs
      forConfigKey:kGREYConfigKeyBlockedURLRegex];
}

// Invoked upon starting each test method in a test case.
// Launches the app under test if necessary.
- (void)setUp {
  [super setUp];

  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];
  [self handleSystemAlertIfVisible];

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
  if ([[AppLaunchManager sharedManager] appIsLaunched]) {
    [CoverageUtils writeClangCoverageProfile];
    [CoverageUtils resetCoverageProfileCounters];
  }
  g_needs_set_up_for_test_case = true;
  [super tearDown];
}

// Handles system alerts if any are present, closing them to unblock the UI.
- (void)handleSystemAlertIfVisible {
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

    @try {
      // TODO(crbug.com/1073542): Style guide does not allow throwing
      // exceptions. This call throws an NSInternalInconsistencyException when
      // the system alert is unknown in EG2 framework. The exception will be
      // handled in @catch. Otherwise the system alert is of a known type,
      // accept it.
      [self grey_systemAlertType];

      DLOG(WARNING) << "Accepting iOS system alert: "
                    << base::SysNSStringToUTF8(alertText);

      NSError* acceptAlertError = nil;
      [self grey_acceptSystemDialogWithError:&acceptAlertError];
      GREYAssertNil(acceptAlertError, @"Error accepting system alert.\n%@",
                    acceptAlertError);

    } @catch (NSException* exception) {
      GREYAssert(
          (exception.name == NSInternalInconsistencyException &&
           [exception.reason rangeOfString:@"Invalid System Alert."].location !=
               NSNotFound),
          @"Unknown error caught when handling unknown system alert: %@",
          exception.reason);
      // If the unsupported alert is iOS upgrade or carrier settings alert,
      // handle it. Otherwise, fail the test.
      if ([alertText isEqualToString:@"Software Update"]) {
        DLOG(WARNING) << "Denying iOS system alert of Software Update!";
        // Software Update alert usually has two consecutive alerts, handle them
        // one by one.
        NSError* error = nil;
        // Choose "Later" for the first alert.
        [self tapAlertButtonWithText:@"Later" error:&error];
        // If an error with code |GREYSystemAlertCustomButtonNotFound| happens,
        // probably the second alert is already there. Try to handle it in
        // following steps.
        GREYAssert(
            error == nil || error.code == GREYSystemAlertCustomButtonNotFound,
            @"Error denying first Software Update alert.\n%@", error);
        // A second alert promoting to update tonight will appear. Wait for it.
        [self grey_waitForAlertVisibility:YES
                              withTimeout:kSystemAlertVisibilityTimeout];
        error = nil;
        // Choose "Remind Me Later" for the second alert.
        [self tapAlertButtonWithText:@"Remind Me Later" error:&error];
        GREYAssertNil(error, @"Error denying second Software Update alert.\n%@",
                      error);
      } else if ([alertText
                     containsString:@"A new iOS update is now available."]) {
        DLOG(WARNING)
            << "Denying iOS system alert of new iOS update is now available!";
        // This is another format of Software Update dialog. Need to choose
        // "Close".
        NSError* error = nil;
        [self tapAlertButtonWithText:@"Close" error:&error];
        GREYAssertNil(error, @"Error closing Software Update alert.\n%@",
                      error);
      } else if ([alertText isEqualToString:@"Carrier Settings Update"]) {
        DLOG(WARNING) << "Denying iOS system alert of Carrier Settings Update!";
        NSError* error = nil;
        [self tapAlertButtonWithText:@"Not Now" error:&error];
        GREYAssertNil(
            error, @"Error closing Carrier Settings Update alert.\n%@", error);
      } else if ([alertText containsString:@"would like to find and connect to "
                                           @"devices on your local network."]) {
        DLOG(WARNING) << "Denying iOS system alert of connecting to local "
                         "network devices!";
        NSError* error = nil;
        [self tapAlertButtonWithText:@"OK" error:&error];
        GREYAssertNil(error,
                      @"Error closing connecting to local network devices.\n%@",
                      error);
      } else {
        XCTFail("An unsupported system alert is present on device. Failing "
                "this test. Alert label: %@",
                alertText);
      }
    }
  }
  // Ensures no visible alert after handling.
  [self grey_waitForAlertVisibility:NO
                        withTimeout:kSystemAlertVisibilityTimeout];
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

// Taps button with |text| in the system alert on screen. If an alert or the
// button doesn't exist, note it in |error| accordingly. In EG1, this method is
// no-op.
- (void)tapAlertButtonWithText:(NSString*)text error:(NSError**)error {
  XCUIApplication* springboardApp = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  XCUIElement* alert = [[springboardApp
      descendantsMatchingType:XCUIElementTypeAlert] firstMatch];
  if (![alert waitForExistenceWithTimeout:kSystemAlertVisibilityTimeout]) {
    *error = [NSError errorWithDomain:kGREYSystemAlertDismissalErrorDomain
                                 code:GREYSystemAlertNotPresent
                             userInfo:nil];
    return;
  }
  XCUIElement* button = alert.buttons[text];
  if (![alert.buttons[text] exists]) {
    *error = [NSError errorWithDomain:kGREYSystemAlertDismissalErrorDomain
                                 code:GREYSystemAlertCustomButtonNotFound
                             userInfo:nil];
    return;
  }

  [button tap];
}

@end
