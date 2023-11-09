// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::StaticTextWithAccessibilityLabelId;

namespace {

// Returns a matcher for the signed-in accounts dialog.
id<GREYMatcher> SignedInAccountsDialogMatcher() {
  return StaticTextWithAccessibilityLabelId(
      IDS_IOS_SIGNED_IN_ACCOUNTS_VIEW_TITLE);
}

// Returns a matcher for the signed-in accounts view.
id<GREYMatcher> SignedInAccountsDialogOkButtonMatcher() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_SIGNED_IN_ACCOUNTS_VIEW_OK_BUTTON);
}

}  // namespace

// Interaction tests for SignedInAccountsViewController.
@interface SignedInAccountsTestCase : ChromeTestCase
@end

@implementation SignedInAccountsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(switches::kRemoveSignedInAccountsDialog);
  return config;
}

// Tests that signed-in accounts view is not shown when user is signed out.
- (void)testSignedInAccountsNotShownWhenSigedOut {
  // Verify that the signed-in accounts view is not shown when signed out.
  [SigninEarlGreyAppInterface clearLastSignedInAccounts];
  [SigninEarlGreyAppInterface presentSignInAccountsViewControllerIfNecessary];
  [[EarlGrey selectElementWithMatcher:SignedInAccountsDialogMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that signed-in accounts view is shown when user is signed out and when
// the list of last signed in accounts is cleared.
- (void)testSignedInAccountsShownWhenLastSignedInAccountsIsCleared {
  // Sign-in with a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Verify that the signed-in accounts view is shown.
  [SigninEarlGreyAppInterface clearLastSignedInAccounts];
  [SigninEarlGreyAppInterface presentSignInAccountsViewControllerIfNecessary];
  [ChromeEarlGrey waitForMatcher:SignedInAccountsDialogMatcher()];

  // Dismiss the signed-in accounts view.
  [[EarlGrey selectElementWithMatcher:SignedInAccountsDialogOkButtonMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SignedInAccountsDialogMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that the app does not crash if the signed-in account is presented
// twice (actually tests that a call to
// `-[SceneController presentSignInAccountsViewControllerIfNecessary]`
// is a no-op when the signed-in account view controller exists already.
//
// Regression test for crbug.com/1467010
- (void)testSignedInAccountsPresentedTwiceDoesNotCrash {
  // Sign-in with a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Verify that the signed-in accounts view is shown.
  [SigninEarlGreyAppInterface clearLastSignedInAccounts];
  [SigninEarlGreyAppInterface presentSignInAccountsViewControllerIfNecessary];
  [ChromeEarlGrey waitForMatcher:SignedInAccountsDialogMatcher()];

  // Verify that attempting to present the signed in account a second time
  // does not crash the app.
  [SigninEarlGreyAppInterface clearLastSignedInAccounts];
  [SigninEarlGreyAppInterface presentSignInAccountsViewControllerIfNecessary];
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests that signing out dismisses the signed-in accounts view.
//
// Regression test for crbug.com/1432369
- (void)testSignOutDismissesSignedInAccounts {
  // Sign-in with a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Verify that the signed-in accounts view is shown.
  [SigninEarlGreyAppInterface clearLastSignedInAccounts];
  [SigninEarlGreyAppInterface presentSignInAccountsViewControllerIfNecessary];
  [ChromeEarlGrey waitForMatcher:SignedInAccountsDialogMatcher()];

  // Verify that signed out dismisses the signed-in accounts view
  [ChromeEarlGreyAppInterface signOutAndClearIdentitiesWithCompletion:nil];
  [[EarlGrey selectElementWithMatcher:SignedInAccountsDialogMatcher()]
      assertWithMatcher:grey_nil()];
}

@end
