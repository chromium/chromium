// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/features.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/elide_url.h"
#import "components/webauthn/ios/features.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/manual_fill/test/manual_fill_matchers.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/device_reauth/test/reauthentication_app_interface.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_app_interface.h"
#import "ios/chrome/browser/passwords/bottom_sheet/test/credential_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/browser/passwords/password_suggestion/public/password_suggestion_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/webauthn/test/ios_chrome_passkey_client_app_interface.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_traits_overrider.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::NavigationBarEditButton;
using chrome_test_util::WebViewMatcher;
using password_manager_test_utils::DeleteCredential;
using password_manager_test_utils::kDefaultUserDisplayName;
using password_manager_test_utils::SaveExamplePasskeyToStore;

static constexpr char kFormUsernameId1[] = "un";
static constexpr char kFormPasswordId1[] = "pw";
static constexpr char kFormUsernameId2[] = "username";
static constexpr char kFormPasswordId2[] = "password";
static constexpr char kLocalhost[] = "localhost";

namespace {

id<GREYMatcher> ButtonWithAccessibilityID(NSString* id) {
  return grey_allOf(grey_accessibilityID(id),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the bottom sheet's "Continue" button.
id<GREYMatcher> ContinueButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CREDENTIAL_BOTTOM_SHEET_CONTINUE);
}

id<GREYMatcher> SubtitleString(const GURL& url) {
  return grey_anyOf(
      grey_text(l10n_util::GetNSStringF(
          IDS_IOS_CREDENTIAL_BOTTOM_SHEET_SUBTITLE,
          url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
              url))),
      grey_text(l10n_util::GetNSStringF(
          IDS_IOS_CREDENTIAL_BOTTOM_SHEET_SUBTITLE_WITH_PASSKEYS,
          url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
              url))),
      nil);
}

id<GREYMatcher> SubtitleWithPasskeysString(const GURL& url) {
  return grey_text(l10n_util::GetNSStringF(
      IDS_IOS_CREDENTIAL_BOTTOM_SHEET_SUBTITLE_WITH_PASSKEYS,
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          url)));
}

// Returns the matcher for the use password button.
id<GREYMatcher> UsePasswordButton() {
  return grey_anyOf(
      chrome_test_util::StaticTextWithAccessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CREDENTIAL_BOTTOM_SHEET_USE_PASSWORD)),
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_CREDENTIAL_BOTTOM_SHEET_CONTINUE),
      nil);
}

// Returns the matcher for the open keyboard button.
id<GREYMatcher> OpenKeyboardButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CREDENTIAL_BOTTOM_SHEET_USE_KEYBOARD);
}

// Returns the matcher for the "Show details" context menu item.
id<GREYMatcher> ShowDetailsContextMenuItem() {
  return grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                        IDS_IOS_CREDENTIAL_BOTTOM_SHEET_SHOW_DETAILS),
                    grey_interactable(), nullptr);
}

// Returns the matcher for the "Password Manager" context menu item.
id<GREYMatcher> PasswordManagerContextMenuItem() {
  return grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                        IDS_IOS_CREDENTIAL_BOTTOM_SHEET_PASSWORD_MANAGER),
                    grey_interactable(), nullptr);
}

// Returns the matcher for the backup password suggestion with the given
// `suggestion_username`.
id<GREYMatcher> BackupPasswordSuggestion(NSString* suggestion_username) {
  id<GREYMatcher> backup_icon = grey_accessibilityID(
      kRecoveryPasswordSuggestionIconAccessibilityIdentifier);
  id<GREYMatcher> backup_text = grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_BOTTOM_SHEET_RECOVERY_PASSWORD_LABEL));
  return grey_allOf(grey_accessibilityID(suggestion_username),
                    grey_descendant(backup_icon), grey_descendant(backup_text),
                    nullptr);
}

// Matcher for the autofill passkey suggestion chip in the keyboard accessory.
id<GREYMatcher> KeyboardAccessoryPasskeySuggestion(NSString* username,
                                                   NSString* index_value) {
  NSString* passkey_subtext =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_SUGGESTION_LABEL);
  NSString* expected_accessibility_label =
      [NSString stringWithFormat:@"%@, %@", username, passkey_subtext];
  return grey_allOf(grey_accessibilityLabel(expected_accessibility_label),
                    grey_accessibilityValue(index_value),
                    grey_ancestor(grey_accessibilityID(
                        kFormInputAccessoryViewAccessibilityID)),
                    grey_interactable(), nil);
}

// Matcher for the autofill password suggestion chip in the keyboard accessory
// on conditional login.
id<GREYMatcher> KeyboardAccessoryPasswordSuggestionOnConditionalLogin(
    NSString* username,
    NSString* index_value) {
  NSString* password_subtext = l10n_util::GetNSString(IDS_IOS_PASSWORD_SUBTEXT);
  NSString* expected_accessibility_label =
      [NSString stringWithFormat:@"%@, %@", username, password_subtext];
  return grey_allOf(grey_accessibilityLabel(expected_accessibility_label),
                    grey_accessibilityValue(index_value),
                    grey_ancestor(grey_accessibilityID(
                        kFormInputAccessoryViewAccessibilityID)),
                    grey_interactable(), nil);
}

// Get the top presented view controller, in this case the bottom sheet view
// controller.
UIViewController* TopPresentedViewController() {
  UIViewController* topController =
      chrome_test_util::GetAnyKeyWindow().rootViewController;
  for (UIViewController* controller = [topController presentedViewController];
       controller && ![controller isBeingDismissed];
       controller = [controller presentedViewController]) {
    topController = controller;
  }
  return topController;
}

// Verifies the number of Password Details visits recorded.
void CheckPasswordDetailsVisitMetricCount(int count) {
  // Check password details visit metric.
  NSError* error = [MetricsAppInterface
      expectTotalCount:count
          forHistogram:
              @(password_manager::kPasswordManagerSurfaceVisitHistogramName)];
  chrome_test_util::GREYAssertErrorNil(error);

  error = [MetricsAppInterface
       expectCount:count
         forBucket:static_cast<int>(password_manager::PasswordManagerSurface::
                                        kPasswordDetails)
      forHistogram:
          @(password_manager::kPasswordManagerSurfaceVisitHistogramName)];
  chrome_test_util::GREYAssertErrorNil(error);
}

// Verifies that the number of accepted suggestions recorded for the given
// `suggestion_index` is as expected. `is_unique` indicates whether the bucket
// count we're verifying should be unique or not.
void CheckAutofillSuggestionAcceptedIndexMetricsCount(
    NSInteger suggestion_index,
    bool is_unique = true) {
  NSString* histogram =
      @"Autofill.UserAcceptedSuggestionAtIndex.Password.BottomSheet";
  NSString* error_message = @"Unexpected histogram count for bottom sheet "
                            @"accepted password suggestion index.";

  if (is_unique) {
    GREYAssertNil(
        [MetricsAppInterface expectUniqueSampleWithCount:1
                                               forBucket:suggestion_index
                                            forHistogram:histogram],
        error_message);
  } else {
    GREYAssertNil([MetricsAppInterface expectCount:1
                                         forBucket:suggestion_index
                                      forHistogram:histogram],
                  error_message);
  }
}

// Checks that the number of stored credentials is as expected.
void CheckNumberOfStoredCredentials(int expected_count) {
  int credentials_count = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(expected_count, credentials_count,
                  @"Wrong number of stored credentials.");
}

// Waits for the element associated with `matcher` to appear. Then taps on it.
void TapElementOnceVisible(id<GREYMatcher> matcher) {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:matcher];
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Waits for the element associated with `matcher` to appear. Then long presses
// it.
void LongPressElementOnceVisible(id<GREYMatcher> matcher) {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:matcher];
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_longPress()];
}

// Verifies that the keyboard accessory displays chips for both the passkey and
// the password, with the passkey listed first.
void VerifyKeyboardAccessoryChips(NSString* username,
                                  NSString* passkey_user_display_name) {
  NSString* indexFirstOfTwo = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_SUGGESTION_INDEX_VALUE, u"1", u"2");
  NSString* indexSecondOfTwo = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_SUGGESTION_INDEX_VALUE, u"2", u"2");

  id<GREYMatcher> passkeyChip = KeyboardAccessoryPasskeySuggestion(
      passkey_user_display_name, indexFirstOfTwo);
  id<GREYMatcher> passwordChip =
      KeyboardAccessoryPasswordSuggestionOnConditionalLogin(username,
                                                            indexSecondOfTwo);

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:passkeyChip];

  // Scroll to the right of the keyboard accessory so that the password
  // suggestion is visible on smaller devices.
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:passwordChip];
}

// Opens the manual fill view and verifies that the action buttons say "Sign in"
// instead of "Autofill form".
void VerifyManualFillShowsSignInActionButton(
    NSString* passkey_user_display_name) {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      manual_fill::KeyboardAccessoryManualFillButton()];
  [[EarlGrey
      selectElementWithMatcher:manual_fill::KeyboardAccessoryManualFillButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      manual_fill::ExpandedManualFillView()];

  NSString* cell1Index = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_IOS_MANUAL_FALLBACK_PASSWORD_CELL_INDEX),
          "count", 2, "position", 1));
  NSString* cell2Index = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_IOS_MANUAL_FALLBACK_PASSWORD_CELL_INDEX),
          "count", 2, "position", 2));

  NSString* passkeySignInButtonLabel = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_SIGN_IN_BUTTON_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(
          [NSString stringWithFormat:@"%@, localhost\nPasskey • %@", cell1Index,
                                     passkey_user_display_name]));
  NSString* passwordSignInButtonLabel = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_SIGN_IN_BUTTON_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(
          [NSString stringWithFormat:@"%@, localhost\nPassword", cell2Index]));
  NSString* passkeyAutofillFormButtonLabel = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_AUTOFILL_FORM_BUTTON_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(
          [NSString stringWithFormat:@"%@, localhost\nPasskey • %@", cell1Index,
                                     passkey_user_display_name]));

  id<GREYMatcher> passkeySignInButton =
      grey_allOf(grey_accessibilityID(
                     manual_fill::kExpandedManualFillAutofillFormButtonID),
                 grey_accessibilityLabel(passkeySignInButtonLabel), nil);
  id<GREYMatcher> passwordSignInButton =
      grey_allOf(grey_accessibilityID(
                     manual_fill::kExpandedManualFillAutofillFormButtonID),
                 grey_accessibilityLabel(passwordSignInButtonLabel), nil);
  id<GREYMatcher> autofillFormButton =
      grey_allOf(grey_accessibilityID(
                     manual_fill::kExpandedManualFillAutofillFormButtonID),
                 grey_accessibilityLabel(passkeyAutofillFormButtonLabel), nil);

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:passkeySignInButton];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:passwordSignInButton];
  [[EarlGrey selectElementWithMatcher:autofillFormButton]
      assertWithMatcher:grey_nil()];
}

}  // namespace

@interface CredentialSuggestionBottomSheetEGTest : ChromeTestCase

- (BOOL)useNewBlur;

@end

@implementation CredentialSuggestionBottomSheetEGTest {
  BOOL _hasSignedInForTest;
}

- (bool)useNewBlur {
  return NO;
}

- (void)setUp {
  [super setUp];
  _hasSignedInForTest = NO;

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  self.testServer->ServeFilesFromSourceDirectory(
      "components/test/data/password_manager");
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Also reset the dismiss count pref to 0 to make sure the bottom sheet is
  // enabled by default.
  [CredentialSuggestionBottomSheetAppInterface setDismissCount:0];

  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];

  // Set up reauth module.
  [CredentialSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];

  // Make sure the fake passkey keychain provider bridge is set.
  [IOSChromePasskeyClientAppInterface setUpFakePasskeyKeychainProviderBridge];
}

- (void)tearDownHelper {
  GREYAssertTrue([PasswordManagerAppInterface clearCredentials],
                 @"Clearing credentials wasn't done.");
  [PasswordSettingsAppInterface clearPasskeyStore];

  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;

  if ([self
          isRunningTest:
              @selector(
                  testOpenCredentialBottomSheetUsePasswordOnConditionalLogin)] ||
      [self
          isRunningTest:
              @selector(
                  testKeyboardAccessoryDisplaysPasskeyAndPasswordOnConditionalLogin)] ||
      [self
          isRunningTest:
              @selector(
                  testKeyboardAccessoryDisplaysPasskeyAndPasswordNoBottomSheetOnConditionalLogin)]) {
    config.features_enabled.push_back(kIOSPasskeyConditionalLoginWithShim);
  }

  if ([self isRunningTest:@selector
            (testOpenCredentialBottomSheetAndUsePasskeyOnModalLogin)]) {
    config.features_enabled.push_back(kIOSPasskeyModalLoginWithShim);
  }

  if ([self useNewBlur]) {
    config.features_enabled.push_back(kAutofillBottomSheetNewBlur);
  } else {
    config.features_disabled.push_back(kAutofillBottomSheetNewBlur);
  }

  if ([self isRunningTest:@selector(DISABLED_testAutoSubmission)]) {
    config.features_enabled.push_back(
        password_manager::features::kIOSPasswordAutoSubmission);
  } else {
    config.features_disabled.push_back(
        password_manager::features::kIOSPasswordAutoSubmission);
  }

  return config;
}

#pragma mark - Helper methods

// Returns the GURL for the simple login page.
- (GURL)loginPageURL {
  return self.testServer->GetURL("/simple_login_form_empty.html");
}

// Returns the GURL for the simple login autofocus page.
- (GURL)loginAutofocusPageURL {
  return self.testServer->GetURL("/simple_login_form_empty_autofocus.html");
}

// Returns the GURL for the simple conditional passkey login page. This is a
// page that accepts both passkeys and passwords.
- (GURL)conditionalPasskeyLoginPageURL {
  return self.testServer->GetURL(
      kLocalhost, "/simple_login_form_empty_passkey_conditional.html");
}

// Returns the GURL for the simple modal passkey login page.
- (GURL)modalPasskeyLoginPageURL {
  return self.testServer->GetURL(kLocalhost,
                                 "/simple_login_form_empty_passkey_modal.html");
}

// Loads simple page on localhost.
- (void)loadLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:[self loginPageURL]];
  // Sign in after loading the page to prevent EarlGrey timeouts caused by
  // asynchronous UI updates (like the "Signed in as..." snackbar) overlapping
  // with the page load.
  if (!_hasSignedInForTest) {
    [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
    _hasSignedInForTest = YES;
  }
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:WebViewMatcher()];
}

- (void)loadLoginAutofocusPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:[self loginAutofocusPageURL]];
  // Sign in after loading the page to prevent EarlGrey timeouts caused by
  // asynchronous UI updates (like the "Signed in as..." snackbar) overlapping
  // with the page load.
  if (!_hasSignedInForTest) {
    [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
    _hasSignedInForTest = YES;
  }
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
}

- (void)loadConditionalPasskeyLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:[self conditionalPasskeyLoginPageURL]];
  // Sign in after loading the page to prevent EarlGrey timeouts caused by
  // asynchronous UI updates (like the "Signed in as..." snackbar) overlapping
  // with the page load.
  if (!_hasSignedInForTest) {
    [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
    _hasSignedInForTest = YES;
  }
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:WebViewMatcher()];
}

- (void)loadModalPasskeyLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:[self modalPasskeyLoginPageURL]];
  // Sign in after loading the page to prevent EarlGrey timeouts caused by
  // asynchronous UI updates (like the "Signed in as..." snackbar) overlapping
  // with the page load.
  if (!_hasSignedInForTest) {
    [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
    _hasSignedInForTest = YES;
  }
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:WebViewMatcher()];
}

// Saves a generic password (i.e., without special arguments) to the store and
// loads the simple login page.
- (void)saveGenericPasswordAndLoadLoginPage {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL([self loginPageURL])];
  [self loadLoginPage];
}

// Saves a generic password (i.e., without special arguments) to the store and
// loads the simple login autofocus page.
- (void)saveGenericPasswordAndLoadLoginAutofocusPage {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(
                                      [self loginAutofocusPageURL])];
  [self loadLoginAutofocusPage];
}

// Saves a credential with a defined backup password to the store and loads the
// simple login page.
- (void)savePasswordWithBackupAndLoadLoginPage {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL([self loginPageURL])
                           shared:NO
                   backupPassword:@"backup password"];
  [self loadLoginPage];
}

- (void)verifyPasswordFieldsHaveBeenFilled:(NSString*)username {
  // Verify that the username has been filled.
  // The test pages use either 'un' or 'username' as the ID for the username
  // field.
  NSString* condition = [NSString
      stringWithFormat:@"document.getElementById('%s')?.value === '%@' || "
                       @"document.getElementById('%s')?.value === '%@'",
                       kFormUsernameId1, username, kFormUsernameId2, username];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];

  // Verify that the password field is not empty.
  NSString* filledFieldCondition =
      [NSString stringWithFormat:@"!!document.getElementById('%s')?.value || "
                                 @"!!document.getElementById('%s')?.value",
                                 kFormPasswordId1, kFormPasswordId2];
  [ChromeEarlGrey waitForJavaScriptCondition:filledFieldCondition];
}

#pragma mark - Tests

- (void)testOpenCredentialBottomSheetUsePassword {
  GURL URL = [self loginPageURL];
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(URL)];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  // Verify that the subtitle string appears.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SubtitleString(URL)];

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  // No histogram logged because there is only 1 credential shown to the user.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"PasswordManager.TouchToFill.CredentialIndex"],
      @"Unexpected histogram error for touch to fill credential index");

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/0);

  [self verifyPasswordFieldsHaveBeenFilled:@"user"];
}

// Tests that accepting suggestions from the sheet works when the stateless
// fill data flow feature is enabled.
- (void)testOpenCredentialBottomSheetUsePassword_StatelessFillDataFlow {
  [self saveGenericPasswordAndLoadLoginPage];

  // Wait a bit to let things settle. Waiting on content to be loaded on the
  // page isn't 100% reliable as trying to interact with that content at that
  // moment doesn't always work.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  // Verify that the subtitle string appears.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SubtitleString([self loginPageURL])];

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  // No histogram logged because there is only 1 credential shown to the user.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"PasswordManager.TouchToFill.CredentialIndex"],
      @"Unexpected histogram error for touch to fill credential index");

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/0);

  [self verifyPasswordFieldsHaveBeenFilled:@"user"];
}

// Tests that the bottom sheet can be used with the new blur.
- (void)testOpenCredentialBottomSheetUsePasswordWithNewBlur {
  GURL URL = [self loginPageURL];
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(URL)];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  // Verify that the subtitle string appears.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SubtitleString(URL)];

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  // No histogram logged because there is only 1 credential shown to the user.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"PasswordManager.TouchToFill.CredentialIndex"],
      @"Unexpected histogram error for touch to fill credential index");

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/0);

  [self verifyPasswordFieldsHaveBeenFilled:@"user"];
}

// This test verifies that the bottom sheet opens on autofocus events.
- (void)testOpenPasswordBottomOnAutofocus {
  [self saveGenericPasswordAndLoadLoginAutofocusPage];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:UsePasswordButton()];
}

// This test verifies that the credential bottom sheet does not open when the
// webpage has enabled passkey login.
- (void)testOpenKeyboardOnPasskey {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(
                                      [self conditionalPasskeyLoginPageURL])];

  [self loadConditionalPasskeyLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  // If the bottom sheet is presented (e.g. if the passkey conditional login
  // flag is enabled), dismiss it to show the keyboard.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:OpenKeyboardButton()]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  if (!error) {
    [[EarlGrey selectElementWithMatcher:OpenKeyboardButton()]
        performAction:grey_tap()];
  }

  [ChromeEarlGrey waitForKeyboardToAppear];
}

// Tests that a password from the credential bottom sheet can be used on a
// webpage where conditional passkey login is enabled.
- (void)testOpenCredentialBottomSheetUsePasswordOnConditionalLogin {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(
                                      [self conditionalPasskeyLoginPageURL])];

  [self loadConditionalPasskeyLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SubtitleWithPasskeysString([self
                                              conditionalPasskeyLoginPageURL])];

  [[EarlGrey selectElementWithMatcher:ContinueButton()]
      performAction:grey_tap()];

  [self verifyPasswordFieldsHaveBeenFilled:@"user"];
}

// Tests that the autofill keyboard accessory bar displays chips for both the
// passkey and the password, with the passkey listed first, when both passwords
// and passkeys can be presented and the bottom sheet is dismissed.
- (void)testKeyboardAccessoryDisplaysPasskeyAndPasswordOnConditionalLogin {
  SaveExamplePasskeyToStore(/*rpId=*/base::SysUTF8ToNSString(kLocalhost));
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(
                                      [self conditionalPasskeyLoginPageURL])];

  [self loadConditionalPasskeyLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kDefaultUserDisplayName)];

  [[EarlGrey selectElementWithMatcher:OpenKeyboardButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForKeyboardToAppear];

  VerifyKeyboardAccessoryChips(@"user", kDefaultUserDisplayName);
  VerifyManualFillShowsSignInActionButton(kDefaultUserDisplayName);
}

// Tests that when the credential bottom sheet is disabled, focusing an input
// field on a conditional passkey login page directly displays chips on the
// keyboard accessory bar for both the passkey and the password, with the
// passkey listed first.
- (void)
    testKeyboardAccessoryDisplaysPasskeyAndPasswordNoBottomSheetOnConditionalLogin {
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  SaveExamplePasskeyToStore(/*rpId=*/base::SysUTF8ToNSString(kLocalhost));
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(
                                      [self conditionalPasskeyLoginPageURL])];

  [self loadConditionalPasskeyLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey waitForKeyboardToAppear];

  VerifyKeyboardAccessoryChips(@"user", kDefaultUserDisplayName);
  VerifyManualFillShowsSignInActionButton(kDefaultUserDisplayName);
}

// Tests using a passkey from the bottom sheet in a modal login context.
- (void)testOpenCredentialBottomSheetAndUsePasskeyOnModalLogin {
  SaveExamplePasskeyToStore(/*rpId=*/base::SysUTF8ToNSString(kLocalhost));

  [self loadModalPasskeyLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kDefaultUserDisplayName)];

  [[EarlGrey selectElementWithMatcher:ContinueButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:grey_accessibilityID(
                                                 kDefaultUserDisplayName)];

  // TODO(crbug.com/460486744): See if there's a way to validate that the
  // passkey usage was successful.
}

// This test will allow us to know if we're using a coherent browser state to
// open the bottom sheet in incognito mode.
- (void)testOpenCredentialBottomSheetUsePasswordIncognito {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL([self loginPageURL])];

  [ChromeEarlGrey openNewIncognitoTab];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  [self verifyPasswordFieldsHaveBeenFilled:@"user"];
}

// Tests that showing the keyboard from the bottom sheet works.
- (void)testOpenCredentialBottomSheetTapUseKeyboardShowKeyboard {
  [self saveGenericPasswordAndLoadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:OpenKeyboardButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForKeyboardToAppear];
}

- (void)testOpenCredentialBottomSheetOpenPasswordManager {
  NSURL* URL = net::NSURLWithGURL([self loginPageURL]);
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user"
                                                  password:@"password"
                                                       URL:URL];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL];
  CheckNumberOfStoredCredentials(/*expected_count=*/2);

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  TapElementOnceVisible(grey_accessibilityID(@"user"));

  // Long press to open context menu.
  LongPressElementOnceVisible(grey_accessibilityID(@"user2"));

  [ChromeEarlGreyUI waitForAppToIdle];

  // Mock local authentication result needed for opening the password manager.
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:PasswordManagerContextMenuItem()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  NSString* origin =
      [NSString stringWithFormat:@"http://%@:%@", [URL host], [URL port]];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ButtonWithAccessibilityID([NSString
                                       stringWithFormat:@"%@, 2 accounts",
                                                        origin]),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

- (void)testOpenCredentialBottomSheetOpenPasswordDetails {
  NSURL* URL = net::NSURLWithGURL([self loginPageURL]);
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user"
                                                  password:@"password"
                                                       URL:URL];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL];
  CheckNumberOfStoredCredentials(/*expected_count=*/2);

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  TapElementOnceVisible(grey_accessibilityID(@"user"));

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Delay the auth result to be able to validate that password details is
  // not visible until the result is emitted.
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  [ReauthenticationAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  // Long press to open context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ShowDetailsContextMenuItem()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Password details shouldn't be visible until auth is passed.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME)]
      assertWithMatcher:grey_notVisible()];

  // Verify visit metric was not recorded yet.
  CheckPasswordDetailsVisitMetricCount(0);

  // Emit auth result so password details surface is revealed.
  [ReauthenticationAppInterface mockReauthenticationModuleReturnMockedResult];

  id<GREYMatcher> usernameCellMatcher =
      chrome_test_util::TextFieldForCellWithLabelId(
          IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);

  // Check that the username cell is displayed.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:usernameCellMatcher]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  NSString* errorMessage =
      @"There is no password view with a username table view cell";
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             errorMessage);
  [[EarlGrey selectElementWithMatcher:usernameCellMatcher]
      assertWithMatcher:grey_textFieldValue(@"user2")];

  // Verify visit metric was recorded.
  CheckPasswordDetailsVisitMetricCount(1);
}

// Verifies that Password Details is not revealed when local authentication
// fails.
- (void)
    testOpenCredentialBottomSheetOpenPasswordDetailsWithFailedAuthentication {
  NSURL* URL = net::NSURLWithGURL([self loginPageURL]);
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user"
                                                  password:@"password"
                                                       URL:URL];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL];
  CheckNumberOfStoredCredentials(/*expected_count=*/2);

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  TapElementOnceVisible(grey_accessibilityID(@"user"));

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Delay the auth result to be able to validate that password details is
  // not visible until the result is emitted.
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [ReauthenticationAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  // Long press to open context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ShowDetailsContextMenuItem()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Password details shouldn't be visible until auth is passed.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsNavigationBar()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify visit metric was not recorded yet.
  CheckPasswordDetailsVisitMetricCount(0);

  // Emit auth result so password details surface is dismissed due to failed
  // auth.
  [ReauthenticationAppInterface mockReauthenticationModuleReturnMockedResult];

  // Validate the whole settings UI is gone.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsNavigationBar()]
      assertWithMatcher:grey_nil()];

  // Verify visit metric was not recorded.
  CheckPasswordDetailsVisitMetricCount(0);
}

- (void)testOpenCredentialBottomSheetDeletePassword {
  GURL loginURL = [self loginPageURL];
  NSURL* storeURL = net::NSURLWithGURL(loginURL);
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user"
                                                  password:@"password"
                                                       URL:storeURL];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:storeURL];
  CheckNumberOfStoredCredentials(/*expected_count=*/2);

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  TapElementOnceVisible(grey_accessibilityID(@"user"));

  // Long press to open context menu.
  LongPressElementOnceVisible(grey_accessibilityID(@"user2"));

  [ChromeEarlGreyUI waitForAppToIdle];

  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:ShowDetailsContextMenuItem()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  NSString* website = [storeURL.absoluteString
      stringByReplacingOccurrencesOfString:@"simple_login_form_empty.html"
                                withString:@""];
  DeleteCredential(@"user2", website);

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kPasswordDetailsViewControllerID)];

  // Verify that user2 is not available anymore.
  // Blur the active element to ensure that the next tap triggers a fresh focus
  // event.
  [ChromeEarlGrey
      evaluateJavaScriptForSideEffect:@"document.activeElement?.blur()"];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey waitForKeyboardToAppear];
  // Since the bottom sheet was dismissed, now suggestions are shown in the
  // keyboard acessory.
  NSString* accessorySuggestionURL =
      base::SysUTF8ToNSString(loginURL.GetHost() + ":" + loginURL.GetPort());
  NSString* passwordSubtext = l10n_util::GetNSString(IDS_IOS_PASSWORD_SUBTEXT);

  NSString* labelUserWithURL =
      [@"user, " stringByAppendingString:accessorySuggestionURL];
  NSString* labelUserWithPassword =
      [NSString stringWithFormat:@"user, %@", passwordSubtext];
  id<GREYMatcher> userSuggestionMatcher =
      grey_anyOf(grey_accessibilityLabel(labelUserWithURL),
                 grey_accessibilityLabel(labelUserWithPassword), nil);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:userSuggestionMatcher];

  NSString* labelUser2WithURL =
      [@"user2, " stringByAppendingString:accessorySuggestionURL];
  NSString* labelUser2WithPassword =
      [NSString stringWithFormat:@"user2, %@", passwordSubtext];
  id<GREYMatcher> user2SuggestionMatcher =
      grey_anyOf(grey_accessibilityLabel(labelUser2WithURL),
                 grey_accessibilityLabel(labelUser2WithPassword), nil);
  [[EarlGrey selectElementWithMatcher:user2SuggestionMatcher]
      assertWithMatcher:grey_nil()];
}

- (void)testOpenCredentialBottomSheetSelectPassword {
  NSURL* URL = net::NSURLWithGURL([self loginPageURL]);
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user"
                                                  password:@"password"
                                                       URL:URL];
  CheckNumberOfStoredCredentials(/*expected_count=*/1);

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  // Tapping the single item doesn't change anything.
  TapElementOnceVisible(grey_accessibilityID(@"user"));

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/0);

  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL];
  CheckNumberOfStoredCredentials(/*expected_count=*/2);

  // Reload the page, now with 2 credentials.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  // Select the first item.
  TapElementOnceVisible(grey_accessibilityID(@"user"));

  // Select the second item.
  TapElementOnceVisible(grey_accessibilityID(@"user2"));

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:1
                         forHistogram:
                             @"PasswordManager.TouchToFill.CredentialIndex"],
      @"Unexpected histogram error for touch to fill credential index");

  // Verify that the acceptance of the password suggestion at index 1 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/1,
                                                   /*is_unique=*/false);

  [self verifyPasswordFieldsHaveBeenFilled:@"user2"];

  GREYWaitForAppToIdle(@"App failed to idle");
}

// TODO(crbug.com/40279461): Fix flaky test & re-enable.
- (void)DISABLED_testOpenCredentialBottomSheetExpand {
  NSURL* URL = net::NSURLWithGURL([self loginPageURL]);
  for (int i = 1; i <= 9; i++) {
    [PasswordManagerAppInterface
        storeCredentialWithUsername:[NSString stringWithFormat:@"user%i", i]
                           password:@"password"
                                URL:URL];
  }
  CheckNumberOfStoredCredentials(/*expected_count=*/9);

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  // Tap to expand.
  TapElementOnceVisible(grey_accessibilityID(@"user1"));

  // Scroll to the last password.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertUnderTitleViewAccessibilityIdentifier)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  TapElementOnceVisible(grey_accessibilityID(@"user9"));

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  [self verifyPasswordFieldsHaveBeenFilled:@"user9"];
}

- (void)testPasswordBottomSheetDismiss3TimesNotShownAnymore {
  // Dismiss #1.
  [self saveGenericPasswordAndLoadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:OpenKeyboardButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForKeyboardToAppear];

  // Dismiss #2.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:OpenKeyboardButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForKeyboardToAppear];

  // Dismiss #3.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:OpenKeyboardButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForKeyboardToAppear];

  // Verify that keyboard is shown.
  [self loadLoginPage];
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];
  [ChromeEarlGrey waitForKeyboardToAppear];
}

// TODO(crbug.com/40279461): Fix flaky test & re-enable.
- (void)DISABLED_testOpenCredentialBottomSheetNoUsername {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@""
                         password:@"password"
                              URL:net::NSURLWithGURL([self loginPageURL])];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(l10n_util::GetNSString(
                          IDS_IOS_CREDENTIAL_BOTTOM_SHEET_NO_USERNAME))];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(l10n_util::GetNSString(
                     IDS_IOS_CREDENTIAL_BOTTOM_SHEET_NO_USERNAME))]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  // Verify that selecting credentials with no username disables the bottom
  // sheet.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey waitForKeyboardToAppear];
}

// Tests that the Credential Bottom Sheet appears when tapping on a password
// related field and that the buttons are still visible after we chang the trait
// collection to larger content size.
- (void)testOpenCredentialBottomSheetUsePasswordAfterTraitCollectionChange {
  [self saveGenericPasswordAndLoadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  // Change trait collection to use accessibility large content size.
  ScopedTraitOverrider overrider(TopPresentedViewController());
  overrider.SetContentSizeCategory(UIContentSizeCategoryAccessibilityLarge);

  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the "Use Password" and "No Thanks" buttons are still visible.
  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:OpenKeyboardButton()]
      assertWithMatcher:grey_notNil()];

  // Verify the credit card tablew view is still visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  [self verifyPasswordFieldsHaveBeenFilled:@"user"];
}

// TODO(crbug.com/361518360): Unflake the test.
- (void)DISABLED_testOpenCredentialBottomSheetWithSingleSharedPassword {
  NSURL* URL = net::NSURLWithGURL([self loginPageURL]);

  // Save 1 password that has been received via sharing and the other not.
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user1"
                                                  password:@"password1"
                                                       URL:URL
                                                    shared:YES];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL
                                                    shared:NO];
  CheckNumberOfStoredCredentials(/*expected_count=*/2);

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user1")];

  // Verify that the sharing notification title is visible.
  id<GREYMatcher> titleMatcher = grey_accessibilityLabel(
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PASSWORD_SHARING_NOTIFICATION_TITLE, 1)));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(titleMatcher,
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Verify that the other password is also accessible to fill.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user1")]
      performAction:grey_swipeSlowInDirection(kGREYDirectionUp)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user1")]
      performAction:grey_tap()];
  TapElementOnceVisible(grey_accessibilityID(@"user2"));
  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];
  [self verifyPasswordFieldsHaveBeenFilled:@"user2"];

  // Verify that after using the shared password regular bottom sheet is
  // displayed.
  [self loadLoginPage];
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user1")];

  // Verify that the sharing notification is not visible anymore.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(titleMatcher,
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];
  [self verifyPasswordFieldsHaveBeenFilled:@"user1"];
}

- (void)testOpenPasswordBottomSheetWithMultipleSharedPasswords {
  NSURL* URL = net::NSURLWithGURL([self loginPageURL]);
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user1"
                                                  password:@"password1"
                                                       URL:URL
                                                    shared:YES];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL
                                                    shared:YES];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user1")];

  // Verify that the sharing notification title is visible.
  id<GREYMatcher> titleMatcher = grey_accessibilityLabel(
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PASSWORD_SHARING_NOTIFICATION_TITLE, 2)));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(titleMatcher,
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user1")]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];
  [self verifyPasswordFieldsHaveBeenFilled:@"user1"];
}

// TODO(crbug.com/361518360): Unflake the test.
- (void)
    DISABLED_testOpenCredentialBottomSheetWithSharedPasswordsAndUseKeyboard {
  // Save a password that has been received via sharing.
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user1"
                         password:@"password1"
                              URL:net::NSURLWithGURL([self loginPageURL])
                           shared:YES];

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user1")];

  // Verify that the sharing notification title is visible.
  id<GREYMatcher> titleMatcher = grey_accessibilityLabel(
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PASSWORD_SHARING_NOTIFICATION_TITLE, 1)));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(titleMatcher,
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:OpenKeyboardButton()]
      performAction:grey_tap()];

  // Verify that after dismissing the shared notification bottom sheet, regular
  // bottom sheet is displayed.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user1")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(titleMatcher,
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  [self verifyPasswordFieldsHaveBeenFilled:@"user1"];
}

// Tests that the bottom sheet isn't displayed when the user uses the omnibox.
- (void)testBottomSheetWithOmnibox {
  // Put a credential in the store so the sheet can trigger, and load the
  // webpage.
  [self saveGenericPasswordAndLoadLoginPage];

  // Display the omnibox UI.
  [ChromeEarlGreyUI focusOmnibox];
  GREYAssertTrue([OmniboxAppInterface isOmniboxFocusedOnMainBrowser],
                 @"IsOmniboxFocused is expected to be true.");

  // While the omnibox UI is being displayed, focus on the webview behind the
  // omnibox UI so the credential bottom sheet would be displayed if the omnibox
  // wasn't handled correctly.
  [ChromeEarlGrey
      evaluateJavaScriptForSideEffect:
          @"document.querySelector('input[type=password]').focus()"];

  // Give some time to the sheet to be displayed if it was to be displayed so we
  // can correctly assess that the credential bottom sheet is indeed not
  // displayed.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2));

  // Verify that the sheet wasn't displayed.
  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that a backup password appears as expected in the bottom sheet and that
// it can be used to fill the form.
- (void)testUseBackupPassword {
  [self savePasswordWithBackupAndLoadLoginPage];

  // Tap on a field to trigger the bottom sheet.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  // Select the backup password and use it to fill the form.
  TapElementOnceVisible(BackupPasswordSuggestion(@"user"));
  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  [self verifyPasswordFieldsHaveBeenFilled:@"user"];

  // Verify that the acceptance of the password suggestion at index 1 was
  // correctly recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/1);
}

// Tests that only the expected options are available in the context menu when
// opened from a backup password suggestion.
- (void)testAvailableContextMenuItemsForBackupPassword {
  [self savePasswordWithBackupAndLoadLoginPage];

  // Tap on a field to trigger the bottom sheet.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPasswordId1)];

  // Long press the backup password suggestion to open the context menu.
  LongPressElementOnceVisible(BackupPasswordSuggestion(@"user"));

  // Verify the availabale options.
  [[EarlGrey selectElementWithMatcher:PasswordManagerContextMenuItem()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ShowDetailsContextMenuItem()]
      assertWithMatcher:grey_nil()];
}

// Tests that a standard, clean login form successfully auto-submits.
// Form Structure:
// [ Username ]
// [ Checkbox ] (Ignored by heuristic)
// [ Password ]
// [ Submit   ]
- (void)DISABLED_testAutoSubmission {
  GURL URL = self.testServer->GetURL("/auto_submit_test.html");

  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(URL)];
  [ChromeEarlGrey loadURL:URL];
  // Sign in after loading the page to prevent EarlGrey timeouts caused by
  // asynchronous UI updates (like the "Signed in as..." snackbar) overlapping
  // with the page load.
  if (!_hasSignedInForTest) {
    [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
    _hasSignedInForTest = YES;
  }

  [ChromeEarlGrey waitForWebStateContainingText:"Auto-Submit Test Page"];

  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("password")];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:UsePasswordButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:"Form Submitted!"];
  using password_manager::SubmissionReadinessState;
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          SubmissionReadinessState::kTwoFields)
                         forHistogram:@"PasswordManager.TouchToFill."
                                      @"SubmissionReadiness"],
      @"Failed to record SubmissionReadiness histogram.");
}

@end
