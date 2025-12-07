// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/i18n/message_formatter.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_matchers.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using chrome_test_util::ActionSheetItemWithAccessibilityLabelId;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CancelButton;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsPasswordMatcher;
using chrome_test_util::SettingsPasswordSearchMatcher;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::TapWebElementWithId;
using chrome_test_util::TapWebElementWithIdInFrame;
using chrome_test_util::UseSuggestedPasswordMatcher;
using ::password_manager_test_utils::kScrollAmount;

namespace {

const char kFormElementUsername[] = "username";
const char kFormElementPassword[] = "password";

NSString* const kPassphrase = @"hello";

const char kExampleUsername[] = "concrete username";
const char kExamplePassword[] = "concrete password";
const char kExampleBackupPassword[] = "backup password";

const char kFormHTMLFile[] = "/username_password_field_form.html";
const char kIFrameHTMLFile[] = "/iframe_form.html";

// Returns a matcher for the example username in the list.
id<GREYMatcher> UsernameButtonMatcher() {
  return grey_buttonTitle(base::SysUTF8ToNSString(kExampleUsername));
}

// Matcher for the not secure website alert.
id<GREYMatcher> NotSecureWebsiteAlert() {
  return StaticTextWithAccessibilityLabelId(
      IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
}

// Matcher for the overflow menu button shown in the password cells.
id<GREYMatcher> OverflowMenuButton(NSInteger cell_index) {
  return grey_allOf(grey_accessibilityID([ManualFillUtil
                        expandedManualFillOverflowMenuID:cell_index]),
                    grey_interactable(), nullptr);
}

// Matcher for the "Edit" action made available by the overflow menu button.
id<GREYMatcher> OverflowMenuEditAction() {
  return grey_allOf(ButtonWithAccessibilityLabelId(IDS_IOS_EDIT_ACTION_TITLE),
                    grey_interactable(), nullptr);
}

// Matcher for the "Autofill form" button shown in the password cells.
id<GREYMatcher> AutofillFormButton() {
  return grey_allOf(grey_accessibilityID(
                        manual_fill::kExpandedManualFillAutofillFormButtonID),
                    grey_interactable(), nullptr);
}

// Matcher for the page showing the details of a password.
id<GREYMatcher> PasswordDetailsPage() {
  return grey_accessibilityID(kPasswordDetailsViewControllerID);
}

// Matcher for a cell displaying a backup password. A backup password cell
// should come with a backup credential icon and no overflow menu.
id<GREYMatcher> BackupCredentialCell(NSString* host,
                                     int cell_index,
                                     int password_count) {
  NSString* cell_position = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_IOS_MANUAL_FALLBACK_PASSWORD_CELL_INDEX),
          "count", password_count, "position", cell_index + 1));

  NSString* cell_label = [NSString
      stringWithFormat:
          @"%@\n%@",
          l10n_util::GetNSString(
              IDS_IOS_MANUAL_FALLBACK_RECOVERY_PASSWORD_SUGGESTION_TITLE),
          host];

  NSString* accessibility_label =
      [NSString stringWithFormat:@"%@, %@", cell_position, cell_label];

  id<GREYMatcher> backup_icon = grey_accessibilityID(
      kRecoveryPasswordSuggestionIconAccessibilityIdentifier);

  return grey_allOf(grey_accessibilityLabel(accessibility_label),
                    grey_descendant(backup_icon),
                    grey_not(grey_descendant(OverflowMenuButton(cell_index))),
                    nullptr);
}

// Matcher for the "Autofill form" button shown in a backup password cell.
id<GREYMatcher> BackupCredentialAutofillFormButton(NSString* host,
                                                   int cell_index,
                                                   int password_count) {
  return grey_allOf(
      AutofillFormButton(),
      grey_ancestor(BackupCredentialCell(host, cell_index, password_count)),
      nullptr);
}

// Opens the password manual fill view and verifies that the password view
// controller is visible afterwards.
void OpenPasswordManualFillView(bool has_suggestions) {
  id<GREYMatcher> button_to_tap =
      has_suggestions ? manual_fill::KeyboardAccessoryManualFillButton()
                      : manual_fill::PasswordManualFillViewButton();

  // Tap the button that'll open the password manual fill view.
  [[EarlGrey selectElementWithMatcher:button_to_tap] performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      manual_fill::PasswordTableViewMatcher()];
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that the number of accepted suggestions recorded for the given
// `suggestion_index` is as expected. `from_all_password_context` indicates
// whether the suggestion was accpeted form the all password list.
void CheckAutofillSuggestionAcceptedIndexMetricsCount(
    NSInteger suggestion_index,
    bool from_all_password_context = false) {
  NSString* histogram =
      @"Autofill.UserAcceptedSuggestionAtIndex.Password.ManualFallback";
  NSString* error_message = @"Unexpected histogram count for manual fallback "
                            @"accepted password suggestion index.";

  if (from_all_password_context) {
    histogram = [NSString stringWithFormat:@"%@.AllPasswords", histogram];
    error_message =
        [NSString stringWithFormat:@"%@ In all password list.", error_message];
  }

  GREYAssertNil(
      [MetricsAppInterface expectUniqueSampleWithCount:1
                                             forBucket:suggestion_index
                                          forHistogram:histogram],
      error_message);
}

// Validates that the Password Manager UI opened from the manual fallback UI is
// dismissed when local authentication fails.
// - manual_fallback_action_matcher: Matcher for the action button opening the
// Password Manager UI (e.g. "Manage Passwords..." button).
void CheckPasswordManagerUIDismissesAfterFailedAuthentication(
    id<GREYMatcher> manual_fallback_action_matcher) {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Simulate failed authentication.
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [PasswordSettingsAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  // Tap the action in the manual fallback UI.
  [[EarlGrey selectElementWithMatcher:manual_fallback_action_matcher]
      performAction:grey_tap()];

  // Validate reauth UI is visible until auth result is delivered.
  [[EarlGrey selectElementWithMatcher:password_manager_test_utils::
                                          ReauthenticationController()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Deliver authentication result should dismiss the UI.
  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];

  // Verify that the whole navigation stack is gone.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::SettingsNavigationBar()];
}

// Checks that the password manual filling option is as expected and visible.
void CheckPasswordFillingOptionIsVisible(NSString* site) {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     manual_fill::kExpandedManualFillPasswordFaviconID)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_text(site)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:UsernameButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that the keyboard is up and not covered by the password manual fill
// view.
void CheckKeyboardIsUpAndNotCovered() {
  // Verify the keyboard is not covered by the profiles view.
  // TODO(crbug.com/332956674): Remove version check once fixed.
  if (@available(iOS 17.4, *)) {
    // Skip verification.
  } else {
    [ChromeEarlGrey waitForKeyboardToAppear];
  }

  [ChromeEarlGrey waitForNotSufficientlyVisibleElementWithMatcher:
                      manual_fill::PasswordTableViewMatcher()];
}

}  // namespace

// Integration Tests for Mannual Fallback Passwords View Controller.
@interface PasswordViewControllerTestCase : ChromeTestCase

// URL of the current page.
@property(assign) GURL URL;

@end

@implementation PasswordViewControllerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  self.URL = self.testServer->GetURL(kFormHTMLFile);
  [self loadLoginPage];
  [AutofillAppInterface saveExamplePasswordFormToProfileStore];
  [ChromeEarlGrey loadURL:self.URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];

  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Set up histogram tester.
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupUserActionTester]);
}

- (void)tearDownHelper {
  [AutofillAppInterface clearProfilePasswordStore];
  [PasswordSettingsAppInterface removeMockReauthenticationModule];

  // Clean up histogram tester.
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseUserActionTester]);
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector
            (testAutofillFormButtonForBackupCredentialFillsForm)]) {
    config.features_enabled.push_back(
        password_manager::features::kIOSFillRecoveryPassword);
  }

  // The proactive password generation bottom sheet isn't tested here, it
  // is tested in its own suite in password_suggestion_egtest.mm.
  config.features_disabled.push_back(
      password_manager::features::kIOSProactivePasswordGenerationBottomSheet);

  return config;
}

- (void)loadLoginPage {
  [ChromeEarlGrey loadURL:self.URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];
}

// Opens the "Other Passwords" screen.
- (void)openOtherPasswords {
  // Bring up the keyboard.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::WebViewMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Tap the "Select Password..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::OtherPasswordsMatcher()]
      performAction:grey_tap()];

  std::u16string origin = base::ASCIIToUTF16(
      password_manager::GetShownOrigin(url::Origin::Create(self.URL)));

  NSString* message = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_SELECT_PASSWORD_DIALOG_MESSAGE, origin);

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_text(message)];
  [[EarlGrey selectElementWithMatcher:grey_text(message)]
      assertWithMatcher:grey_notNil()];

  // Acknowledge concerns using other passwords on a website.
  [[EarlGrey selectElementWithMatcher:
                 ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_CONFIRM_USING_OTHER_PASSWORD_CONTINUE)]
      performAction:grey_tap()];
}

#pragma mark - Tests

// Tests that the passwords view controller appears on screen.
- (void)testPasswordsViewControllerIsPresented {
  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  // Save password for site.
  NSString* URLString = base::SysUTF8ToNSString(self.URL.spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view and verify that the password controller
  // table view is visible.
  OpenPasswordManualFillView(/*has_suggestions=*/true);

  // Verify that the number of visible suggestions in the manual fallback was
  // correctly recorded.
  NSString* histogram =
      @"ManualFallback.VisibleSuggestions.ExpandIcon.OpenPasswords";
  GREYAssertNil(
      [MetricsAppInterface expectUniqueSampleWithCount:1
                                             forBucket:1
                                          forHistogram:histogram],
      @"Unexpected histogram error for number of visible suggestions.");
}

// Tests that the passwords view controller contains the "Manage Passwords..."
// and "Manage Settings..." actions.
- (void)testPasswordsViewControllerContainsManageActions {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Verify the password controller contains the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::ManagePasswordsMatcher()]
      assertWithMatcher:grey_interactable()];

  // Verify the password controller contains the "Manage Settings..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::ManageSettingsMatcher()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the "Manage Passwords..." action works.
- (void)testManagePasswordsActionOpensPasswordManager {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::ManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Verify that the Password Manager opened.
  // Changed minimum visible percentage to 70% for Passwords table view in
  // settings because subviews cover > 25% in smaller screens(eg. iPhone 6s).
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      assertWithMatcher:grey_minimumVisiblePercent(0.7)];
}

// Tests that the Password Manager is dismissed when local authentication fails
// after tapping "Manage Passwords...".
- (void)testManagePasswordsActionWithFailedAuthDismissesPasswordManager {
  CheckPasswordManagerUIDismissesAfterFailedAuthentication(
      manual_fill::ManagePasswordsMatcher());

  // The keyboard should be visible.
  CheckKeyboardIsUpAndNotCovered();
}

// Tests that the "Manage Settings..." action works.
- (void)testManageSettingsActionOpensPasswordSettings {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::ManageSettingsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  // Changed minimum visible percentage to 70% for Passwords table view in
  // settings because subviews cover > 25% in smaller screens(eg. iPhone 6s).
  [[EarlGrey selectElementWithMatcher:SettingsPasswordMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.7)];
}

// Tests that Password Settings is dismissed when local authentication fails
// after tapping "Manage Settings...".
- (void)testManageSettingsActionWithFailedAuthDismissesPasswordSettings {
  CheckPasswordManagerUIDismissesAfterFailedAuthentication(
      manual_fill::ManageSettingsMatcher());

  // The keyboard should be visible.
  CheckKeyboardIsUpAndNotCovered();
}

// Tests that the "Manage Passwords..." action works in incognito mode.
- (void)testManagePasswordsActionOpensPasswordSettingsInIncognito {
  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::ManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Verify that the Password Manager opened.
  // Changed minimum visible percentage to 70% for Passwords table view in
  // settings because subviews cover > 25% in smaller screens(eg. iPhone 6s).
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      assertWithMatcher:grey_minimumVisiblePercent(0.7)];
}

// Tests that the "Manage Settings..." action works in incognito mode.
- (void)testManageSettingsActionOpensPasswordSettingsInIncognito {
  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Tap the "Manage Settings..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::ManageSettingsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  // Changed minimum visible percentage to 70% for Passwords table view in
  // settings because subviews cover > 25% in smaller screens(eg. iPhone 6s).
  [[EarlGrey selectElementWithMatcher:SettingsPasswordMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.7)];
}

// Tests that the "Select Password..." action works in incognito mode.
// TODO(crbug.com/363017975): Re-enable test
- (void)DISABLED_testSelectPasswordActionInIncognito {
  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadLoginPage];

  [self openOtherPasswords];

  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that returning from "Manage Settings..." leaves the keyboard and the
// icons in the right state.
- (void)testPasswordsStateAfterPresentingManageSettings {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::ManageSettingsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsPasswordMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Verify that the keyboard is not covered by the password view.
  CheckKeyboardIsUpAndNotCovered();
}

// Tests that the "Select Password..." action works.
- (void)testSelectPasswordActionOpensOtherPasswordList {
  [self openOtherPasswords];

  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Select Password..." screen won't open if canceled.
- (void)testCancellingSelectPasswordAction {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Tap the "Select Password..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::OtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Cancel using other passwords on a website.
  [[EarlGrey selectElementWithMatcher:ActionSheetItemWithAccessibilityLabelId(
                                          IDS_CANCEL)]
      performAction:grey_tap()];

  // Verify that the other password list is not opened.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that the other password list can be dismissed with a swipe down.
- (void)testClosingOtherPasswordListViaSwipeDown {
  [self openOtherPasswords];

  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss Other Passwords via swipe.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  [ChromeEarlGrey waitForNotSufficientlyVisibleElementWithMatcher:
                      manual_fill::OtherPasswordsDismissMatcher()];

  // Open it again to make sure the old coordinator was properly cleaned up.
  [self openOtherPasswords];

  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Select Password..." action is only availbale when there are
// saved passwords in the password stores.
- (void)testSelectPasswordActionAvailability {
  // Delete all saved passwords.
  [AutofillAppInterface clearProfilePasswordStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // The "Select Password..." action shouldn't be visible as there a no saved
  // passwords.
  [[EarlGrey selectElementWithMatcher:manual_fill::OtherPasswordsMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Save a password.
  [AutofillAppInterface saveExamplePasswordFormToProfileStore];

  // The "Select Password..." action should now be visible as there's a saved
  // password available.
  [[EarlGrey selectElementWithMatcher:manual_fill::OtherPasswordsMatcher()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the "Select Password..." UI is dismissed after failed local
// authentication.
- (void)testOtherPasswordListUIDismissedAfterFailedAuth {
  // Setup failed authentication.
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [PasswordSettingsAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  [self openOtherPasswords];

  // Validate reauth UI is visible until auth result is delivered.
  [[EarlGrey selectElementWithMatcher:password_manager_test_utils::
                                          ReauthenticationController()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Passwords UI shouldn't be visible.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Deliver authentication result should dismiss the UI.
  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];

  // Verify that the whole navigation stack is gone.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:password_manager_test_utils::
                                                 ReauthenticationController()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      manual_fill::OtherPasswordsDismissMatcher()];

  // The keyboard should be visible.
  CheckKeyboardIsUpAndNotCovered();
}

// Tests that returning from "Select Password..." leaves the view and icons
// in the right state.
- (void)testPasswordsStateAfterPresentingOtherPasswordList {
  [self openOtherPasswords];

  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Verify that the keyboard is not covered by the password view.
  CheckKeyboardIsUpAndNotCovered();
}

// Tests that the Password View Controller is still present after tapping the
// search bar.
// TODO(crbug.com/362893177): Deflake and reenable the test.
- (void)DISABLED_testPasswordControllerWhileSearching {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Tap the "Select Password..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::OtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Acknowledge concerns using other passwords on a website.
  [[EarlGrey selectElementWithMatcher:
                 ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_CONFIRM_USING_OTHER_PASSWORD_CONTINUE)]
      performAction:grey_tap()];

  // Verify that the all saved password list is visible.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  CheckPasswordFillingOptionIsVisible(/*site=*/@"example.com");

  // Tap the password search.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordSearchBarMatcher()]
      performAction:grey_tap()];

  // Verify keyboard is shown and that the password controller is still present
  // in the background.
  [ChromeEarlGrey waitForKeyboardToAppear];
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.5)];
  CheckPasswordFillingOptionIsVisible(/*site=*/@"example.com");

  // Search for a term that shouldn't give any results.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordSearchBarMatcher()]
      performAction:grey_replaceText(@"example1")];
  [[EarlGrey selectElementWithMatcher:grey_text(@"example.com")]
      assertWithMatcher:grey_notVisible()];

  // Search for a term that matches with the saved credential.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordSearchBarMatcher()]
      performAction:grey_replaceText(@"AMPL")];
  CheckPasswordFillingOptionIsVisible(/*site=*/@"example.com");
}

// Tests that the Password View Controller is dismissed when tapping the outside
// the popover on iPad.
- (void)testIPadTappingOutsidePopOverDismissPasswordController {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  [ChromeEarlGreyUI dismissByTappingOnTheWindowOfPopover:
                        manual_fill::PasswordTableViewMatcher()];

  // Verify the password controller table view is not visible and the password
  // icon is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password View Controller is dismissed when tapping the
// keyboard.
// TODO(crbug.com/41428686): started to be flaky and sometimes opens full list
// when typing text.
- (void)DISABLED_testTappingKeyboardDismissPasswordControllerPopOver {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      performAction:grey_replaceText(@"text")];

  // Verify the password controller table view and the password icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:manual_fill::KeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password View Controller stays on rotation.
- (void)testPasswordControllerSupportsRotation {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                   error:nil];

  // Verify the password controller table view is still visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that content is injected in iframe messaging.
- (void)testPasswordControllerSupportsIFrameMessaging {
  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  const GURL URL = self.testServer->GetURL(kIFrameHTMLFile);
  NSString* URLString = base::SysUTF8ToNSString(URL.spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"iFrame"];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithIdInFrame(kFormElementUsername, 0)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/true);

  CheckPasswordFillingOptionIsVisible(
      /*site=*/base::SysUTF8ToNSString(self.URL.GetHost()));

  // Select a username.
  [[EarlGrey selectElementWithMatcher:UsernameButtonMatcher()]
      performAction:grey_tap()];

  // Verify Web Content.
  NSString* javaScriptCondition = [NSString
      stringWithFormat:
          @"window.frames[0].document.getElementById('%s').value === '%s'",
          kFormElementUsername, kExampleUsername];
  [ChromeEarlGrey waitForJavaScriptCondition:javaScriptCondition];
}

// Tests that an alert is shown when trying to fill a password in an unsecure
// field.
- (void)testPasswordControllerPresentsUnsecureAlert {
  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  // Only Objc objects can cross the EDO portal.
  NSString* URLString = base::SysUTF8ToNSString(self.URL.spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/true);

  CheckPasswordFillingOptionIsVisible(
      /*site=*/base::SysUTF8ToNSString(self.URL.GetHost()));

  // Select a password.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordButtonMatcher()]
      performAction:grey_tap()];

  // Look for the alert.
  [[EarlGrey selectElementWithMatcher:NotSecureWebsiteAlert()]
      assertWithMatcher:grey_not(grey_nil())];

  // Dismiss the alert.
  [[EarlGrey selectElementWithMatcher:ActionSheetItemWithAccessibilityLabelId(
                                          IDS_OK)] performAction:grey_tap()];
}

// Tests that the "no passwords found" message is visible when no password
// suggestions are available for the current website.
- (void)testNoPasswordsFoundMessageIsVisibleWhenNoPasswordSuggestions {
  [AutofillAppInterface clearProfilePasswordStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Assert that the "no passwords found" message is visible.
  id<GREYMatcher> noPasswordsFoundMessage = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_PASSWORDS_FOR_SITE));
  [[EarlGrey selectElementWithMatcher:noPasswordsFoundMessage]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the number of visible suggestions in the manual fallback was
  // correctly recorded.
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:0
                         forHistogram:@"ManualFallback.VisibleSuggestions."
                                      @"OpenPasswords"],
      @"Unexpected histogram error for number of visible suggestions.");
}

// Tests password generation on manual fallback.
- (void)testPasswordGenerationOnManualFallback {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementPassword)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Select a suggest password option.
  [[EarlGrey selectElementWithMatcher:manual_fill::SuggestPasswordMatcher()]
      performAction:grey_tap()];

  // Confirm by tapping on the 'Use Suggested Password' button.
  [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordMatcher()]
      performAction:grey_tap()];

  // Verify Web Content.
  NSString* javaScriptCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value !== ''",
                                 kFormElementPassword];
  [ChromeEarlGrey waitForJavaScriptCondition:javaScriptCondition];

  // Verify actions.
  GREYAssertNil(
      [MetricsAppInterface
            expectCount:1
          forUserAction:@"IOS.PasswordManager.PasswordGenerationSheet."
                        @"Present"],
      @"Incorrect user action count for Present");
  GREYAssertNil(
      [MetricsAppInterface
            expectCount:1
          forUserAction:@"IOS.PasswordManager.PasswordGenerationSheet."
                        @"Accept"],
      @"Incorrect user action count for Accept");
}

// Tests password generation on manual fallback not showing for signed in users
// with Passwords toggle in account settings disabled.
// TODO(crbug.com/371189341): Test fails on device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testPasswordGenerationFallbackSignedInPasswordsDisabled \
  testPasswordGenerationFallbackSignedInPasswordsDisabled
#else
#define MAYBE_testPasswordGenerationFallbackSignedInPasswordsDisabled \
  DISABLED_testPasswordGenerationFallbackSignedInPasswordsDisabled
#endif
- (void)MAYBE_testPasswordGenerationFallbackSignedInPasswordsDisabled {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];

  // Disable Passwords toggle in account settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncPasswordsIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/NO)];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementPassword)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Verify the suggest password option is not shown.
  [[EarlGrey selectElementWithMatcher:manual_fill::SuggestPasswordMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests password generation on manual fallback not showing for signed in users
// with encryption error.
// TODO(crbug.com/371189341): Test fails on device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testPasswordGenerationFallbackSignedInEncryptionError \
  testPasswordGenerationFallbackSignedInEncryptionError
#else
#define MAYBE_testPasswordGenerationFallbackSignedInEncryptionError \
  DISABLED_testPasswordGenerationFallbackSignedInEncryptionError
#endif
- (void)MAYBE_testPasswordGenerationFallbackSignedInEncryptionError {
  // TODO(crbug.com/455768802): Re-enable the test.
  if (@available(iOS 26.1, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.1.");
  }

  // Encrypt synced data with a passphrase to enable passphrase encryption for
  // the signed in account.
  [ChromeEarlGrey addSyncPassphrase:kPassphrase];

  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];

  // Verify encryption error is showing in in account settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  // Verify the error section is showing.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncErrorButtonIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  [self loadLoginPage];

  // Swipe up the sync infobar error.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementPassword)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Verify the suggest password option is not shown.
  [[EarlGrey selectElementWithMatcher:manual_fill::SuggestPasswordMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the overflow menu button is visible.
- (void)testOverflowMenuVisibility {
  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  // Save password for site.
  NSString* URLString = base::SysUTF8ToNSString(self.URL.spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/true);

  [[EarlGrey selectElementWithMatcher:OverflowMenuButton(/*cell_index=*/0)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the "Edit" action of the overflow menu button displays the password's
// details in edit mode.
- (void)testEditPasswordFromOverflowMenu {
  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  // Save password for site.
  NSString* URLString = base::SysUTF8ToNSString(self.URL.spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/true);

  // Tap the overflow menu button and select the "Edit" action.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton(/*cell_index=*/0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OverflowMenuEditAction()]
      performAction:grey_tap()];

  // Edit the username.
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          kExampleUsername))]
      performAction:grey_replaceText(@"new username")];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Tap the "Done" button to dismiss the view.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // TODO(crbug.com/332956674): Check that the updated suggestion is visible.
}

// Tests the "Edit" action of the overflow menu button in the all password list
// displays the password's details in edit mode.
- (void)testEditPasswordFromAllPasswordListOverflowMenu {
  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  [self loadLoginPage];

  [self openOtherPasswords];

  // Tap the overflow menu button and select the "Edit" action.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton(/*cell_index=*/0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OverflowMenuEditAction()]
      performAction:grey_tap()];

  // Check that the password details page opened.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsPage()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
}

// Tests that tapping the "Autofill form" button fills the password form with
// the right data.
- (void)testAutofillFormButtonFillsForm {
  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];

  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  // Save password for site.
  NSString* URLString = base::SysUTF8ToNSString(self.URL.spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/true);

  // Tap the "Autofill form" button.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self
      verifyPasswordInfoHasBeenFilled:base::SysUTF8ToNSString(kExampleUsername)
                             password:base::SysUTF8ToNSString(
                                          kExamplePassword)];

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/0);

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];
}

// Tests that tapping the "Autofill form" button doesn't fill the password form
// if reauth failed.
- (void)testAutofillFormButtonWithFailedAuth {
  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kFailure];

  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  // Save password for site.
  NSString* URLString = base::SysUTF8ToNSString(self.URL.spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/true);

  // Tap the "Autofill form" button.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyPasswordInfoHasntBeenFilled];

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];
}

// Tests that tapping the "Autofill form" button in the all password list fills
// the password form with the right data.
#if TARGET_OS_SIMULATOR
#define MAYBE_testAutofillFormButtonInAllPasswordListFillsForm \
  FLAKY_testAutofillFormButtonInAllPasswordListFillsForm
#else
#define MAYBE_testAutofillFormButtonInAllPasswordListFillsForm \
  testAutofillFormButtonInAllPasswordListFillsForm
#endif
- (void)MAYBE_testAutofillFormButtonInAllPasswordListFillsForm {
  // TODO(crbug.com/426435086): Test consistently fails on ipad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPad.");
  }

  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  [self loadLoginPage];

  [self openOtherPasswords];

  // Tap the "Autofill form" button.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self
      verifyPasswordInfoHasBeenFilled:base::SysUTF8ToNSString(kExampleUsername)
                             password:base::SysUTF8ToNSString(
                                          kExamplePassword)];

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(
      /*suggestion_index=*/0, /*from_all_password_context=*/true);
}

// Tests that tapping the "Autofill form" button for a backup credential fills
// the password form with the right data.
- (void)testAutofillFormButtonForBackupCredentialFillsForm {
  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];

  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  // Save a credential with a backup password for the current site.
  NSString* URLString = base::SysUTF8ToNSString(self.URL.spec());
  [AutofillAppInterface savePasswordFormWithBackupForURLSpec:URLString];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the keyboard to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/true);

  // Scroll down to the backup credential cell and tap the "Autofill form"
  // button.
  NSString* host = base::SysUTF8ToNSString(self.URL.GetHost());
  int password_index = 1;
  [[[EarlGrey selectElementWithMatcher:BackupCredentialAutofillFormButton(
                                           host, password_index,
                                           /*password_count=*/2)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self
      verifyPasswordInfoHasBeenFilled:base::SysUTF8ToNSString(kExampleUsername)
                             password:base::SysUTF8ToNSString(
                                          kExampleBackupPassword)];

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(password_index);
}

#pragma mark - Private

// Verify that the password info has been filled.
- (void)verifyPasswordInfoHasBeenFilled:(NSString*)username
                               password:(NSString*)password {
  // Username.
  NSString* usernameCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementUsername, username];

  // Password.
  NSString* passwordCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementPassword, password];

  NSString* condition = [NSString
      stringWithFormat:@"%@ && %@", usernameCondition, passwordCondition];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Verify that the password info has not been filled.
- (void)verifyPasswordInfoHasntBeenFilled {
  // Username.
  NSString* usernameCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === ''",
                       kFormElementUsername];

  // Password.
  NSString* passwordCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value === ''",
                                 kFormElementPassword];

  NSString* condition = [NSString
      stringWithFormat:@"%@ && %@", usernameCondition, passwordCondition];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

@end
