// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::SettingsCollectionView;
using password_manager_test_utils::DeleteCredential;
using password_manager_test_utils::GetInteractionForPasswordIssueEntry;
using password_manager_test_utils::kDefaultPassword;
using password_manager_test_utils::kDefaultSite;
using password_manager_test_utils::kDefaultUsername;
using password_manager_test_utils::PasswordCheckupCellForState;
using password_manager_test_utils::PasswordIssuesTableView;
using password_manager_test_utils::ReauthenticationController;
using password_manager_test_utils::SaveCompromisedPasswordFormToProfileStore;
using password_manager_test_utils::
    SaveMutedCompromisedPasswordFormToProfileStore;
using password_manager_test_utils::SavePasswordFormToProfileStore;

namespace {

constexpr NSString* kDefaultDomain = @"example.com";
constexpr NSString* kDomain1 = @"example1.com";

constexpr NSString* kSite1 = @"https://example1.com/";
constexpr NSString* kSite2 = @"https://example2.com/";
constexpr NSString* kSite3 = @"https://example3.com/";

constexpr NSString* kReusedPassword = @"reused password";
constexpr NSString* kSafePassword = @"s@fe pa55word!";
constexpr NSString* kWeakPassword = @"1";

#pragma mark - Password Checkup Homepage matchers

// Matcher for the TableView in the Password Checkup Homepage.
id<GREYMatcher> PasswordCheckupTableView() {
  return grey_accessibilityID(password_manager::kPasswordCheckupTableViewId);
}

// Matcher for the header image view shown in the Password Checkup Homepage.
id<GREYMatcher> PasswordCheckupHompageHeaderImageView() {
  return grey_accessibilityID(
      password_manager::kPasswordCheckupHeaderImageViewId);
}

// Matcher for the compromised passwords table view item shown in the Password
// Checkup Homepage.
id<GREYMatcher> PasswordCheckupHomepageCompromisedPasswordsItem() {
  return grey_accessibilityID(
      password_manager::kPasswordCheckupCompromisedPasswordsItemId);
}

// Matcher for the reused passwords table view item shown in the Password
// Checkup Homepage.
id<GREYMatcher> PasswordCheckupHomepageReusedPasswordsItem() {
  return grey_accessibilityID(
      password_manager::kPasswordCheckupReusedPasswordsItemId);
}

// Matcher for the weak passwords table view item shown in the Password Checkup
// Homepage.
id<GREYMatcher> PasswordCheckupHomepageWeakPasswordsItem() {
  return grey_accessibilityID(
      password_manager::kPasswordCheckupWeakPasswordsItemId);
}

// Matcher for the "Check Again" button shown in the Password Checkup Homepage.
id<GREYMatcher> CheckAgainButton() {
  return ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_CHECK_AGAIN_BUTTON));
}

// Matcher for the error dialog that pops up in the Password Checkup Homepage
// when an error occurred while running a new password check.
id<GREYMatcher> PasswordCheckupHomepageErrorDialog() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_OFFLINE));
}

// Matcher for the "OK" button of the error dialog that pops up in the Password
// Checkup Homepage when an error occurred while running a new password check.
id<GREYMatcher> PasswordCheckupHomepageErrorDialogOKButton() {
  return chrome_test_util::AlertAction(l10n_util::GetNSString(IDS_OK));
}

#pragma mark - Password Issues matchers

// Matcher for the navigation title of the compromised issues page.
id<GREYMatcher> CompromisedPasswordIssuesPageTitle(int issue_count) {
  return grey_accessibilityLabel(
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_COMPROMISED_PASSWORD_ISSUES_TITLE, issue_count)));
}

// Matcher for dismissed warnings table view item shown in the compromised
// issues page.
id<GREYMatcher> CompromisedPasswordIssuesDismissedWarnings() {
  return grey_accessibilityID(kDismissedWarningsCellID);
}

// Matcher for the navigation title of the dismissed warnings page.
id<GREYMatcher> DismissedWarningsPageTitle() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_DISMISSED_WARNINGS_PASSWORD_ISSUES_TITLE));
}

// Matcher for the navigation title of the reused issues page.
id<GREYMatcher> ReusedPasswordIssuesPageTitle(int issue_count) {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringF(IDS_IOS_REUSED_PASSWORD_ISSUES_TITLE,
                              base::NumberToString16(issue_count)));
}

// Matcher for the navigation title of the weak issues page.
id<GREYMatcher> WeakPasswordIssuesPageTitle(int issue_count) {
  return grey_accessibilityLabel(
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_WEAK_PASSWORD_ISSUES_TITLE, issue_count)));
}

#pragma mark - Password Details matchers

// Matcher for the compromised warning found in a compromised password's details
// page.
id<GREYMatcher> CompromisedWarning() {
  return grey_accessibilityID(kCompromisedWarningID);
}

// Matcher for the "Dismiss Warning" button found in a compromised password's
// details page.
id<GREYMatcher> DismissWarningButton() {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_DISMISS_WARNING));
}

// Matcher for the "Dismiss" button of the confirmation dialog found in a
// compromised password's details page when trying to dismiss the warning.
id<GREYMatcher> DismissWarningConfirmationDialogButton() {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_DISMISS_WARNING_DIALOG_DISMISS_BUTTON));
}

// Matcher for the "Restore Warning" button found in a muted compromised
// password's details page.
id<GREYMatcher> RestoreWarningButton() {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_RESTORE_WARNING));
}

#pragma mark - Helpers

// Saves two reused passwords.
void SaveReusedPasswordFormsToProfileStore() {
  SavePasswordFormToProfileStore(kReusedPassword, kDefaultUsername, kSite1);
  SavePasswordFormToProfileStore(kReusedPassword, kDefaultUsername, kSite2);
}

// Saves a weak password.
void SaveWeakPasswordFormToProfileStore() {
  SavePasswordFormToProfileStore(kWeakPassword, kDefaultUsername, kSite3);
}

// Opens the Password Checkup Homepage.
void OpenPasswordCheckupHomepage(PasswordCheckUIState result_state,
                                 int result_password_count) {
  password_manager_test_utils::OpenPasswordManager();

  id<GREYMatcher> password_checkup_result_cell =
      PasswordCheckupCellForState(result_state, result_password_count);

  // Wait for Password Checkup result.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      password_checkup_result_cell];

  // Open the Password Checkup Homepage and make sure that it is visible.
  [[EarlGrey selectElementWithMatcher:password_checkup_result_cell]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordCheckupTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify that the compromised issues page is correctly presented.
void VerifyCompromisedPasswordIssuesPageIsVisible(int issue_count) {
  [[EarlGrey
      selectElementWithMatcher:CompromisedPasswordIssuesPageTitle(issue_count)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordIssuesTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify that the dismissed warnings page is correctly presented.
void VerifyDismissedWarningsPageIsVisible() {
  [[EarlGrey selectElementWithMatcher:DismissedWarningsPageTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordIssuesTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify that the reused issues page is correctly presented.
void VerifyReusedPasswordIssuesPageIsVisible(int issue_count) {
  [[EarlGrey
      selectElementWithMatcher:ReusedPasswordIssuesPageTitle(issue_count)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordIssuesTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify that the weak issues page is correctly presented.
void VerifyWeakPasswordIssuesPageIsVisible(int issue_count) {
  [[EarlGrey selectElementWithMatcher:WeakPasswordIssuesPageTitle(issue_count)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordIssuesTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Taps the "Back" button of the navigation bar.
void GoBackToPreviousPage() {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
}

// Edits the credential's password.
void EditPassword(NSString* new_password) {
  [[EarlGrey selectElementWithMatcher:password_manager_test_utils::
                                          PasswordDetailPassword()]
      performAction:grey_replaceText(new_password)];

  [[EarlGrey
      selectElementWithMatcher:password_manager_test_utils::EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:password_manager_test_utils::
                                          EditPasswordConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the confirmation dialog is dimsissed.
  [ChromeEarlGreyUI waitForAppToIdle];
}

NSString* LeakedPasswordDescription() {
  return l10n_util::GetNSString(
      IDS_IOS_COMPROMISED_PASSWORD_ISSUES_LEAKED_DESCRIPTION);
}

}  // namespace

// Test case for Password Checkup.
@interface PasswordCheckupTestCase : ChromeTestCase
@end

@implementation PasswordCheckupTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (void)setUp {
  [super setUp];

  // Set the FakeBulkLeakCheckService to return the idle state.
  [PasswordSettingsAppInterface
      setFakeBulkLeakCheckBufferedState:
          password_manager::BulkLeakCheckServiceInterface::State::kIdle];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
}

- (void)tearDown {
  [PasswordSettingsAppInterface removeMockReauthenticationModule];
  [super tearDown];
}

#pragma mark - Tests

// Tests the safe state of the Password Checkup Homepage.
- (void)testPasswordCheckupHomepageSafeState {
  SavePasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(/*result_state=*/PasswordCheckStateSafe,
                              /*result_password_count=*/0);

  // Verify that tapping the items of the insecure types section doesn't open
  // another page.
  [[[EarlGrey selectElementWithMatcher:
                  PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()] assertWithMatcher:grey_sufficientlyVisible()];
  [[[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageReusedPasswordsItem()]
      performAction:grey_tap()] assertWithMatcher:grey_sufficientlyVisible()];
  [[[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageWeakPasswordsItem()]
      performAction:grey_tap()] assertWithMatcher:grey_sufficientlyVisible()];
}

// Validates that the Password Manager UI is dismissed when local authentication
// fails while in the Password Checkup UI.
- (void)testPasswordCheckupHomepageWithFailedAuth {
  SavePasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(/*result_state=*/PasswordCheckStateSafe,
                              /*result_password_count=*/0);

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [PasswordSettingsAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Password Checkup should be covered by reauthentication UI until successful
  // local authentication.
  [[EarlGrey selectElementWithMatcher:PasswordCheckupTableView()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ReauthenticationController()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];

  // Password Manager UI should be dismissed leaving the Settings UI Visible.
  [[EarlGrey selectElementWithMatcher:PasswordCheckupTableView()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ReauthenticationController()]
      assertWithMatcher:grey_notVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the warning state of the Password Checkup Homepage.
- (void)testPasswordCheckupHomepageWarningState {
  SaveMutedCompromisedPasswordFormToProfileStore();
  SaveReusedPasswordFormsToProfileStore();
  SaveWeakPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateReusedPasswords,
      /*result_password_count=*/2);

  // Verify that tapping the compromised passwords item opens the compromised
  // password issues page.
  [[EarlGrey selectElementWithMatcher:
                 PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()];
  VerifyCompromisedPasswordIssuesPageIsVisible(/*issue_count=*/0);

  GoBackToPreviousPage();

  // Verify that tapping the reused passwords item opens the reused password
  // issues page.
  [[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageReusedPasswordsItem()]
      performAction:grey_tap()];
  VerifyReusedPasswordIssuesPageIsVisible(/*issue_count=*/2);

  GoBackToPreviousPage();

  // Verify that tapping the weak passwords item opens the weak password issues
  // page.
  [[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageWeakPasswordsItem()]
      performAction:grey_tap()];
  VerifyWeakPasswordIssuesPageIsVisible(/*issue_count=*/1);
}

// Tests the severe warning state of the Password Checkup Homepage.
- (void)testPasswordCheckupHomepageCompromisedState {
  SaveCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Verify that tapping the reused and weak passwords items doesn't open
  // another page.
  [[[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageReusedPasswordsItem()]
      performAction:grey_tap()] assertWithMatcher:grey_sufficientlyVisible()];
  [[[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageWeakPasswordsItem()]
      performAction:grey_tap()] assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that tapping the compromised passwords item opens the compromised
  // password issues page.
  [[EarlGrey selectElementWithMatcher:
                 PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()];
  VerifyCompromisedPasswordIssuesPageIsVisible(/*issue_count=*/1);
}

// Tests the loading state of the Password Checkup Homepage.
// TODO(crbug.com/40921746): Fix and re enable the test.
- (void)DISABLED_testPasswordCheckupHomepageLoadingState {
  // TODO(crbug.com/41484731): Test fails on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPad.");
  }
  SaveCompromisedPasswordFormToProfileStore();

  NSInteger numberOfAffiliatedGroups = 1;

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Trigger a new check by tapping the "Check Again" button.
  [[EarlGrey selectElementWithMatcher:CheckAgainButton()]
      performAction:grey_tap()];

  // Verify that tapping the items of the insecure types section doesn't open
  // another page.
  [[[EarlGrey selectElementWithMatcher:
                  PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()] assertWithMatcher:grey_sufficientlyVisible()];
  [[[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageReusedPasswordsItem()]
      performAction:grey_tap()] assertWithMatcher:grey_sufficientlyVisible()];
  [[[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageWeakPasswordsItem()]
      performAction:grey_tap()] assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the "Check Again" button is disabled.
  [[EarlGrey selectElementWithMatcher:CheckAgainButton()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Wait for Password Checkup to finish loading.
  [ChromeEarlGrey waitForNotSufficientlyVisibleElementWithMatcher:
                      PasswordCheckupCellForState(PasswordCheckStateRunning,
                                                  numberOfAffiliatedGroups)];

  // Verify that the "Check Again" button is enabled again.
  [[EarlGrey selectElementWithMatcher:CheckAgainButton()]
      assertWithMatcher:grey_userInteractionEnabled()];
}

// Tests the error state of the Password Checkup Homepage.
- (void)testPasswordCheckupHomepageErrorState {
  SaveCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Set the FakeBulkLeakCheckService to return the offline error state.
  [PasswordSettingsAppInterface
      setFakeBulkLeakCheckBufferedState:
          password_manager::BulkLeakCheckServiceInterface::State::
              kNetworkError];

  // Trigger a new check by tapping the "Check Again" button.
  [[EarlGrey selectElementWithMatcher:CheckAgainButton()]
      performAction:grey_tap()];

  // Wait for the error dialog to pop up.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      PasswordCheckupHomepageErrorDialog()];

  // Tap the OK button.
  [[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageErrorDialogOKButton()]
      performAction:grey_tap()];

  // Verify that the Password Checkup Homepage is still visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(password_manager::kPasswordCheckupTableViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Password Checkup Homepage header image view is correctly
// shown/hidden depending on the device's orientation.
- (void)testPasswordCheckupHomepageDeviceOrientation {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Landscape orientation doesn't change the look of "
                           @"the Password Checkup Homepage.");
  }

  SavePasswordFormToProfileStore();

  // Rotate device to left landscape orientation before opening the Password
  // Checkup Homepage.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  OpenPasswordCheckupHomepage(/*result_state=*/PasswordCheckStateSafe,
                              /*result_password_count=*/0);

  // The header image view should not be visible after being rotated to left
  // landscape orientation.
  [[EarlGrey selectElementWithMatcher:PasswordCheckupHompageHeaderImageView()]
      assertWithMatcher:grey_notVisible()];

  // The header image view should be visible after being rotated to portrait
  // orientation.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [[EarlGrey selectElementWithMatcher:PasswordCheckupHompageHeaderImageView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The header image view should not be visible after being rotated to right
  // landscape orientation.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  [[EarlGrey selectElementWithMatcher:PasswordCheckupHompageHeaderImageView()]
      assertWithMatcher:grey_notVisible()];
}

// Tests opening the Password Issues page.
- (void)testOpeningPasswordIssues {
  SaveCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Auth should not be required to open Password Issues. The user is
  // authenticated when opening the Password Manager page. Catch any unexpected
  // authentication requests.
  [PasswordSettingsAppInterface mockReauthenticationModuleCanAttempt:NO];

  // Open the compromised issues page.
  [[EarlGrey selectElementWithMatcher:
                 PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()];

  VerifyCompromisedPasswordIssuesPageIsVisible(/*issue_count=*/1);
}

// Tests dismissing a compromised password warning.
- (void)testPasswordCheckupDismissCompromisedPasswordWarning {
  SaveCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Open the compromised issues page.
  [[EarlGrey selectElementWithMatcher:
                 PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()];

  VerifyCompromisedPasswordIssuesPageIsVisible(/*issue_count=*/1);

  // Validate that the compromised password is present in the list and that the
  // "Dismissed Warning" cell is not present.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      assertWithMatcher:grey_sufficientlyVisible()];
  [password_manager_test_utils::GetInteractionForIssuesListItem(
      CompromisedPasswordIssuesDismissedWarnings(), kGREYDirectionDown)
      assertWithMatcher:grey_notVisible()];

  // Open the password's details.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CompromisedWarning()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the "Dismiss Warning" button and confirm the warning dismissal.
  [[EarlGrey selectElementWithMatcher:DismissWarningButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DismissWarningConfirmationDialogButton()]
      performAction:grey_tap()];

  // Wait until the alert and the details view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the compromised warning is gone.
  [[EarlGrey selectElementWithMatcher:CompromisedWarning()]
      assertWithMatcher:grey_notVisible()];

  GoBackToPreviousPage();

  // Check that the current view is now the compromised password issues view.
  VerifyCompromisedPasswordIssuesPageIsVisible(/*issue_count=*/0);

  // Verify that the password is not in the list anymore and that the "Dismissed
  // Warning" cell is now present.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      assertWithMatcher:grey_notVisible()];
  [password_manager_test_utils::GetInteractionForIssuesListItem(
      CompromisedPasswordIssuesDismissedWarnings(), kGREYDirectionDown)
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests restoring a muted compromised password warning.
- (void)testPasswordCheckupRestoreCompromisedPasswordWarning {
  SaveMutedCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateDismissedWarnings,
      /*result_password_count=*/1);

  // Open the compromised issues page.
  [[EarlGrey selectElementWithMatcher:
                 PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()];

  // Verify that the compromised password issues page is displayed and that the
  // "Dismissed Warning" cell is present.
  VerifyCompromisedPasswordIssuesPageIsVisible(/*issue_count=*/0);
  [password_manager_test_utils::GetInteractionForIssuesListItem(
      CompromisedPasswordIssuesDismissedWarnings(), kGREYDirectionDown)
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the dismissed warnings page.
  [[EarlGrey
      selectElementWithMatcher:CompromisedPasswordIssuesDismissedWarnings()]
      performAction:grey_tap()];

  // Verify that the dismissed warnings is displayed and that the muted password
  // is in the list.
  VerifyDismissedWarningsPageIsVisible();
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the password's details.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CompromisedWarning()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the "Restore Warning" button.
  [[EarlGrey selectElementWithMatcher:RestoreWarningButton()]
      performAction:grey_tap()];

  // Wait until the details view is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the compromised password issues view.
  VerifyCompromisedPasswordIssuesPageIsVisible(/*issue_count=*/1);

  // Verify that the compromised password is present in the list and that the
  // "Dismissed Warning" cell is not present anymore.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      assertWithMatcher:grey_sufficientlyVisible()];
  [password_manager_test_utils::GetInteractionForIssuesListItem(
      CompromisedPasswordIssuesDismissedWarnings(), kGREYDirectionDown)
      assertWithMatcher:grey_notVisible()];
}

// Tests deleting the last saved password through Password Checkup.
- (void)testDeleteLastPassword {
  SaveCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Open the compromised issues page.
  [[EarlGrey selectElementWithMatcher:
                 PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()];

  // Open the password's details.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      performAction:grey_tap()];

  // Enter edit mode and delete the password.
  password_manager_test_utils::TapNavigationBarEditButton();
  DeleteCredential(kDefaultUsername, kDefaultSite);

  // Wait until the details view is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the empty view of Password Manager is now displayed.
  [[EarlGrey selectElementWithMatcher:password_manager_test_utils::
                                          PasswordManagerEmptyView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests resolving the last reused passwords issue by editing a password through
// Password Checkup.
- (void)testResolveLastIssueByEditingPassword {
  SaveReusedPasswordFormsToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateReusedPasswords,
      /*result_password_count=*/2);

  // Open the reused issues page.
  [[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageReusedPasswordsItem()]
      performAction:grey_tap()];

  // Open one of the password's details.
  [GetInteractionForPasswordIssueEntry(kDomain1, kDefaultUsername)
      performAction:grey_tap()];

  // Enter edit mode and change the password to something that's not weak.
  password_manager_test_utils::TapNavigationBarEditButton();
  EditPassword(kSafePassword);

  GoBackToPreviousPage();

  // Wait until the details view is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the Password Checkup Homepage is now displayed and that tapping
  // the reused passwords item doesn't open the issues page since there are no
  // reused issues left.
  [[[[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageReusedPasswordsItem()]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests resolving the last compromised passwords issue by deleting a password
// through Password Checkup.
- (void)testResolveLastIssueByDeletingPassword {
  SavePasswordFormToProfileStore(kSafePassword, kDefaultUsername, kSite1);
  SaveCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Open the compromised issues page.
  [[EarlGrey selectElementWithMatcher:
                 PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()];

  // Open the password's details.

  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      performAction:grey_tap()];

  // Enter edit mode and change the password to something that's not weak.
  password_manager_test_utils::TapNavigationBarEditButton();
  DeleteCredential(kDefaultUsername, kDefaultSite);

  // Wait until the details view is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the Password Checkup Homepage is now displayed and that tapping
  // the compromised passwords item doesn't open the issues page since there are
  // no compromised issues left.
  [[[[EarlGrey selectElementWithMatcher:
                   PasswordCheckupHomepageCompromisedPasswordsItem()]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests resolving the last compromised passwords issue by deleting a password
// through Password Checkup.
- (void)testChangeCompromisedPasswordToSafePassword {
  SaveCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Open the compromised issues page.
  [[EarlGrey selectElementWithMatcher:
                 PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()];

  // Open the password's details.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CompromisedWarning()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DismissWarningButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Enter edit mode and change the password to something that's not
  // compromised.
  password_manager_test_utils::TapNavigationBarEditButton();
  EditPassword(kSafePassword);

  // Verify that the compromised warning and the "Dismiss Warning" button are
  // now gone.
  [[EarlGrey selectElementWithMatcher:CompromisedWarning()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:DismissWarningButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests changing the password of a muted compromised password to a weak
// password.
- (void)testChangeMutedPasswordToWeakPassword {
  SaveMutedCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateDismissedWarnings,
      /*result_password_count=*/1);

  // Open the compromised issues page.
  [[EarlGrey selectElementWithMatcher:
                 PasswordCheckupHomepageCompromisedPasswordsItem()]
      performAction:grey_tap()];

  // Open the dismissed warnings page.
  [[EarlGrey
      selectElementWithMatcher:CompromisedPasswordIssuesDismissedWarnings()]
      performAction:grey_tap()];

  // Open the password's details.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername,
                                       LeakedPasswordDescription())
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CompromisedWarning()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:RestoreWarningButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Enter edit mode and change the password to something that's not
  // compromised.
  password_manager_test_utils::TapNavigationBarEditButton();
  EditPassword(kWeakPassword);

  // Verify that the compromised warning and the "Restore Warning" button are
  // now gone.
  [[EarlGrey selectElementWithMatcher:CompromisedWarning()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:RestoreWarningButton()]
      assertWithMatcher:grey_notVisible()];

  GoBackToPreviousPage();

  // Wait until the details view is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the Password Checkup Homepage is now displayed and that tapping
  // the compromised passwords item doesn't open the issues page since there are
  // no muted compromised issues left.
  [[[[EarlGrey selectElementWithMatcher:
                   PasswordCheckupHomepageCompromisedPasswordsItem()]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that tapping the weak passwords item opens the weak password issues
  // page since there is now a weak password issue.
  [[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageWeakPasswordsItem()]
      performAction:grey_tap()];
  VerifyWeakPasswordIssuesPageIsVisible(/*issue_count=*/1);
}

// Tests the details page of a credential that is both weak and compromised when
// opened from the weak issues page.
- (void)testCompromisedAndWeakPasswordOpenedInWeakContext {
  SaveCompromisedPasswordFormToProfileStore(kWeakPassword);

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Open the weak issues page.
  [[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageWeakPasswordsItem()]
      performAction:grey_tap()];

  // Open the password's details.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername)
      performAction:grey_tap()];

  // Verify that the compromised warning and the "Dismiss Warning" button are
  // not present.
  [[EarlGrey selectElementWithMatcher:CompromisedWarning()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:DismissWarningButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests the details page of a credential that is both reused and compromised
// when opened from the reused issues page.
- (void)testCompromisedAndReusedPasswordOpenedInReusedContext {
  SaveCompromisedPasswordFormToProfileStore(kReusedPassword);
  SaveReusedPasswordFormsToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateUnmutedCompromisedPasswords,
      /*result_password_count=*/1);

  // Open the reused issues page.
  [[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageReusedPasswordsItem()]
      performAction:grey_tap()];

  // Open the password's details.
  [GetInteractionForPasswordIssueEntry(kDefaultDomain, kDefaultUsername)
      performAction:grey_tap()];

  // Verify that the compromised warning and the "Dismiss Warning" button are
  // not present.
  [[EarlGrey selectElementWithMatcher:CompromisedWarning()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:DismissWarningButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that Password Checkup Homepage is dismissed when there are no saved
// passwords.
- (void)testPasswordCheckupDismissedAfterAllPasswordsGone {
  SavePasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateSafe,
      /*result_password_count=*/0);

  [PasswordSettingsAppInterface clearProfilePasswordStore];

  // Verify that the Password Checkup Homepage is dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(password_manager::kPasswordCheckupTableViewId)]
      assertWithMatcher:grey_nil()];
}

// Validates that the Password Manager UI is dismissed when local authentication
// fails while in the Password Issues UI.
- (void)testPasswordIssuesWithFailedAuth {
  SaveWeakPasswordFormToProfileStore();

  OpenPasswordCheckupHomepage(
      /*result_state=*/PasswordCheckStateWeakPasswords,
      /*result_password_count=*/1);

  // Verify that tapping the reused passwords item opens the reused password
  // issues page.
  [[EarlGrey
      selectElementWithMatcher:PasswordCheckupHomepageWeakPasswordsItem()]
      performAction:grey_tap()];
  VerifyWeakPasswordIssuesPageIsVisible(/*issue_count=*/1);

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [PasswordSettingsAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Reauthentication UI should be covering Password Issues.
  [[EarlGrey
      selectElementWithMatcher:WeakPasswordIssuesPageTitle(/*issue_count=*/1)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ReauthenticationController()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];

  // Password Manager UI should have been dismissed leaving Settings visible.
  [[EarlGrey
      selectElementWithMatcher:WeakPasswordIssuesPageTitle(/*issue_count=*/1)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ReauthenticationController()]
      assertWithMatcher:grey_notVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
