// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/policy/core/common/policy_loader_ios_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager_constants.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SignOutAccountsButton;
using chrome_test_util::GoogleSyncSettingsButton;
using chrome_test_util::PrimarySignInButton;

namespace {

// Identifier for the main scroll view covering all the screen content.
NSString* const kScrollViewIdentifier =
    @"kPromoStyleScrollViewAccessibilityIdentifier";

// Returns a matcher for the sign-in screen "Continue as <identity>" button.
id<GREYMatcher> GetContinueButtonWithIdentityMatcher(
    FakeChromeIdentity* fakeIdentity) {
  NSString* buttonTitle = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));

  return grey_allOf(grey_accessibilityLabel(buttonTitle),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the whole forced sign-in screen.
id<GREYMatcher> GetForcedSigninScreenMatcher() {
  return grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
}

// Checks that the forced sign-in prompt is fully dismissed by making sure
// that there isn't any forced sign-in screen displayed.
void VerifyForcedSigninFullyDismissed() {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSyncScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Scrolls down to |elementMatcher| in the scrollable content of the first run
// screen.
void ScrollToElementAndAssertVisibility(id<GREYMatcher> elementMatcher) {
  id<GREYMatcher> scrollView = grey_accessibilityID(kScrollViewIdentifier);

  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(elementMatcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:scrollView] assertWithMatcher:grey_notNil()];
}

// Signs in the browser from the forced sign-in screen.
void SigninBrowserFromForcedSigninScreen(FakeChromeIdentity* fakeIdentity) {
  // Scroll to the "Continue as ..." button to go to the bottom of the screen.
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));

  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];
}

// Opens the sign-out actions sheets from the account settings.
void OpenAccountSignOutActionsSheets() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];
}

// Signs out from the sign-out actions sheets UI. Will handle the data action
// sheet if |syncEnabled|.
void SignOutFromActionSheets(BOOL syncEnabled) {
  id<GREYMatcher> confirmationButtonMatcher = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(confirmationButtonMatcher,
                                          grey_not(SignOutAccountsButton()),
                                          nil)] performAction:grey_tap()];

  if (syncEnabled) {
    confirmationButtonMatcher = [ChromeMatchersAppInterface
        buttonWithAccessibilityLabelID:
            IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(confirmationButtonMatcher,
                                            grey_not(SignOutAccountsButton()),
                                            nil)] performAction:grey_tap()];
  }
}

// Opens account settings and signs out from them.
void OpenAccountSettingsAndSignOut(BOOL syncEnabled) {
  OpenAccountSignOutActionsSheets();
  SignOutFromActionSheets(syncEnabled);
}

}  // namespace

// Test the forced sign-in screens.
@interface ForcedSigninTestCase : ChromeTestCase

@end

@implementation ForcedSigninTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  // Disable the kOldSyncStringFRE feature to avoid having the strings on the
  // sync screen changing.
  config.features_disabled = std::vector<base::Feature>{kOldSyncStringFRE};

  // Configure the policy to force sign-in.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(
      "<dict><key>BrowserSignin</key><integer>2</integer></dict>");

  return config;
}

- (void)tearDown {
  // Sign out then wait for the sign-in screen to reappear if not already
  // displayed. This is to avoid a conflict between the dismiss animation and
  // the presentation animation of the sign-in screen UI which can be triggered
  // simultaneously when tearing down the test case. The sign-in UI may be
  // triggered again when tearing down because the browser is signed out. Making
  // sure that sign-out is done and that the sign-in screen animation is done
  // before tearing down avoids the conflict.
  [ChromeEarlGreyAppInterface signOutAndClearIdentities];
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];

  [super tearDown];
}

#pragma mark - Tests

// Tests the sign-in screen with accounts that are already available.
- (void)testSignInScreenWithAccount {
  // Add an identity to sign-in to enable the "Continue as ..." button in the
  // sign-in screen.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Validate the Title text of the forced sign-in screen.
  id<GREYMatcher> title =
      grey_text(l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE));
  ScrollToElementAndAssertVisibility(title);

  // Validate the Subtitle text of the forced sign-in screen.
  id<GREYMatcher> subtitle = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_MANAGED));
  ScrollToElementAndAssertVisibility(subtitle);

  // Scroll to the "Continue as ..." button to go to the bottom of the screen.
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));

  // Check that there isn't the button to skip sign-in.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN))]
      assertWithMatcher:grey_nil()];

  // Touch the continue button to go to the next screen.
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];

  VerifyForcedSigninFullyDismissed();
}

// Tests the sign-in screen without accounts where an account has to be added
// before signing in.
- (void)testSignInScreenWithoutAccount {
  // Tap on the "Sign in" button.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION))]
      performAction:grey_tap()];

  // Check for the fake SSO screen.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAddAccountViewIdentifier)];
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(@"Cancel"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [ChromeEarlGreyUI waitForAppToIdle];

  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Check that the title of the primary button updates for |fakeIdentity|.
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the "Sign in" button isn't there anymore.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION))]
      assertWithMatcher:grey_nil()];

  // Check that there isn't the button to skip sign-in.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN))]
      assertWithMatcher:grey_nil()];
}

// Tests that accounts can be switched and that there is the button add a new
// account.
- (void)testSignInScreenSwitchAccount {
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGrey fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Tap on the account switcher.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];

  // Check that |fakeIdentity2| is displayed.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that 'Add Account' is displayed.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select |fakeIdentity2|.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];

  // Check that the title of the primary button updates for |fakeIdentity2|.
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity2)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the sign-out action sheet has the right UI.
- (void)testSignOutActionSheetUI {
  // Add account.
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  SigninBrowserFromForcedSigninScreen(fakeIdentity1);

  // Make sure the forced sign-in screen isn't shown.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];

  // Open the SignOut menu actions sheets.
  OpenAccountSignOutActionsSheets();

  // Check the action sheet message and title that are exclusive to forced
  // sign-in.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ENTERPRISE_FORCED_SIGNIN_SIGNOUT_DIALOG_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests signing out account from settings with sync disabled.
- (void)testSignOutFromAccountSettingSyncDisabled {
  // Add account.
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  SigninBrowserFromForcedSigninScreen(fakeIdentity1);

  // Make sure the forced sign-in screen isn't shown.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];

  // Sign out account from account settings.
  OpenAccountSettingsAndSignOut(NO);

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests signing out account from settings with sync enabled.
- (void)testSignOutFromAccountSettingSyncEnable {
  // Add account.
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  SigninBrowserFromForcedSigninScreen(fakeIdentity1);

  // Enable sync.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  const NSTimeInterval kSyncOperationTimeout = 5.0;
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];

  OpenAccountSettingsAndSignOut(YES);

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Test cancelling sign out from account settings.
- (void)testSignOutFromAccountSettingCancel {
  // Add account.
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  SigninBrowserFromForcedSigninScreen(fakeIdentity1);

  // Sign in and enable sync.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];

  OpenAccountSignOutActionsSheets();

  // Note that the iPad does not provide a CANCEL button by design. Click
  // anywhere on the screen to exit.
  [[[EarlGrey
      selectElementWithMatcher:grey_anyOf(chrome_test_util::CancelButton(),
                                          SignOutAccountsButton(), nil)]
      atIndex:1] performAction:grey_tap()];

  // Verify that the force sign-in screen isn't triggered when cancelling
  // sign-out.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests signing out from sync settings.
- (void)testSignOutFromSyncSettings {
  // Add account.
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in.
  SigninBrowserFromForcedSigninScreen(fakeIdentity1);

  // Enable sync.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  const NSTimeInterval kSyncOperationTimeout = 5.0;
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleSyncSettingsButton()];
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC))]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  SignOutFromActionSheets(YES);

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests turning on sync for an account different from the one that is
// currently signed in.
- (void)testSignInWithOneAccountStartSyncWithAnotherAccount {
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGrey fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Tap on the account switcher and select |fakeIdentity1|..
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity1.userEmail)]
      performAction:grey_tap()];

  // Sign in account without enabling sync.
  SigninBrowserFromForcedSigninScreen(fakeIdentity1);

  // Open turn on sync dialog.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  // Select fakeIdentity2.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Check fakeIdentity2 is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];
}

// Tests that the sign-out footer has the right text when the user is signed in
// and not syncing with forced sign-in enabled.
- (void)testSignOutFooterForSignInOnlyUserWithForcedSigninEnabled {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in from forced sign-in prompt.
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];

  // Open account settings and verify the content of the sign-out footer.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityLabel(l10n_util::GetNSString(
                  IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE_WITH_LEARN_MORE)),
              grey_sufficientlyVisible(), nil)] assertWithMatcher:grey_nil()];
}

// Tests that the sign-out footer has the right text when the user is syncing
// and forced sign-in is enabled.
- (void)testSignOutFooterForSignInAndSyncUserWithForcedSigninEnabled {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in from forced sign-in prompt and enable sync for the signed in
  // account.
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];

  // Open account settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify the content of the sign-out footer.
  NSString* footerText = [NSString
      stringWithFormat:
          @"%@\n\n%@",
          l10n_util::GetNSString(
              IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE),
          l10n_util::GetNSString(
              IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE_WITH_LEARN_MORE)];
  footerText = [footerText stringByReplacingOccurrencesOfString:@"BEGIN_LINK"
                                                     withString:@""];
  footerText = [footerText stringByReplacingOccurrencesOfString:@"END_LINK"
                                                     withString:@""];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(footerText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

@end
