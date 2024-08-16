// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The passphrase for the fake sync server.
NSString* const kPassphrase = @"hello";

// The primary identity.
FakeSystemIdentity* kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];

FakeSystemIdentity* kSecondaryIdentity = [FakeSystemIdentity fakeIdentity2];

// Matcher for the account menu.
id<GREYMatcher> accountMenuMatcher() {
  return grey_accessibilityID(kAccountMenuTableViewId);
}

// Matcher for the identity disc.
id<GREYMatcher> identityDiscMatcher() {
  return grey_accessibilityID(kNTPFeedHeaderIdentityDisc);
}

// A matcher for the snackbar message, when the user is signed in with primary
// identity `identity`.
id<GREYMatcher> snackbarMessageMatcher(FakeSystemIdentity* identity) {
  NSString* snackbarMessage =
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
                              base::SysNSStringToUTF16(identity.userGivenName));
  return grey_allOf(grey_text(snackbarMessage), grey_sufficientlyVisible(),
                    nil);
}
}  // namespace

// Integration tests using the Account Menu.
@interface AccountMenuTestCase : WebHttpServerChromeTestCase
@end

@implementation AccountMenuTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kIdentityDiscAccountMenu);

  if ([self isRunningTest:@selector
            (testMultipleIdentities_IdentityConfirmationToast)] ||
      [self isRunningTest:@selector
            (testSingleIdentity_IdentityConfirmationToast)] ||
      [self isRunningTest:@selector
            (testFrequencyLimitation_IdentityConfirmationToast)] ||
      [self isRunningTest:@selector
            (testRecentSignin_IdentityConfirmationToast)]) {
    config.features_enabled.push_back(kIdentityConfirmationSnackbar);
  }

  return config;
}

- (void)setUp {
  [super setUp];
  // Adding the sync passphrase must be done before signin due to limitation of
  // the fakes.
  [ChromeEarlGrey addSyncPassphrase:kPassphrase];
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
}

- (void)tearDown {
  base::TimeDelta marginToAllowIdentityConfirmationSnackbar = base::Days(20);
  [ChromeEarlGrey
      setTimeValue:base::Time::FromDeltaSinceWindowsEpoch(
                       marginToAllowIdentityConfirmationSnackbar)
       forUserPref:prefs::kIdentityConfirmationSnackbarLastPromptTime];
  [ChromeEarlGrey signOutAndClearIdentities];
  [super tearDown];
}

// Update the last sign-in to be long enough in the past that we should display
// the account snackbar.
- (void)updateLastSignInToPastDate {
  base::TimeDelta marginBetweenLastSigninAndIdentityConfirmationPrompt =
      base::Days(20);
  [ChromeEarlGrey
      setTimeValue:base::Time::FromDeltaSinceWindowsEpoch(
                       marginBetweenLastSigninAndIdentityConfirmationPrompt)
       forUserPref:prefs::kLastSigninTimestamp];
}

// Select the identity disc particle.
- (void)selectIdentityDisc {
  [[EarlGrey selectElementWithMatcher:identityDiscMatcher()]
      performAction:grey_tap()];
}

// Select the identity disc particle and verify the account menu is displayed.
- (void)selectIdentityDiscAndVerify {
  [[EarlGrey selectElementWithMatcher:identityDiscMatcher()]
      performAction:grey_tap()];
  // Ensure the Account Menu is displayed.
  [[EarlGrey selectElementWithMatcher:accountMenuMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Asserts that there is no account menu.
- (void)assertAccountMenuIsNotShown {
  [[EarlGrey selectElementWithMatcher:accountMenuMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Assert the snackbar is not shown for kPrimaryIdentity.
- (void)assertSnackbarNotShown {
  [[EarlGrey selectElementWithMatcher:snackbarMessageMatcher(kPrimaryIdentity)]
      assertWithMatcher:grey_nil()];
}

// Assert the snackbar is shown for `identity`.
- (void)assertSnackbarShown:(FakeSystemIdentity*)identity {
  [[EarlGrey selectElementWithMatcher:snackbarMessageMatcher(identity)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Close the account menu.
- (void)closeAccountMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // There is no stop button on ipad.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kAccountMenuCloseButtonId)]
        assertWithMatcher:grey_nil()];
    // Dismiss the menu by tapping on the identity disc particle.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kNTPFeedHeaderIdentityDisc)]
        performAction:grey_tap()];
  } else {
    // Tap on the Close button.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kAccountMenuCloseButtonId)]
        performAction:grey_tap()];
  }
}

#pragma mark - Test open and close

// Tests that the identity disc particle can be selected, and lead to opening
// the account menu.
- (void)testViewAccountMenu {
  // Select the identity disc particle.
  [self selectIdentityDiscAndVerify];
}

// Tests that the close button appears if and only if it’s not an ipad and that
// if it’s present it close the account menu.
- (void)testCloseButtonAccountMenu {
  [self selectIdentityDiscAndVerify];

  [self closeAccountMenu];

  // Verify the Account Menu is dismissed.
  [self assertAccountMenuIsNotShown];
}

// Test that the account menu can’t be opened when the user is signed out.
- (void)testNoAccountMenuWhenSignedOut {
  // Keep the identity but sign-out.
  [SigninEarlGrey signOut];
  [self selectIdentityDisc];
  [self assertAccountMenuIsNotShown];
}

#pragma mark - Test tapping on views

// Test the manage account menu entry opens the manage account view.
- (void)testManageAccount {
  [self selectIdentityDisc];
  // Tap on the Ellipsis button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAccountMenuSecondaryActionMenuButtonId)]
      performAction:grey_tap()];
  // Tap on Manage your account.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_text(l10n_util::GetNSString(
                  IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_GOOGLE_ACCOUNT_ITEM)),
              grey_interactable(), nil)] performAction:grey_tap()];
  // Checks the Fake Account Detail View Controller is shown
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAccountDetailsViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the edit accounts menu entry opens the edit account list view.
- (void)testEditAccountsList {
  [self selectIdentityDisc];
  // Tap on the Ellipsis button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAccountMenuSecondaryActionMenuButtonId)]
      performAction:grey_tap()];
  // Tap on Manage your account.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_text(l10n_util::GetNSString(
                                       IDS_IOS_ACCOUNT_MENU_EDIT_ACCOUNT_LIST)),
                                   grey_interactable(), nil)]
      performAction:grey_tap()];
  // Checks the account settings is shown
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsEditAccountListTableViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the sign out button actually signs out and the account menu view
// is closed.
- (void)testSignOut {
  [self selectIdentityDisc];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSignoutButtonId)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
  [self assertAccountMenuIsNotShown];
}

// Tests that the add account button opens the add account view.
- (void)testAddAccount {
  [self selectIdentityDisc];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuAddAccountButtonId)]
      performAction:grey_tap()];
  // Checks the Fake authentication view is shown
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthActivityViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityID(kFakeAuthCancelButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests the enter passphrase button.
- (void)testAddPassphrase {
  // Encrypt synced data with a passphrase to enable passphrase encryption for
  // the signed in account.
  [self selectIdentityDisc];
  // Check the error button is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuErrorActionButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap on it
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuErrorActionButtonId)]
      performAction:grey_tap()];
  // Verify that the passphrase view was opened.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kSyncEncryptionPassphraseTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Enter the passphrase.
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];
  // Entering the passphrase closes the view.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kSyncEncryptionPassphraseTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  // Check the error button disappeared.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kAccountMenuErrorActionButtonId),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests that tapping on the secondary account button causes the primary account
// to be changed and the account menu view to be closed.
- (void)testSwitch {
  [SigninEarlGrey addFakeIdentity:kSecondaryIdentity];
  [self selectIdentityDisc];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSecondaryAccountButtonId)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:kSecondaryIdentity];
  [self assertAccountMenuIsNotShown];
  [self assertSnackbarShown:kSecondaryIdentity];
}

#pragma mark - Test snackbar

// Verifies identity confirmation snackbar shows on startup with multiple
// identities on device.
- (void)testMultipleIdentities_IdentityConfirmationToast {
  // Add multiple identities and sign in with one of them.
  [SigninEarlGrey addFakeIdentity:kSecondaryIdentity];
  [self updateLastSignInToPastDate];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Confirm the snackbar shows.
  [self assertSnackbarShown:kPrimaryIdentity];
}

// Verifies no identity confirmation snackbar shows on startup with only one
// identity on device.
- (void)testSingleIdentity_IdentityConfirmationToast {
  // Add multiple identities and sign in with one of them.
  [self updateLastSignInToPastDate];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [self assertSnackbarNotShown];
}

// Verifies no identity confirmation snackbar shows on startup when there is an
// identity on the device but the user is signed-out.
- (void)testNoIdentity_IdentityConfirmationToast {
  // Keep the identity but sign-out.
  [SigninEarlGrey signOut];
  [self updateLastSignInToPastDate];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [self assertSnackbarNotShown];
}

// Verifies identity confirmation snackbar shows on startup with multiple
// identities on device with frequency limitations.
- (void)testFrequencyLimitation_IdentityConfirmationToast {
  // Add multiple identities and sign in with one of them.
  [SigninEarlGrey addFakeIdentity:kSecondaryIdentity];
  [self updateLastSignInToPastDate];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Confirm the snackbar shows.
  [self assertSnackbarShown:kPrimaryIdentity];

  // Dismiss the snackabr.
  [[EarlGrey selectElementWithMatcher:snackbarMessageMatcher(kPrimaryIdentity)]
      performAction:grey_tap()];

  // Background then foreground the app again.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [self assertSnackbarNotShown];
}

// Verifies identity confirmation snackbar on startup does not show after a
// recent sign-in.
- (void)testRecentSignin_IdentityConfirmationToast {
  // Add multiple identities and sign in with one of them.
  [SigninEarlGrey addFakeIdentity:kSecondaryIdentity];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [self assertSnackbarNotShown];
}

#pragma mark - Test Error Badge

- (void)testErrorBadge {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the error badge shows on the ADP.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kNTPFeedHeaderIdentityDiscBadge)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [self selectIdentityDiscAndVerify];

  // Check the error button is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuErrorActionButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap on the error action button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuErrorActionButtonId)]
      performAction:grey_tap()];
  // Verify that the passphrase view was opened.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kSyncEncryptionPassphraseTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Enter the passphrase.
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];
  // Entering the passphrase closes the view.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kSyncEncryptionPassphraseTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [self closeAccountMenu];

  [self assertAccountMenuIsNotShown];

  // Verify the error badge on the ADP disappears.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kNTPFeedHeaderIdentityDiscBadge)]
      assertWithMatcher:grey_notVisible()];
}

@end
