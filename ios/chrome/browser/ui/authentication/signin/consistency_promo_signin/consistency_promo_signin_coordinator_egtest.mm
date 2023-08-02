// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

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
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector
            (testRemoveLastIdentityWithSigninErrorDialogNoDismiss)]) {
    config.features_enabled.push_back(kConsistencyNewAccountInterface);
  }
  if ([self isRunningTest:@selector
            (testRemoveLastIdentityWithSigninErrorDialogAutomaticDismiss)]) {
    config.features_disabled.push_back(kConsistencyNewAccountInterface);
  }
  if ([self isRunningTest:@selector(testFromSettings)]) {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  return config;
}

// Tests that ConsistencyPromoSigninCoordinator shows up, and then skips it.
- (void)testDismissConsistencyPromoSignin {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL url = self.testServer->GetURL("/echo");
  [SigninEarlGreyAppInterface
      triggerConsistencyPromoSigninDialogWithURL:net::NSURLWithGURL(url)];
  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebSigninSkipButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGreyUI verifyWebSigninIsVisible:NO];
}

// Tests that ConsistencyPromoSigninCoordinator is not shown after the last
// dismissal (based on kDefaultWebSignInDismissalCount value).
- (void)testDismissalCount {
  // Setup.
  GREYAssertTrue(kDefaultWebSignInDismissalCount > 0,
                 @"The default dismissal max value should be more than 0");
  [ChromeEarlGrey setIntegerValue:kDefaultWebSignInDismissalCount - 1
                      forUserPref:prefs::kSigninWebSignDismissalCount];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Show the web sign-in consistency dialog for the last time.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL url = self.testServer->GetURL("/echo");
  [SigninEarlGreyAppInterface
      triggerConsistencyPromoSigninDialogWithURL:net::NSURLWithGURL(url)];
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
  [SigninEarlGreyAppInterface
      triggerConsistencyPromoSigninDialogWithURL:net::NSURLWithGURL(url)];
  [SigninEarlGreyUI verifyWebSigninIsVisible:NO];
  GREYAssertEqual(
      kDefaultWebSignInDismissalCount,
      [ChromeEarlGrey userIntegerPref:prefs::kSigninWebSignDismissalCount],
      @"Dismissal count should be at the max value");
}

// Removes the only identity while the error dialog is opened. Once the identity
// is removed, the web sign-in dialog needs to update itself to show the version
// with no identity.
// kConsistencyNewAccountInterface is enabled.
- (void)testRemoveLastIdentityWithSigninErrorDialogNoDismiss {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL url = self.testServer->GetURL("/echo");
  [SigninEarlGreyAppInterface
      triggerConsistencyPromoSigninDialogWithURL:net::NSURLWithGURL(url)];
  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          WebSigninPrimaryButtonMatcher()]
      performAction:grey_tap()];
  // Wait for the error dialog (sign-in fails since the sign-in is done with a
  // fake identity).
  [ChromeEarlGreyUI waitForAppToIdle];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::StaticTextWithAccessibilityLabelId(
              IDS_IOS_WEBSIGN_ERROR_TITLE)
                                  timeout:base::test::ios::
                                              kWaitForDownloadTimeout];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Dismiss the error dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_SIGN_IN_DISMISS)] performAction:grey_tap()];
  // The web sign-in should be still visible.
  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];
}

// Removes the only identity while the error dialog is opened. Once the identity
// is removed, the web sign-in dialog needs to disappear automatically.
// kConsistencyNewAccountInterface is disabled.
- (void)testRemoveLastIdentityWithSigninErrorDialogAutomaticDismiss {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL url = self.testServer->GetURL("/echo");
  [SigninEarlGreyAppInterface
      triggerConsistencyPromoSigninDialogWithURL:net::NSURLWithGURL(url)];
  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          WebSigninPrimaryButtonMatcher()]
      performAction:grey_tap()];
  // Wait for the error dialog (sign-in fails since the sign-in is done with a
  // fake identity).
  [ChromeEarlGreyUI waitForAppToIdle];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::StaticTextWithAccessibilityLabelId(
              IDS_IOS_WEBSIGN_ERROR_TITLE)
                                  timeout:base::test::ios::
                                              kWaitForDownloadTimeout];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Expect the web sign-in dialog to disappear automatically.
  [SigninEarlGreyUI verifyWebSigninIsVisible:NO];
}

// Display an error dialog and then dismiss the web sign-in dialog.
- (void)testGetErrorDialogAndSkipWebSigninDialog {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL url = self.testServer->GetURL("/echo");
  [SigninEarlGreyAppInterface
      triggerConsistencyPromoSigninDialogWithURL:net::NSURLWithGURL(url)];
  [SigninEarlGreyUI verifyWebSigninIsVisible:YES];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          WebSigninPrimaryButtonMatcher()]
      performAction:grey_tap()];
  // Wait for the error dialog (sign-in fails since the sign-in is done with a
  // fake identity).
  [ChromeEarlGreyUI waitForAppToIdle];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::StaticTextWithAccessibilityLabelId(
              IDS_IOS_WEBSIGN_ERROR_TITLE)
                                  timeout:base::test::ios::
                                              kWaitForDownloadTimeout];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_SIGN_IN_DISMISS)] performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Skip the web sign-in dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebSigninSkipButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGreyUI verifyWebSigninIsVisible:NO];
}

// Tests that the bottom sheet doesn't wait for the cookies when being triggered
// from the settings.
- (void)testFromSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          WebSigninPrimaryButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGreyUI verifyWebSigninIsVisible:NO];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

@end
