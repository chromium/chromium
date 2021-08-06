// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/metrics_app_interface.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// ConsistencyPromoSigninCoordinator EarlGrey tests.
// Note: Since those tests are not using real identities, it is not possible
// to test when the user signs in using the web sign-in consistency dialog.
// This limitation is related to cookies reason. The web sign-in consistency
// dialog waits for the cookies to be set before closing. This doesn't work
// with fake chrome identities.
@interface ConsistencyPromoSigninCoordinatorTestCase : ChromeTestCase
@end

@implementation ConsistencyPromoSigninCoordinatorTestCase

- (void)setUp {
  [super setUp];
  // Resets the number of dismissals for web sign-in.
  [ChromeEarlGrey setIntegerValue:0
                      forUserPref:prefs::kSigninWebSignDismissalCount];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
}

- (void)tearDown {
  [super tearDown];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
}

// Tests that ConsistencyPromoSigninCoordinator shows up, and then skips it.
- (void)testDismissConsistencyPromoSignin {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyAppInterface triggerConsistencyPromoSigninDialog];
  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebSigninSkipButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGreyUI verifyWebSigninIsVisible:NO];
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@(kSigninAccountConsistencyPromoActionShownCount)];
  GREYAssertNil(error, @"Failed to record show count histogram");
}

// Tests that ConsistencyPromoSigninCoordinator is not shown after the last
// dismissal (based on kDefaultWebSignInDismissalCount value).
- (void)testDismissalCount {
  // Setup.
  GREYAssertTrue(kDefaultWebSignInDismissalCount > 0,
                 @"The default dismissal max value should be more than 0");
  [ChromeEarlGrey setIntegerValue:kDefaultWebSignInDismissalCount - 1
                      forUserPref:prefs::kSigninWebSignDismissalCount];
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Show the web sign-in consistency dialog for the last time.
  [SigninEarlGreyAppInterface triggerConsistencyPromoSigninDialog];
  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebSigninSkipButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGreyUI verifyWebSigninIsVisible:NO];
  GREYAssertEqual(
      kDefaultWebSignInDismissalCount,
      [ChromeEarlGrey userIntegerPref:prefs::kSigninWebSignDismissalCount],
      @"Dismissal count should be increased to the max value");
  // Asks for the web sign-in consistency that should not succeed.
  [SigninEarlGreyAppInterface triggerConsistencyPromoSigninDialog];
  [SigninEarlGreyUI verifyWebSigninIsVisible:NO];
  GREYAssertEqual(
      kDefaultWebSignInDismissalCount,
      [ChromeEarlGrey userIntegerPref:prefs::kSigninWebSignDismissalCount],
      @"Dismissal count should be at the max value");
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@(kSigninAccountConsistencyPromoActionShownCount)];
  GREYAssertNil(error, @"Failed to record show count histogram");
}

@end
