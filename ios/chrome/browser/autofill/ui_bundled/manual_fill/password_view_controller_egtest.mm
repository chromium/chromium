// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_matchers.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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

namespace {

const char kFormElementUsername[] = "username";
const char kFormElementPassword[] = "password";

NSString* const kPassphrase = @"hello";

const char kExampleUsername[] = "concrete username";

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

// Matcher for the confirmation dialog Continue button.
id<GREYMatcher> ConfirmUsingOtherPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(
                        IDS_IOS_CONFIRM_USING_OTHER_PASSWORD_CONTINUE),
                    grey_interactable(), nullptr);
}

// Matcher for the confirmation dialog Cancel button.
id<GREYMatcher> CancelUsingOtherPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(IDS_CANCEL),
                    grey_interactable(), nullptr);
}

// Matcher for the overflow menu button shown in the password cells.
id<GREYMatcher> OverflowMenuButton() {
  return grey_allOf(
      grey_accessibilityID(manual_fill::kExpandedManualFillOverflowMenuID),
      grey_interactable(), nullptr);
}

// Matcher for the "Edit" action made available by the overflow menu button.
id<GREYMatcher> OverflowMenuEditAction() {
  return grey_allOf(ButtonWithAccessibilityLabelId(IDS_IOS_EDIT_ACTION_TITLE),
                    grey_interactable(), nullptr);
}

// Matcher for the "Autofill Form" button shown in the password cells.
id<GREYMatcher> AutofillFormButton() {
  return grey_allOf(grey_accessibilityID(
                        manual_fill::kExpandedManualFillAutofillFormButtonID),
                    grey_interactable(), nullptr);
}

// Matcher for the page showing the details of a password.
id<GREYMatcher> PasswordDetailsPage() {
  return grey_accessibilityID(kPasswordDetailsViewControllerID);
}

// Opens the password manual fill view and verifies that the password view
// controller is visible afterwards.
void OpenPasswordManualFillView(bool has_suggestions) {
  id<GREYMatcher> button_to_tap;
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    button_to_tap = has_suggestions
                        ? grey_accessibilityLabel(l10n_util::GetNSString(
                              IDS_IOS_AUTOFILL_ACCNAME_AUTOFILL_DATA))
                        : manual_fill::PasswordManualFillViewButton();
  } else {
    button_to_tap = manual_fill::PasswordIconMatcher();
    [[EarlGrey
        selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  }

  // Tap the button that'll open the password manual fill view.
  [[EarlGrey selectElementWithMatcher:button_to_tap] performAction:grey_tap()];

  // Verify the password controller table view is visible.
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

- (BOOL)shouldEnableKeyboardAccessoryUpgradeFeature {
  return YES;
}

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
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDown {
  [AutofillAppInterface clearProfilePasswordStore];
  [PasswordSettingsAppInterface removeMockReauthenticationModule];

  // Clean up histogram tester.
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self shouldEnableKeyboardAccessoryUpgradeFeature]) {
    config.features_enabled.push_back(kIOSKeyboardAccessoryUpgrade);
  } else {
    config.features_disabled.push_back(kIOSKeyboardAccessoryUpgrade);
  }

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

  [[EarlGrey selectElementWithMatcher:grey_text(message)]
      assertWithMatcher:grey_notNil()];

  // Acknowledge concerns using other passwords on a website.
  [[EarlGrey selectElementWithMatcher:ConfirmUsingOtherPasswordButton()]
      performAction:grey_tap()];
}

// Tests that the passwords view controller appears on screen.
- (void)testPasswordsViewControllerIsPresented {
  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

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

  // Verify that the number of visible suggestions in the keyboard accessory was
  // correctly recorded.
  NSString* histogram =
      [AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]
          ? @"ManualFallback.VisibleSuggestions.ExpandIcon.OpenPasswords"
          : @"ManualFallback.VisibleSuggestions.OpenPasswords";
  GREYAssertNil(
      [MetricsAppInterface expectUniqueSampleWithCount:1
                                             forBucket:1
                                          forHistogram:histogram],
      @"Unexpected histogram error for number of visible suggestions.");
}

// Tests that the passwords view controller contains the "Manage Passwords..."
// and "Manage Settings..." actions.
- (void)testPasswordsViewControllerContainsManageActions {
  // TODO(crbug.com/40857537): Re-enable when flake fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test flaky failing on iPad.")
  }

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
  // TODO(crbug.com/363172305): Re-enable when flake fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test flaky failing on iPad.")
  }

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
- (void)testSelectPasswordActionInIncognito {
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
  // TODO(crbug.com/363172305): Re-enable when flake fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test flaky failing on iPad.")
  }

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Icons are not present when the Keyboard Accessory Upgrade feature is
  // enabled.
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    // Verify the status of the icon.
    [[EarlGrey selectElementWithMatcher:manual_fill::PasswordIconMatcher()]
        assertWithMatcher:grey_not(grey_userInteractionEnabled())];
  }

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::ManageSettingsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsPasswordMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // TODO(crbug.com/332956674): Keyboard and keyboard accessory are not present
  // on iOS 17.4+, remove version check once fixed.
  if (@available(iOS 17.4, *)) {
    // Skip verifications.
  } else {
    // Icons are not present when the Keyboard Accessory Upgrade feature is
    // enabled.
    if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
      // Verify the status of the icons.
      [[EarlGrey selectElementWithMatcher:manual_fill::PasswordIconMatcher()]
          assertWithMatcher:grey_sufficientlyVisible()];
      [[EarlGrey selectElementWithMatcher:manual_fill::PasswordIconMatcher()]
          assertWithMatcher:grey_userInteractionEnabled()];
      [[EarlGrey selectElementWithMatcher:manual_fill::KeyboardIconMatcher()]
          assertWithMatcher:grey_not(grey_sufficientlyVisible())];
    }
  }

  // Verify that the keyboard is not covered by the password view.
  CheckKeyboardIsUpAndNotCovered();
}

// Tests that the "Select Password..." action works.
- (void)testSelectPasswordActionOpensOtherPasswordList {
  // TODO(crbug.com/363172305): Re-enable when flake fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test flaky failing on iPad.")
  }

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
  [[EarlGrey selectElementWithMatcher:CancelUsingOtherPasswordButton()]
      performAction:grey_tap()];

  // Verify that the other password list is not opened.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that the other password list can be dismissed with a swipe down.
- (void)testClosingOtherPasswordListViaSwipeDown {
  // TODO(crbug.com/363172305): Re-enable when flake fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test flaky failing on iPad.")
  }

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
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

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
  // TODO(crbug.com/363172305): Re-enable when flake fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test flaky failing on iPad.")
  }

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

  // TODO(crbug.com/332956674): Keyboard and keyboard accessory are not present
  // on iOS 17.4+, remove version check once fixed.
  if (@available(iOS 17.4, *)) {
    // Skip verifications.
  } else {
    // Icons are not present when the Keyboard Accessory Upgrade feature is
    // enabled.
    if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
      // Verify the status of the icons.
      [[EarlGrey selectElementWithMatcher:manual_fill::PasswordIconMatcher()]
          assertWithMatcher:grey_sufficientlyVisible()];
      [[EarlGrey selectElementWithMatcher:manual_fill::PasswordIconMatcher()]
          assertWithMatcher:grey_userInteractionEnabled()];
      [[EarlGrey selectElementWithMatcher:manual_fill::KeyboardIconMatcher()]
          assertWithMatcher:grey_not(grey_sufficientlyVisible())];
    }
  }

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
  [[EarlGrey selectElementWithMatcher:ConfirmUsingOtherPasswordButton()]
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

// Tests that the Password View Controller is dismissed when tapping the
// keyboard icon.
- (void)testKeyboardIconDismissPasswordController {
  if ([ChromeEarlGrey isIPadIdiom] ||
      [AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"The keyboard icon is never present on iPads or when the Keyboard "
        @"Accessory Upgrade feature is enabled.");
  }

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Tap on the keyboard icon.
  [[EarlGrey selectElementWithMatcher:manual_fill::KeyboardIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view and the password icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:manual_fill::KeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
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
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordIconMatcher()]
      assertWithMatcher:grey_interactable()];
  // Verify the interaction status of the password icon.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
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

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  // Verify the password controller table view is still visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that content is injected in iframe messaging.
- (void)testPasswordControllerSupportsIFrameMessaging {
  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

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
      /*site=*/base::SysUTF8ToNSString(self.URL.host()));

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
  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

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
      /*site=*/base::SysUTF8ToNSString(self.URL.host()));

  // Select a password.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordButtonMatcher()]
      performAction:grey_tap()];

  // Look for the alert.
  [[EarlGrey selectElementWithMatcher:NotSecureWebsiteAlert()]
      assertWithMatcher:grey_not(grey_nil())];

  // Dismiss the alert.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI cleanupAfterShowingAlert];
}

// Tests that the password icon is not present when no passwords are available.
- (void)testPasswordIconIsNotVisibleWhenPasswordStoreEmpty {
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is enabled.");
  }

  [AutofillAppInterface clearProfilePasswordStore];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Assert the password icon is not enabled and not visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the "no passwords found" message is visible when no password
// suggestions are available for the current website.
- (void)testNoPasswordsFoundMessageIsVisibleWhenNoPasswordSuggestions {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the Keyboard "
                           @"Accessory Upgrade feature is disabled.");
  }

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

  // Verify that the number of visible suggestions in the keyboard accessory was
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
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(10)];

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
}

// Tests password generation on manual fallback for signed in not syncing users.
- (void)testPasswordGenerationOnManualFallbackSignedInNotSyncingAccount {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(10)];

  [self loadLoginPage];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementPassword)];

  // Open the password manual fill view.
  OpenPasswordManualFillView(/*has_suggestions=*/false);

  // Verify a suggest password option is showing.
  [[EarlGrey selectElementWithMatcher:manual_fill::SuggestPasswordMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select a suggest password option.
  [[EarlGrey selectElementWithMatcher:manual_fill::SuggestPasswordMatcher()]
      performAction:grey_tap()];
}

// Tests password generation on manual fallback not showing for signed in not
// syncing users with Passwords toggle in account settings disbaled.
// TODO(crbug.com/371189341): Test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testPasswordGenerationFallbackSignedInNotSyncingPasswordsDisabled \
  testPasswordGenerationFallbackSignedInNotSyncingPasswordsDisabled
#else
#define MAYBE_testPasswordGenerationFallbackSignedInNotSyncingPasswordsDisabled \
  DISABLED_testPasswordGenerationFallbackSignedInNotSyncingPasswordsDisabled
#endif
- (void)
    MAYBE_testPasswordGenerationFallbackSignedInNotSyncingPasswordsDisabled {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(10)];

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

// Tests password generation on manual fallback not showing for signed in not
// syncing users with encryption error.
// TODO(crbug.com/371189341): Test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testPasswordGenerationFallbackSignedInNotSyncingEncryptionError \
  testPasswordGenerationFallbackSignedInNotSyncingEncryptionError
#else
#define MAYBE_testPasswordGenerationFallbackSignedInNotSyncingEncryptionError \
  DISABLED_testPasswordGenerationFallbackSignedInNotSyncingEncryptionError
#endif
- (void)MAYBE_testPasswordGenerationFallbackSignedInNotSyncingEncryptionError {
  // Encrypt synced data with a passphrase to enable passphrase encryption for
  // the signed in account.
  [ChromeEarlGrey addSyncPassphrase:kPassphrase];

  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(10)];

  // Verify encryption error is showing in in account settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  // Verify the error section is showing.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON))]
      assertWithMatcher:grey_sufficientlyVisible()];
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

// Tests that the overflow menu button is only visible when the Keyboard
// Accessory Upgrade feature is enabled.
- (void)testOverflowMenuVisibility {
  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

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

  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests the "Edit" action of the overflow menu button displays the password's
// details in edit mode.
- (void)testEditPasswordFromOverflowMenu {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

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
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
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
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

  [self loadLoginPage];

  [self openOtherPasswords];

  // Tap the overflow menu button and select the "Edit" action.
  [[EarlGrey selectElementWithMatcher:OverflowMenuButton()]
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

// Tests that tapping the "Autofill Form" button fills the password form with
// the right data.
- (void)testAutofillFormButtonFillsForm {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];

  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

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

  // Tap the "Autofill Form" button.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyPasswordInfoHasBeenFilled:base::SysUTF8ToNSString(
                                            kExampleUsername)];

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/0);

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];
}

// Tests that tapping the "Autofill Form" button doesn't fill the password form
// if reauth failed.
- (void)testAutofillFormButtonWithFailedAuth {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kFailure];

  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

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

  // Tap the "Autofill Form" button.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyPasswordInfoHasntBeenFilled];

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];
}

// Tests that tapping the "Autofill Form" button in the all password list fills
// the password form with the right data.
- (void)testAutofillFormButtonInAllPasswordListFillsForm {
  if (![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_DISABLED(@"This test is not relevant when the Keyboard "
                            @"Accessory Upgrade feature is disabled.")
  }

  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

  [self loadLoginPage];

  [self openOtherPasswords];

  // Tap the "Autofill Form" button.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyPasswordInfoHasBeenFilled:base::SysUTF8ToNSString(
                                            kExampleUsername)];

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(
      /*suggestion_index=*/0, /*from_all_password_context=*/true);
}

#pragma mark - Private

// Verify that the password info has been filled.
- (void)verifyPasswordInfoHasBeenFilled:(NSString*)username {
  // Username.
  NSString* usernameCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementUsername, username];

  // Password.
  NSString* passwordCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value !== ''",
                                 kFormElementPassword];

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

// Rerun all the tests in this file but with kIOSKeyboardAccessoryUpgrade
// disabled. This will be removed once that feature launches fully, but ensures
// regressions aren't introduced in the meantime.
@interface PasswordViewControllerKeyboardAccessoryUpgradeDisabledTestCase
    : PasswordViewControllerTestCase

@end

@implementation PasswordViewControllerKeyboardAccessoryUpgradeDisabledTestCase

- (BOOL)shouldEnableKeyboardAccessoryUpgradeFeature {
  return NO;
}

// This causes the test case to actually be detected as a test case. The actual
// tests are all inherited from the parent class.
- (void)testEmpty {
}

@end
