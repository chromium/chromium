// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/passwords/password_manager_app_interface.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

static constexpr char kFormPassword[] = "pw";

namespace {

using base::test::ios::kWaitForActionTimeout;

BOOL WaitForKeyboardToAppear() {
  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard"
                  block:^BOOL {
                    return [EarlGrey isKeyboardShownWithError:nil];
                  }];
  return [waitForKeyboard waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
}

id<GREYMatcher> ButtonWithAccessibilityID(NSString* id) {
  return grey_allOf(grey_accessibilityID(id),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

}  // namespace

@interface PasswordSuggestionBottomSheetEGTest : ChromeTestCase
@end

@implementation PasswordSuggestionBottomSheetEGTest

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
}

- (void)tearDown {
  [PasswordManagerAppInterface clearCredentials];
  [PasswordSuggestionBottomSheetAppInterface removeMockReauthenticationModule];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      password_manager::features::kIOSPasswordBottomSheet);
  config.features_enabled.push_back(
      password_manager::features::kPasswordsGrouping);
  return config;
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
}

// Return the edit button from the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
                    grey_not(chrome_test_util::TabGridEditButton()),
                    grey_userInteractionEnabled(), nil);
}

// Matcher for the Delete button at with accessibility identifier containing
// `username` and `password` in Password Details view.
id<GREYMatcher> DeleteButtonForUsernameAndPassword(NSString* username,
                                                   NSString* password) {
  return grey_allOf(
      grey_accessibilityID([NSString
          stringWithFormat:@"%@%@%@", kDeleteButtonForPasswordDetailsId,
                           username, password]),
      grey_interactable(), nullptr);
}

// Matcher for the Delete button in Confirmation Alert for password deletion.
id<GREYMatcher> DeleteConfirmationButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(
                        l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)),
                    grey_interactable(), nullptr);
}

#pragma mark - Tests

- (void)testOpenPasswordBottomSheetUsePassword {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];
}

// Notes:
// - Using the password twice will allow us to know if observers are
//   properly removed between 2 uses of the bottom sheet.
// - Testing in incognito mode will allow us to know if we're using a
//   coherent browser state for all the objects in the bottom sheet.
- (void)testOpenPasswordBottomSheetUsePasswordTwiceIncognito {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];

  [ChromeEarlGrey openNewIncognitoTab];
  [self loadLoginPage];

  for (int i = 0; i < 2; ++i) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityLabel(l10n_util::GetNSString(
                       IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
        performAction:grey_tap()];
  }
}

- (void)testOpenPasswordBottomSheetTapNoThanksShowKeyboard {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_THANKS))]
      performAction:grey_tap()];

  WaitForKeyboardToAppear();
}

- (void)testOpenPasswordBottomSheetOpenPasswordManager {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL =
      net::NSURLWithGURL(self.testServer->GetURL("/simple_login_form.html"));
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user"
                                                  password:@"password"
                                                       URL:URL];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(2, credentialsCount, @"Wrong number of stored credentials.");

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user")]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];

  // Long press to open context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      performAction:grey_longPress()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(
                         l10n_util::GetNSString(
                             IDS_IOS_PASSWORD_BOTTOM_SHEET_PASSWORD_MANAGER)),
                     grey_interactable(), nullptr)] performAction:grey_tap()];

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

- (void)testOpenPasswordBottomSheetOpenPasswordDetails {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL =
      net::NSURLWithGURL(self.testServer->GetURL("/simple_login_form.html"));
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user"
                                                  password:@"password"
                                                       URL:URL];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(2, credentialsCount, @"Wrong number of stored credentials.");

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user")]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];

  // Long press to open context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      performAction:grey_longPress()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(
                         l10n_util::GetNSString(
                             IDS_IOS_PASSWORD_BOTTOM_SHEET_SHOW_DETAILS)),
                     grey_interactable(), nullptr)] performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME)]
      assertWithMatcher:grey_textFieldValue(@"user2")];
}

- (void)testOpenPasswordBottomSheetDeletePassword {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL =
      net::NSURLWithGURL(self.testServer->GetURL("/simple_login_form.html"));
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user"
                                                  password:@"password"
                                                       URL:URL];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(2, credentialsCount, @"Wrong number of stored credentials.");

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user")]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];

  // Long press to open context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      performAction:grey_longPress()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(
                         l10n_util::GetNSString(
                             IDS_IOS_PASSWORD_BOTTOM_SHEET_SHOW_DETAILS)),
                     grey_interactable(), nullptr)] performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButtonForUsernameAndPassword(
                                          @"user2", @"password2")]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that user2 is not available anymore.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user")]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      assertWithMatcher:grey_nil()];
}

- (void)testOpenPasswordBottomSheetSelectPassword {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL =
      net::NSURLWithGURL(self.testServer->GetURL("/simple_login_form.html"));
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user"
                                                  password:@"password"
                                                       URL:URL];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(2, credentialsCount, @"Wrong number of stored credentials.");

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user")]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];
}

- (void)testOpenPasswordBottomSheetExpand {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL =
      net::NSURLWithGURL(self.testServer->GetURL("/simple_login_form.html"));
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  for (int i = 1; i <= 9; i++) {
    [PasswordManagerAppInterface
        storeCredentialWithUsername:[NSString stringWithFormat:@"user%i", i]
                           password:@"password"
                                URL:URL];
  }
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(9, credentialsCount, @"Wrong number of stored credentials.");

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user1")];

  // Tap to expand.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user1")]
      performAction:grey_tap()];

  // Scroll to the last password.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kPasswordSuggestionBottomSheetTableViewId)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user9")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user9")]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];
}

- (void)testPasswordBottomSheetDismiss3TimesNotShownAnymore {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];

  // Dismiss #1.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_THANKS))]
      performAction:grey_tap()];

  WaitForKeyboardToAppear();

  // Dismiss #2.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_THANKS))]
      performAction:grey_tap()];

  WaitForKeyboardToAppear();

  // Dismiss #3.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_THANKS))]
      performAction:grey_tap()];

  WaitForKeyboardToAppear();

  // Verify that keyboard is shown.
  [self loadLoginPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];
  WaitForKeyboardToAppear();
}

- (void)testOpenPasswordBottomSheetNoUsername {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@""
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(l10n_util::GetNSString(
                          IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_USERNAME))];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_USERNAME))]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];

  // Verify that selecting credentials with no username disables the bottom
  // sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  WaitForKeyboardToAppear();
}

@end
