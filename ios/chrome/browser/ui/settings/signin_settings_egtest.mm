// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/signin_settings_app_interface.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;

@interface SigninSettingsTestCase : ChromeTestCase
@end

@implementation SigninSettingsTestCase

- (void)tearDown {
  [PolicyAppInterface clearPolicies];

  [super tearDown];
}

// Tests the primary button with no accounts on the device.
- (void)testSignInPromoWithNoAccountsOnDeviceUsingPrimaryButton {
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];

  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

// Tests signing in, using the primary button with one account on the device.
- (void)testSignInPromoWithAccountUsingPrimaryButton {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // User signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_interactable()];
}

// Tests signing in, using the secondary button with one account on the device.
- (void)testSignInPromoWithWarmStateUsingSecondaryButton {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // User signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the sign-in promo should not be shown after been shown 5 times.
- (void)testAutomaticSigninPromoDismiss {
  const int displayedCount = 19;
  [SigninSettingsAppInterface
      setSettingsSigninPromoDisplayedCount:displayedCount];
  [ChromeEarlGreyUI openSettingsMenu];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
  // Check the sign-in promo will not be shown anymore.
  int newDisplayedCount =
      [SigninSettingsAppInterface settingsSigninPromoDisplayedCount];
  GREYAssertEqual(displayedCount + 1, newDisplayedCount,
                  @"Should have incremented the display count");
  // Close the settings menu and open it again.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI openSettingsMenu];
  // Check that the sign-in promo is not visible anymore.
  [SigninEarlGreyUI verifySigninPromoNotVisible];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSettingsSignInCellId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

- (void)testDissmissSigninPromo {
  [ChromeEarlGreyUI openSettingsMenu];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
  // Tap on dismiss button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Check that the sign-in promo is not visible anymore.
  [SigninEarlGreyUI verifySigninPromoNotVisible];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSettingsSignInCellId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// This tests closes the sign-in promo, opens the sign-in view, and adds an
// account.
- (void)testCloseSigninPromoOpenSigninAndAddAccount {
  [ChromeEarlGreyUI openSettingsMenu];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
  // Tap on dismiss button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Open the sign-in dialog.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSettingsSignInCellId),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Add an account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];
}

// Tests the Settings UI when Sync is disabled and the user is not signed in.
- (void)testSyncDisabled {
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  // Dismiss the popup.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                            grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openSettingsMenu];

  // Check that the cell contains a button
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kTableViewCellInfoButtonViewId),
                                          grey_ancestor(grey_accessibilityID(
                                              kSettingsSignInDisabledCellId)),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the cell opens the popup menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsSignInDisabledCellId)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SETTINGS_SIGNIN_DISABLED_POPOVER_TEXT))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the Settings UI when Sync is disabled and the user is signed in.
- (void)testSyncDisabledSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];

  // Disable Sync
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  id<GREYMatcher> syncCell =
      grey_allOf(grey_accessibilityID(kSettingsGoogleSyncAndServicesCellId),
                 grey_sufficientlyVisible(), nil);
  // Check that the cell contains a button
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kTableViewCellInfoButtonViewId),
                                          grey_ancestor(syncCell), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the cell opens the popup menu.
  [[EarlGrey selectElementWithMatcher:syncCell] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SYNC_SETTINGS_DISABLED_POPOVER_TEXT))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
