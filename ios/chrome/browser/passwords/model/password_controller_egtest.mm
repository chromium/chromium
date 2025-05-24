// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import <memory>
#import <optional>

#import "base/check.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/ios/common/features.h"
#import "components/enterprise/connectors/core/realtime_reporting_test_environment.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/plus_addresses/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_prefs.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

constexpr char kFormUsername[] = "un";
constexpr char kFormPassword[] = "pw";

namespace {

NSString* const kPassphrase = @"hello";
constexpr base::TimeDelta kReportUploadTimeout = base::Seconds(15);
constexpr char kEnrollmentToken[] = "fake-enrollment-token";
constexpr char kEnrollmentTokenPolicyName[] = "CloudManagementEnrollmentToken";

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using ::chrome::cros::reporting::proto::Event;
using ::chrome::cros::reporting::proto::PasswordBreachEvent;
using ::chrome::cros::reporting::proto::UploadEventsRequest;
using Identity =
    ::chrome::cros::reporting::proto::PasswordBreachEvent::Identity;
using chrome_test_util::GREYAssertErrorNil;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::TapWebElementWithId;
using chrome_test_util::UseSuggestedPasswordMatcher;
using enterprise_connectors::test::RealtimeReportingTestEnvironment;

using testing::ElementWithAccessibilityLabelSubstring;

id<GREYMatcher> PasswordInfobarLabels(int prompt_id) {
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      ElementWithAccessibilityLabelSubstring(l10n_util::GetNSString(prompt_id)),
      nil);
}

id<GREYMatcher> PasswordInfobarButton(int button_id) {
  return chrome_test_util::ButtonWithAccessibilityLabelId(button_id);
}

id<GREYMatcher> SuggestPasswordChip() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_IOS_SUGGEST_PASSWORD)),
      nil);
}

id<GREYMatcher> PasswordBreachMatcher() {
  return grey_accessibilityID(kPasswordBreachViewAccessibilityIdentifier);
}

// Simulates a keyboard event where a character is typed.
void SimulateKeyboardEvent(NSString* letter) {
  if ([letter isEqual:@"@"]) {
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:letter
                                            flags:UIKeyModifierShift];
    return;
  }

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:letter flags:0];
}

// Simulates typing text on the keyboard and avoid having the first character
// typed uppercased.
//
// TODO(crbug.com/40916974): This should be replaced by grey_typeText when
// fixed.
void TypeText(NSString* nsText) {
  std::string text = base::SysNSStringToUTF8(nsText);
  for (size_t i = 0; i < text.size(); ++i) {
    // Type each character in the provided text.
    NSString* letter = base::SysUTF8ToNSString(text.substr(i, 1));
    SimulateKeyboardEvent(letter);
    if (i == 0) {
      // Undo and retype the first letter to not have it uppercased.
      [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"z"
                                              flags:UIKeyModifierCommand];
      SimulateKeyboardEvent(letter);
    }
  }
}

// Waits for the bottom sheet and then re-opens the keyboard from there.
void WaitForBottomSheetAndOpenKeyboard(NSString* username) {
  id<GREYMatcher> buttonMatcher =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD);
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(username)];
  [[EarlGrey selectElementWithMatcher:buttonMatcher] performAction:grey_tap()];
  [ChromeEarlGrey waitForKeyboardToAppear];
}

// Types `text` on an input field with `fieldID`. Dismisses the password bottom
// sheet if `dismissBottomSheet` is true.
void TypeTextOnField(NSString* text,
                     const std::string& fieldID,
                     bool dismissBottomSheet = false) {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(fieldID)];
  if (dismissBottomSheet) {
    WaitForBottomSheetAndOpenKeyboard(text);
  }
  TypeText(text);
}

// Types the username and password on the UFF forms.
void TypeUsernameAndPasswordOnUFF(NSString* username,
                                  NSString* password,
                                  bool dismissBottomSheetOnUsername = false) {
  // Type username and dismiss the bottom sheet because it is the first login
  // field to be focused on, which triggers the password bottom sheet. Once
  // dismissed the bottom sheet isn't shown again when focusing on other login
  // fields, as long as the page isn't reloaded.
  TypeTextOnField(username, "single_un", dismissBottomSheetOnUsername);
  TypeTextOnField(password, "single_pw");
}

// Taps on the login button in UFF for logging in.
void LoginOnUff() {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("login_btn")];
}

}  // namespace

@interface PasswordControllerEGTest : WebHttpServerChromeTestCase
@end

@implementation PasswordControllerEGTest {
  std::unique_ptr<RealtimeReportingTestEnvironment> _reportingEnvironment;
}

- (void)setUp {
  std::optional<std::string_view> maybe_event =
      [self enterpriseReportingEventForTest];
  if (maybe_event) {
    // Start the servers before calling the superclass's `-setUp` so that their
    // addresses can be added to the app launch config. `GREYAssertTrue` can
    // only be used after calling the superclass's `-setUp`, so use `CHECK()`
    // instead.
    _reportingEnvironment = RealtimeReportingTestEnvironment::Create(
        /*enabled_event_names=*/{std::string(*maybe_event)},
        /*enabled_opt_in_events=*/{{std::string(*maybe_event), {"*"}}});
    CHECK(_reportingEnvironment);
    CHECK(_reportingEnvironment->Start());
  }

  // This call launches the application and will wait for the profile to be
  // initialized correctly (possibly with Enterprise policies).
  [super setUp];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Also reset the dismiss count pref to 0 to make sure the bottom sheet is
  // enabled by default.
  [PasswordSuggestionBottomSheetAppInterface setDismissCount:0];

  // Clear credentials and autofill profile before starting the test in case
  // there are some left over from a previous test case.
  GREYAssertTrue([PasswordManagerAppInterface clearCredentials],
                 @"Clearing credentials wasn't done.");
  [AutofillAppInterface clearProfilesStore];

  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
}

- (void)tearDownHelper {
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);

  GREYAssertTrue([PasswordManagerAppInterface clearCredentials],
                 @"Clearing credentials wasn't done.");
  [AutofillAppInterface clearProfilesStore];
  [PasswordSuggestionBottomSheetAppInterface setDismissCount:0];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector(testStickySavePromptJourney)]) {
    config.features_enabled.push_back(kAutofillStickyInfobarIos);
  }

  // Set Enterprise features for testing password-related event reporting. The
  // policy and reporting servers must be started by this point.
  if ([self enterpriseReportingEventForTest]) {
    CHECK(_reportingEnvironment);
    std::vector<std::string> reporting_args =
        _reportingEnvironment->GetArguments();
    config.additional_args.insert(config.additional_args.end(),
                                  reporting_args.begin(), reporting_args.end());
    config.additional_args.push_back(base::StrCat(
        {"-", base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey)}));
    config.additional_args.push_back(
        base::StrCat({"<dict><key>", kEnrollmentTokenPolicyName,
                      "</key><string>", kEnrollmentToken, "</string></dict>"}));
    config.relaunch_policy = ForceRelaunchByKilling;
  }

  if ([self isRunningTest:@selector(testPasswordBreachEventReported)]) {
    config.features_enabled.push_back(
        password_manager::features::kMarkAllCredentialsAsLeaked);
  }

  // The proactive password suggestion bottom sheet isn't tested here, it
  // is tested in its own suite in password_suggestion_egtest.mm.
  config.features_disabled.push_back(
      password_manager::features::kIOSProactivePasswordGenerationBottomSheet);
  // The tests are incompatible with the feature.
  config.features_disabled.push_back(
      plus_addresses::features::kPlusAddressesEnabled);
  return config;
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
}

- (void)loadUFFLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/uff_login_forms.html")];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Step 1, Single username form."];
}

- (std::optional<std::string_view>)enterpriseReportingEventForTest {
  if ([self isRunningTest:@selector(testLoginEventReported)]) {
    return "loginEvent";
  } else if ([self isRunningTest:@selector(testPasswordBreachEventReported)]) {
    return "passwordBreachEvent";
  }
  return std::nullopt;
}

- (void)waitForEnterpriseReports:(int)count {
  // Use metrics to detect that the report upload completed. This is the best
  // known way to wait because a task environment isn't available here, so
  // there's nothing for the reporting server to post to when the request
  // arrives. This also precludes helpers like `base::RunLoop` or
  // `net::test_server::ControllableHttpResponse` that require such an
  // environment.
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          kReportUploadTimeout,
          ^{
            NSError* error = [MetricsAppInterface
                expectTotalCount:count
                    forHistogram:@"Enterprise.ReportingEventUploadSuccess"];
            return error == nil;
          }),
      @"Timed out uploading security event.");
  GREYAssertErrorNil([MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"Enterprise.ReportingEventUploadFailure"]);
}

#pragma mark - Tests

// Tests that save password prompt is shown on new login.
- (void)testSavePromptAppearsOnFormSubmission {
  [self loadLoginPage];

  // Simulate user interacting with fields.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormUsername)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  // Wait until the save password prompt becomes visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)];

  [[EarlGrey selectElementWithMatcher:PasswordInfobarButton(
                                          IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON)]
      performAction:grey_tap()];

  // Wait until the save password infobar disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)];

  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of stored credentials.");
}

// Tests that update password prompt is shown on submitting the new password
// for an already stored login.
- (void)testUpdatePromptAppearsOnFormSubmission {
  // Load the page the first time an store credentials.
  [self loadLoginPage];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"Eguser"
                                                  password:@"OldPass"];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of initial credentials.");

  // Load the page again and have a new password value to save.
  [self loadLoginPage];
  // Simulate user interacting with fields.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormUsername)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  // Wait until the update password prompt becomes visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD)];

  [[EarlGrey
      selectElementWithMatcher:PasswordInfobarButton(
                                   IDS_IOS_PASSWORD_MANAGER_UPDATE_BUTTON)]
      performAction:grey_tap()];

  // Wait until the update password infobar disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD)];

  credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of final credentials.");
}

// Tests that update password prompt is shown on submitting the new password
// while signed in, for an already stored credential in local store.
- (void)testUpdateLocalPasswordPromptOnFormSubmissionWhileSignedIn {
  // Load the page the first time an store credentials locally.
  [self loadLoginPage];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"Eguser"
                                                  password:@"OldPass"];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of initial credentials.");

  // Sign in with identity where the credential still lives in the local store.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Load the page again and have a new password value to save.
  [self loadLoginPage];
  // Emulate user interacting with fields.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormUsername)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  // Wait until the update password prompt becomes visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD)];

  // Verify the update subtitle describes a local update as the password was
  // stored locally.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_PASSWORD_MANAGER_LOCAL_SAVE_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:PasswordInfobarButton(
                                   IDS_IOS_PASSWORD_MANAGER_UPDATE_BUTTON)]
      performAction:grey_tap()];

  // Wait until the update password infobar disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD)];

  credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of final credentials.");
}

// Tests the sticky password prompt journey where the prompt remains there when
// navigating without an explicit user gesture, and then the prompt is dismissed
// when navigating with a user gesture. Test with the password save prompt but
// the type of password prompt doesn't matter in this test case.
- (void)testStickySavePromptJourney {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];

  // Emulate user interacting with fields.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormUsername)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  // Wait until the save password prompt becomes visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)];

  {
    // Reloading page from script shouldn't dismiss the infobar.
    NSString* script = @"location.reload();";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Assigning url from script to the page aka open an url shouldn't dismiss
    // the infobar.
    NSString* script = @"window.location.assign(window.location.href);";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Pushing new history entry without reloading content shouldn't dismiss the
    // infobar.
    NSString* script = @"history.pushState({}, '', 'destination2.html');";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Replacing history entry without reloading content shouldn't dismiss the
    // infobar.
    NSString* script = @"history.replaceState({}, '', 'destination3.html');";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }

  // Wait some time for things to settle.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));

  // Verify that the prompt is still there after the non-user initiated
  // navigations.
  [[EarlGrey
      selectElementWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Navigate with an emulated user gesture.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];

  // Verify that the infobar is dismissed.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD)];
}

// Tests password generation flow.
// TODO(crbug.com/40260214): The test fails on simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testPasswordGeneration FLAKY_testPasswordGeneration
#else
#define MAYBE_testPasswordGeneration testPasswordGeneration
#endif
- (void)MAYBE_testPasswordGeneration {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_signup_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Signup form."];

  // Verify that the target field is empty.
  NSString* emptyFieldCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value === ''",
                                 kFormPassword];
  [ChromeEarlGrey waitForJavaScriptCondition:emptyFieldCondition];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormPassword)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Tap on a suggest password chip.
  [[EarlGrey selectElementWithMatcher:SuggestPasswordChip()]
      performAction:grey_tap()];

  // Confirm by tapping on the 'Use Suggested Password' button.
  [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordMatcher()]
      performAction:grey_tap()];

  // Verify that the target field is not empty.
  NSString* filledFieldCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value !== ''",
                                 kFormPassword];
  [ChromeEarlGrey waitForJavaScriptCondition:filledFieldCondition];
}

// Tests that password generation is offered for signed in users.
- (void)testPasswordGenerationForSignedInAccount {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_signup_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Signup form."];

  // Verify that the target field is empty.
  NSString* emptyFieldCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value === ''",
                                 kFormPassword];
  [ChromeEarlGrey waitForJavaScriptCondition:emptyFieldCondition];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormPassword)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Verify the suggest password chip is shown.
  [[EarlGrey selectElementWithMatcher:SuggestPasswordChip()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on a suggest password chip.
  [[EarlGrey selectElementWithMatcher:SuggestPasswordChip()]
      performAction:grey_tap()];

  // Confirm by tapping on the 'Use Suggested Password' button.
  [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordMatcher()]
      performAction:grey_tap()];
}

// Tests that password generation is not offered for signed in users with
// passwords toggle disabled.
// TODO(crbug.com/371189341): Test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testPasswordGenerationWhileSignedInWithPasswordsDisabled \
  testPasswordGenerationWhileSignedInWithPasswordsDisabled
#else
#define MAYBE_testPasswordGenerationWhileSignedInWithPasswordsDisabled \
  DISABLED_testPasswordGenerationWhileSignedInWithPasswordsDisabled
#endif
- (void)MAYBE_testPasswordGenerationWhileSignedInWithPasswordsDisabled {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Disable Passwords toggle in account settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncPasswordsIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/NO)];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_signup_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Signup form."];

  // Verify that the target field is empty.
  NSString* emptyFieldCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value === ''",
                                 kFormPassword];
  [ChromeEarlGrey waitForJavaScriptCondition:emptyFieldCondition];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormPassword)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Verify the suggest password chip is not shown.
  [[EarlGrey selectElementWithMatcher:SuggestPasswordChip()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that password generation is not offered for signed in users with an
// encryption error; missing passphrase.
// TODO(crbug.com/371189341): Test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testPasswordGenerationWhileSignedInWithError \
  testPasswordGenerationWhileSignedInWithError
#else
#define MAYBE_testPasswordGenerationWhileSignedInWithError \
  DISABLED_testPasswordGenerationWhileSignedInWithError
#endif
- (void)MAYBE_testPasswordGenerationWhileSignedInWithError {
  // Encrypt synced data with a passphrase to enable passphrase encryption for
  // the signed in account.
  [ChromeEarlGrey addSyncPassphrase:kPassphrase];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Verify encryption error is showing in in account settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  // Verify the error section is showing.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncErrorButtonIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_signup_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Signup form."];

  // Verify that the target field is empty.
  NSString* emptyFieldCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value === ''",
                                 kFormPassword];
  [ChromeEarlGrey waitForJavaScriptCondition:emptyFieldCondition];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormPassword)];

  // Wait for the accessory icon to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Verify the suggest password chip is not shown.
  [[EarlGrey selectElementWithMatcher:SuggestPasswordChip()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the typed credentials are correctly saved in the sign-in UFF flow.
- (void)testSaveTypedCredentialInUff {
  NSString* usernameValue = @"test-username";
  NSString* passwordValue = @"test-password";

  [self loadUFFLoginPage];

  // Type username and password in their respective fields.
  TypeUsernameAndPasswordOnUFF(usernameValue, passwordValue);

  LoginOnUff();

  // Wait until the save password prompt becomes visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)];

  [[EarlGrey selectElementWithMatcher:PasswordInfobarButton(
                                          IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON)]
      performAction:grey_tap()];

  // Wait until the save password infobar disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)];

  // Verify that the credential was correctly saved.
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of stored credentials.");
  [PasswordManagerAppInterface
      verifyCredentialStoredWithUsername:@"test-username"
                                password:@"test-password"];
}

// Tests that the autofilled email is correctly saved as the username in the
// sign-in UFF flow.
- (void)testSaveCredentialWithAutofilledEmailInUFF {
  NSString* passwordValue = @"test-password";

  // Add Autofill profile to store.
  [AutofillAppInterface saveExampleProfile];

  [self loadUFFLoginPage];

  // Fill username field with the email from the autofill profile.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("single_un")];
  NSString* email = base::SysUTF16ToNSString(
      autofill::test::GetFullProfile().GetRawInfo(autofill::EMAIL_ADDRESS));
  id<GREYMatcher> email_chip = grey_text(email);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:email_chip];
  [[EarlGrey selectElementWithMatcher:email_chip] performAction:grey_tap()];

  // Type password.
  TypeTextOnField(passwordValue, "single_pw");

  LoginOnUff();

  // Wait until the save password prompt becomes visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)];

  [[EarlGrey selectElementWithMatcher:PasswordInfobarButton(
                                          IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON)]
      performAction:grey_tap()];

  // Wait until the save password infobar disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)];

  // Verify that the credential was correctly saved.
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of stored credentials.");
  [PasswordManagerAppInterface
      verifyCredentialStoredWithUsername:email
                                password:passwordValue];
}

// Tests that the typed credentials are correctly updated in the sign-in UFF
// flow when there is already a credential stored for the corresponding email.
- (void)testUpdateTypedCredentialInUff {
  NSString* usernameValue = @"test-username";
  NSString* passwordValue = @"test-password";
  NSString* passwordValueToBeReplaced = @"old-password";

  [self loadUFFLoginPage];

  [PasswordManagerAppInterface
      storeCredentialWithUsername:usernameValue
                         password:passwordValueToBeReplaced];
  GREYAssertEqual(1, [PasswordManagerAppInterface storedCredentialsCount],
                  @"Wrong number of initial credentials.");

  // Load the page again to take into consideration the new saved credential.
  [self loadUFFLoginPage];

  // Type username and password in their respective fields.
  TypeUsernameAndPasswordOnUFF(usernameValue, passwordValue,
                               /*dismissBottomSheetOnUsername=*/true);

  LoginOnUff();

  // Wait until the update password prompt becomes visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD)];

  [[EarlGrey
      selectElementWithMatcher:PasswordInfobarButton(
                                   IDS_IOS_PASSWORD_MANAGER_UPDATE_BUTTON)]
      performAction:grey_tap()];

  // Wait until the update password infobar disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          PasswordInfobarLabels(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD)];

  // Verify that the credential was correctly saved.
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of stored credentials.");
  [PasswordManagerAppInterface
      verifyCredentialStoredWithUsername:usernameValue
                                password:passwordValue];
}

// Tests that a login event is reported to an enterprise connector.
- (void)testLoginEventReported {
  [self loadLoginPage];

  // Simulate login.
  TypeTextOnField(@"test-username@test-domain.com", kFormUsername);
  TypeTextOnField(@"test-password", kFormPassword);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  // Wait for report to upload.
  [self waitForEnterpriseReports:1];

  std::vector<UploadEventsRequest> requests =
      _reportingEnvironment->reporting_server()->GetUploadedReports();
  GREYAssertEqual(1U, requests.size(), @"Wrong number of reports.");
  GREYAssertEqual(std::string("iOS"), requests[0].device().os_platform(),
                  @"Wrong OS platform in report.");
  GREYAssertEqual(1, requests[0].events_size(), @"Wrong number of events.");

  const Event& event = requests[0].events(0);
  GREYAssertTrue(event.has_login_event(), @"Wrong event type.");
  GREYAssertEqual(self.testServer->GetURL("/"), event.login_event().url(),
                  @"Wrong URL reported to server.");
  // The `test-username` portion of the email will be masked, but the domain
  // part shouldn't be.
  GREYAssertTrue(
      event.login_event().login_user_name().ends_with("@test-domain.com"),
      @"Wrong domain in login user name.");
}

// Tests that a password breach event is reported to an enterprise connector.
- (void)testPasswordBreachEventReported {
  [self loadLoginPage];

  // Simulate login.
  TypeTextOnField(@"test-username@test-domain.com", kFormUsername);
  TypeTextOnField(@"test-password", kFormPassword);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  // Wait for report to upload and UI to update.
  [self waitForEnterpriseReports:1];
  [[EarlGrey selectElementWithMatcher:PasswordBreachMatcher()]
      assertWithMatcher:grey_notNil()];

  // Retrieve and check the password breach event. There's no login event
  // expected because during `-setUp`, the browser fetches a policy from
  // `_reportingEnvironment` that only enables password breach event reporting.
  std::vector<UploadEventsRequest> requests =
      _reportingEnvironment->reporting_server()->GetUploadedReports();
  GREYAssertEqual(1U, requests.size(), @"Wrong number of reports.");
  GREYAssertEqual(std::string("iOS"), requests[0].device().os_platform(),
                  @"Wrong OS platform in report.");
  GREYAssertEqual(1, requests[0].events_size(), @"Wrong number of events.");

  const Event& event = requests[0].events(0);
  GREYAssertTrue(event.has_password_breach_event(), @"Wrong event type.");
  GREYAssertEqual(PasswordBreachEvent::PASSWORD_ENTRY,
                  event.password_breach_event().trigger(),
                  @"Wrong trigger type.");
  GREYAssertEqual(1, event.password_breach_event().identities_size(),
                  @"Wrong number of leaked identities.");

  const Identity& identity = event.password_breach_event().identities(0);
  GREYAssertEqual(self.testServer->GetURL("/"), identity.url(),
                  @"Wrong URL reported for leaked identity.");
  // The `test-username` portion of the email will be masked, but the domain
  // part shouldn't be.
  GREYAssertTrue(identity.username().ends_with("@test-domain.com"),
                 @"Wrong domain in leaked username.");
}

@end
