// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/test/separate_profiles_util.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/signin/model/test_constants_utils.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The passphrase for the fake sync server.
NSString* const kPassphrase = @"hello";

// The primary identity.
FakeSystemIdentity* const kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];

FakeSystemIdentity* const kSecondaryIdentity =
    [FakeSystemIdentity fakeIdentity2];

FakeSystemIdentity* const kManagedIdentity1 =
    [FakeSystemIdentity fakeManagedIdentity];

FakeSystemIdentity* const kManagedIdentity2 =
    [FakeSystemIdentity identityWithEmail:@"foo2@google.com"];

// Matcher for the account menu.
id<GREYMatcher> accountMenuMatcher() {
  return grey_accessibilityID(kAccountMenuTableViewId);
}

// Matcher for the identity disc.
id<GREYMatcher> identityDiscMatcher() {
  return grey_accessibilityID(kNTPFeedHeaderIdentityDisc);
}

}  // namespace

// Integration tests using the Account Menu.
@interface AccountMenuTestCase : WebHttpServerChromeTestCase
@end

@implementation AccountMenuTestCase

+ (void)setUpForTestCase {
  [SigninEarlGrey setUseFakeResponsesForProfileSeparationPolicyRequests];
}

+ (void)tearDown {
  [SigninEarlGrey clearUseFakeResponsesForProfileSeparationPolicyRequests];
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  // Adding the sync passphrase must be done before signin due to limitation of
  // the fakes.
  [ChromeEarlGrey addSyncPassphrase:kPassphrase];
}

- (void)tearDownHelper {
  [ChromeEarlGrey signOutAndClearIdentities];
  [super tearDownHelper];
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
  ConditionBlock wait_for_disappearance = ^{
    NSError* error;

    // Checking if collection view does not exist in the UI hierarchy.
    [[EarlGrey selectElementWithMatcher:accountMenuMatcher()]
        assertWithMatcher:grey_nil()
                    error:&error];

    return error == nil;
  };

  // The account menu fades with animation; wait for 5 seconds to ensure the
  // animation is completed.
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::Seconds(5), wait_for_disappearance),
             @"Account menu did not disappear.");
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
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  // Select the identity disc particle.
  [self selectIdentityDiscAndVerify];
}

// Tests that the close button appears if and only if it’s not an ipad and that
// if it’s present it close the account menu.
- (void)testCloseButtonAccountMenu {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [self selectIdentityDiscAndVerify];

  [self closeAccountMenu];

  // Verify the Account Menu is dismissed.
  [self assertAccountMenuIsNotShown];
}

// Test that the account menu can’t be opened when the user is signed out.
- (void)testNoAccountMenuWhenSignedOut {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  // Keep the identity but sign-out.
  [SigninEarlGrey signOut];
  [self selectIdentityDisc];
  [self assertAccountMenuIsNotShown];
}

// Tests that the account menu is not dismissed if the app was backgrounded.
// TODO(crbug.com/436894248): Test is flaky on simulator. Reenable the test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testAccountMenuStaysIfAppBackgrounded \
  FLAKY_testAccountMenuStaysIfAppBackgrounded
#else
#define MAYBE_testAccountMenuStaysIfAppBackgrounded \
  testAccountMenuStaysIfAppBackgrounded
#endif
- (void)MAYBE_testAccountMenuStaysIfAppBackgrounded {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  // Select the identity disc particle.
  [self selectIdentityDiscAndVerify];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Ensure the Account Menu is still displayed.
  [[EarlGrey selectElementWithMatcher:accountMenuMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Test tapping on views

// Test the manage account menu entry opens the manage account view.
- (void)testManageAccount {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
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
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
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

  // Checks that "done" close the view.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsAccountsTableViewDoneButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsEditAccountListTableViewId)]
      assertWithMatcher:grey_nil()];
}

// Tests that the sign out button actually signs out and the account menu view
// is closed, from a personal account.
- (void)testSignOut {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [self selectIdentityDisc];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSignoutButtonId)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
  [self assertAccountMenuIsNotShown];
}

// Tests that the sign out button actually signs out and the account menu view
// is closed, from a managed account.
- (void)testSignOutFromManaged {
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:kManagedIdentity1];
  [self selectIdentityDisc];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSignoutButtonId)]
      performAction:grey_tap()];
  // Confirm "Delete and Signout" alert dialog that data will be cleared is
  // shown. This dialog is only shown when multi profiles are not available.
  if (![SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled]) {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                       IDS_IOS_SIGNOUT_AND_DELETE_DIALOG_SIGN_OUT_BUTTON)]
        performAction:grey_tap()];
  }
  [SigninEarlGrey verifySignedOut];
  [self assertAccountMenuIsNotShown];
}

// Tests that the add account button opens the add account view.
- (void)testAddAccount {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [self selectIdentityDisc];
  for (NSString* cancelButtonId in
           signin::FakeSystemIdentityManagerStaySignedOutButtons()) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kAccountMenuAddAccountButtonId)]
        performAction:grey_tap()];
    // Checks the Fake authentication view is shown
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kFakeAuthActivityViewIdentifier)]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Close the SSO view controller.
    id<GREYMatcher> matcher = grey_allOf(grey_accessibilityID(cancelButtonId),
                                         grey_sufficientlyVisible(), nil);
    [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  }
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controller.
  [ChromeEarlGreyUI waitForAppToIdle];

  // TODO(crbug.com/41493423): Check whether the Add Account or Account Menu
  // should be logged as Signin started histogram.
}

// Tests the enter passphrase button.
- (void)testAddPassphrase {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
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
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [SigninEarlGrey addFakeIdentity:kSecondaryIdentity];
  [self selectIdentityDisc];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSecondaryAccountButtonId)]
      performAction:grey_tap()];

  [SigninEarlGreyUI
      dismissSigninConfirmationSnackbarForIdentity:kSecondaryIdentity
                                     assertVisible:YES];
  [SigninEarlGrey verifySignedInWithFakeIdentity:kSecondaryIdentity];
  [self assertAccountMenuIsNotShown];
}

// Tests that tapping on an account button causes the managed account to sign
// out with a sign-out confirmation dialog.
- (void)testSwitchFromManagedAccount {
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:kManagedIdentity1];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey addFakeIdentity:kPrimaryIdentity];
  [self selectIdentityDisc];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSecondaryAccountButtonId)]
      performAction:grey_tap()];

  // Confirm "Delete and Switch" when alert dialog that data will be cleared
  // is shown. This dialog is only shown when multi profiles are not available.
  if (![SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled]) {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                       IDS_IOS_DATA_NOT_UPLOADED_SWITCH_DIALOG_BUTTON)]
        performAction:grey_tap()];
  }

  if ([ChromeEarlGrey isIPadIdiom]) {
    // The snackbar shows in test executed locally and during actual usage, but
    // is not always detected on CQ causing flakyness.
    // TODO(crbug.com/433726717): Remove the `if` around the assertion when
    // snack-bar stop being flaky on egtest on iphone.
    [SigninEarlGreyUI
        dismissSigninConfirmationSnackbarForIdentity:kPrimaryIdentity
                                       assertVisible:YES];
  }
  [SigninEarlGrey verifySignedInWithFakeIdentity:kPrimaryIdentity];
  [self assertAccountMenuIsNotShown];
}

// TODO(crbug.com/446869344): This test is flaky.
// Tests that tapping on a managed account button causes the primary account
// to be changed and the account menu view to be closed after showing managed
// account sign-in dialog.
- (void)FLAKY_testSwitchToManagedAccount {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [SigninEarlGrey addFakeIdentity:kManagedIdentity1];
  [self selectIdentityDisc];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSecondaryAccountButtonId)]
      performAction:grey_tap()];

  if ([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled]) {
    WaitForEnterpriseOnboardingScreen();
  }
  // Tap on Continue button to acknowledge signing in with a managed account.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_text(l10n_util::GetNSString(
                  IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_CONTINUE_BUTTON_LABEL)),
              grey_interactable(), nil)] performAction:grey_tap()];

  if ([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled]) {
    // Dismiss the history sync screen.
    [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
        performAction:grey_tap()];
  }

  if ([ChromeEarlGrey isIPadIdiom]) {
    // The snackbar shows in test executed locally and during actual usage, but
    // is not always detected on CQ causing flakyness.
    // TODO(crbug.com/433726717): Remove the `if` around the assertion when
    // snack-bar stop being flaky on egtest on iphone.
    [SigninEarlGreyUI
        dismissSigninConfirmationSnackbarForIdentity:kManagedIdentity1
                                       assertVisible:YES];
  }
  [SigninEarlGrey verifySignedInWithFakeIdentity:kManagedIdentity1];
  [self assertAccountMenuIsNotShown];
}

- (void)testSwitchFromManagedAccountToManagedAccount {
  // TODO(crbug.com/433726717): Test disabled on iPhones.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPhones.");
  }

  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:kManagedIdentity1];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey addFakeIdentity:kManagedIdentity2];
  [self selectIdentityDisc];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSecondaryAccountButtonId)]
      performAction:grey_tap()];

  // Confirm "Delete and Switch" when alert dialog that data will be cleared
  // is shown. This dialog is only shown when multi profiles are not available.
  if (![SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled]) {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                       IDS_IOS_DATA_NOT_UPLOADED_SWITCH_DIALOG_BUTTON)]
        performAction:grey_tap()];
  }

  if ([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled]) {
    WaitForEnterpriseOnboardingScreen();
  }
  // Tap on Continue button to acknowledge signing in with a managed account.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_text(l10n_util::GetNSString(
                  IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_CONTINUE_BUTTON_LABEL)),
              grey_interactable(), nil)] performAction:grey_tap()];

  if ([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled]) {
    // Dismiss the history sync screen.
    [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
        performAction:grey_tap()];
  }

  [SigninEarlGreyUI
      dismissSigninConfirmationSnackbarForIdentity:kManagedIdentity2
                                     assertVisible:YES];
  [self assertAccountMenuIsNotShown];
  [SigninEarlGrey verifySignedInWithFakeIdentity:kManagedIdentity2];
}

// Tests remove account from the edit accounts menu.
- (void)testEditAccountsListRemoveAccount {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
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

  // Tap on Remove kPrimaryIdentity button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:kPrimaryIdentity.userEmail])]
      performAction:grey_tap()];

  // Tap on kPrimaryIdentity confirm remove button.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_REMOVE_ACCOUNT_LABEL)] performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];

  // Verify the Account Menu is dismissed.
  [self assertAccountMenuIsNotShown];
}

@end
