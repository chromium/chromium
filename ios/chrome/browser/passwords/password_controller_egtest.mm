// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_prefs.h"
#import "ios/chrome/browser/passwords/password_manager_app_interface.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_app_interface.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

constexpr char kFormUsername[] = "un";
constexpr char kFormPassword[] = "pw";

namespace {

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::TapWebElementWithId;
using chrome_test_util::UseSuggestedPasswordMatcher;
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

BOOL WaitForKeyboardToAppear() {
  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard"
                  block:^BOOL {
                    return [EarlGrey isKeyboardShownWithError:nil];
                  }];
  return [waitForKeyboard waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
}

}  // namespace

@interface PasswordControllerEGTest : WebHttpServerChromeTestCase
@end

@implementation PasswordControllerEGTest

- (void)setUp {
  [super setUp];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Prefs aren't reset between tests, crbug.com/1069086. Most tests don't care
  // about the account storage notice, so suppress it by marking it as shown.
  [PasswordManagerAppInterface setAccountStorageNoticeShown:YES];
  // Also reset the dismiss count pref to 0 to make sure the bottom sheet is
  // enabled by default.
  [PasswordSuggestionBottomSheetAppInterface setDismissCount:0];
  // Manually clear sync passwords pref before testShowAccountStorageNotice*.
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:base::SysUTF8ToNSString(
                                syncer::SyncPrefs::GetPrefNameForTypeForTesting(
                                    syncer::UserSelectableType::kPasswords))];
}

- (void)tearDown {
  [PasswordManagerAppInterface clearCredentials];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self
          isRunningTest:@selector(testShowAccountStorageNoticeBeforeSaving)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  if ([self
          isRunningTest:@selector(testShowAccountStorageNoticeBeforeFilling)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
    config.features_disabled.push_back(
        password_manager::features::kIOSPasswordBottomSheet);
  }
  if ([self isRunningTest:@selector
            (testShowAccountStorageNoticeBeforeFillingBottomSheet)]) {
    config.features_enabled.push_back(
        password_manager::features::kIOSPasswordBottomSheet);
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  if ([self isRunningTest:@selector(testUpdatePromptAppearsOnFormSubmission)]) {
    config.features_enabled.push_back(
        password_manager::features::kIOSPasswordBottomSheet);
  }
  if ([self isRunningTest:@selector(testFillPasswordFieldsOnForm)]) {
    config.features_disabled.push_back(
        password_manager::features::kIOSPasswordBottomSheet);
  }
  return config;
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
}

// Verifies that the username and password fields are filled.
- (void)verifyFieldsHaveBeenFilledWithUsername:(NSString*)username
                                      password:(NSString*)password {
  // Verify that the username field has been filled.
  NSString* condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormUsername, username];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];

  // Verify that the password field has been filled.
  NSString* filledFieldCondition = [NSString
      stringWithFormat:@"document.getElementById('%s').value === '%@'",
                       kFormPassword, password];
  [ChromeEarlGrey waitForJavaScriptCondition:filledFieldCondition];
}

#pragma mark - Tests

// Tests that save password prompt is shown on new login.
// TODO(crbug.com/1192446): Reenable this test.
- (void)DISABLED_testSavePromptAppearsOnFormSubmission {
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

- (void)testShowAccountStorageNoticeBeforeSaving {
  [PasswordManagerAppInterface setAccountStorageNoticeShown:NO];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityLabel(l10n_util::GetNSString(
                          IDS_IOS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_TITLE))];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_BUTTON_TEXT))]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityLabel(l10n_util::GetNSStringF(
                          IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                          base::SysNSStringToUTF16(fakeIdentity.userEmail)))];
}

- (void)testShowAccountStorageNoticeBeforeFilling {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [PasswordManagerAppInterface setAccountStorageNoticeShown:NO];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityLabel(l10n_util::GetNSString(
                          IDS_IOS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_TITLE))];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_BUTTON_TEXT))]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_accessibilityLabel(
                                                          @"user ••••••••")];
}

- (void)testShowAccountStorageNoticeBeforeFillingBottomSheet {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [PasswordManagerAppInterface setAccountStorageNoticeShown:NO];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityLabel(l10n_util::GetNSString(
                          IDS_IOS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_TITLE))];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_BUTTON_TEXT))]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];
}

- (void)testFillPasswordFieldsOnForm {
  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];

  NSString* username = @"user";
  NSString* password = @"password";
  [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [self loadLoginPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  id<GREYMatcher> user_chip = grey_accessibilityLabel(@"user ••••••••");

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:user_chip];

  [[EarlGrey selectElementWithMatcher:user_chip] performAction:grey_tap()];

  [self verifyFieldsHaveBeenFilledWithUsername:username password:password];

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];
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
  // Open and dismiss the bottom sheet
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"Eguser")];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
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

// Tests password generation flow.
// TODO(crbug.com/1423865): The test fails on simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testPasswordGeneration FLAKY_testPasswordGeneration
#else
#define MAYBE_testPasswordGeneration testPasswordGeneration
#endif
- (void)MAYBE_testPasswordGeneration {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(10)];

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
  WaitForKeyboardToAppear();

  // Tap on a 'Suggest Password...' chip.
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

@end
