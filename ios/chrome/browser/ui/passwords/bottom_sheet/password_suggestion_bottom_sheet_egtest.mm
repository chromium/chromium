// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/metrics/metrics_app_interface.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_traits_overrider.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

static constexpr char kFormUsername[] = "un";
static constexpr char kFormPassword[] = "pw";

namespace {

using base::test::ios::kWaitForActionTimeout;
using password_manager_test_utils::DeleteCredential;

BOOL WaitForKeyboardToAppear() {
  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard"
                  block:^BOOL {
                    return [EarlGrey isKeyboardShownWithError:nil];
                  }];
  return [waitForKeyboard waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
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

id<GREYMatcher> ButtonWithAccessibilityID(NSString* id) {
  return grey_allOf(grey_accessibilityID(id),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> SubtitleString(const GURL& url) {
  return grey_text(l10n_util::GetNSStringF(
      IDS_IOS_PASSWORD_BOTTOM_SHEET_SUBTITLE,
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          url)));
}

// Verifies the number of Password Details visits recorded.
void CheckPasswordDetailsVisitMetricCount(int count) {
  NSString* histogram = @"PasswordManager.iOS.PasswordDetailsVisit";

  // Check password details visit metric.
  NSError* error = [MetricsAppInterface expectTotalCount:count
                                            forHistogram:histogram];
  GREYAssertNil(error, @"Unexpected Password Details Visit histogram count");

  error = [MetricsAppInterface expectCount:count
                                 forBucket:YES
                              forHistogram:histogram];
  GREYAssertNil(error, @"Unexpected Password Details Visit histogram count");
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

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDown {
  [PasswordManagerAppInterface clearCredentials];
  [PasswordSuggestionBottomSheetAppInterface removeMockReauthenticationModule];

  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(
      password_manager::features::kIOSPasswordBottomSheet);

  if ([self isRunningTest:@selector
            (testOpenPasswordBottomSheetOpenPasswordDetails)]) {
    config.features_enabled.push_back(
        password_manager::features::kIOSPasswordAuthOnEntryV2);
  }

  if ([self isRunningTest:@selector
            (testOpenPasswordBottomSheetOpenPasswordDetailsWithoutAuthentication
                )]) {
    config.features_disabled.push_back(
        password_manager::features::kIOSPasswordAuthOnEntryV2);
  }

  if ([self isRunningTest:@selector
            (testOpenPasswordBottomSheetWithSingleSharedPassword)] ||
      [self isRunningTest:@selector
            (testOpenPasswordBottomSheetWithMultipleSharedPasswords)] ||
      [self isRunningTest:@selector
            (testOpenPasswordBottomSheetWithSharedPasswordsAndUseKeyboard)]) {
    config.features_enabled.push_back(
        password_manager::features::kSharedPasswordNotificationUI);
  }
  return config;
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/simple_login_form_empty.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
}

// Return the edit button from the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
                    grey_not(chrome_test_util::TabGridEditButton()),
                    grey_userInteractionEnabled(), nil);
}

- (void)verifyPasswordFieldsHaveBeenFilled:(NSString*)username {
  // Verify that the username has been filled.
  NSString* condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormUsername, username];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];

  // Verify that the password field is not empty.
  NSString* filledFieldCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value !== ''",
                                 kFormPassword];
  [ChromeEarlGrey waitForJavaScriptCondition:filledFieldCondition];
}

#pragma mark - Tests

- (void)testOpenPasswordBottomSheetUsePassword {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];

  GURL URL = self.testServer->GetURL("/simple_login_form_empty.html");
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(URL)];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  // Verify that the subtitle string appears.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SubtitleString(URL)];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];

  // No histogram logged because there is only 1 credential shown to the user.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"PasswordManager.TouchToFill.CredentialIndex"],
      @"Unexpected histogram error for touch to fill credential index");

  [self verifyPasswordFieldsHaveBeenFilled:@"user"];
}

// This test will allow us to know if we're using a coherent browser state to
// open the bottom sheet in incognito mode.
- (void)testOpenPasswordBottomSheetUsePasswordIncognito {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form_empty.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];

  [ChromeEarlGrey openNewIncognitoTab];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];

  [self verifyPasswordFieldsHaveBeenFilled:@"user"];
}

- (void)testOpenPasswordBottomSheetTapUseKeyboardShowKeyboard {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form_empty.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD)]
      performAction:grey_tap()];

  WaitForKeyboardToAppear();
}

- (void)testOpenPasswordBottomSheetOpenPasswordManager {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL = net::NSURLWithGURL(
      self.testServer->GetURL("/simple_login_form_empty.html"));
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

  // Mock local authentication result needed for opening the password manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

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

  [PasswordSettingsAppInterface removeMockReauthenticationModule];
}

- (void)testOpenPasswordBottomSheetOpenPasswordDetails {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL = net::NSURLWithGURL(
      self.testServer->GetURL("/simple_login_form_empty.html"));
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
  GREYAssertEqual(2, credentialsCount, @".");

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user")]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Delay the auth result to be able to validate that password details is
  // not visible until the result is emitted.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  [PasswordSettingsAppInterface
      mockReauthenticationModuleShouldReturnSynchronously:NO];

  // Long press to open context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                         IDS_IOS_PASSWORD_BOTTOM_SHEET_SHOW_DETAILS),
                     grey_interactable(), nullptr)] performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Password details shouldn't be visible until auth is passed.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME)]
      assertWithMatcher:grey_notVisible()];

  // Verify visit metric was not recorded yet.
  CheckPasswordDetailsVisitMetricCount(0);

  // Emit auth result so password details surface is revealed.
  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME)]
      assertWithMatcher:grey_textFieldValue(@"user2")];

  // Verify visit metric was recorded.
  CheckPasswordDetailsVisitMetricCount(1);
}

- (void)testOpenPasswordBottomSheetOpenPasswordDetailsWithoutAuthentication {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL = net::NSURLWithGURL(
      self.testServer->GetURL("/simple_login_form_empty.html"));
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
          grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                         IDS_IOS_PASSWORD_BOTTOM_SHEET_SHOW_DETAILS),
                     grey_interactable(), nullptr)] performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME)]
      assertWithMatcher:grey_textFieldValue(@"user2")];

  // Verify visit metric was recorded.
  CheckPasswordDetailsVisitMetricCount(1);
}

- (void)testOpenPasswordBottomSheetDeletePassword {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL = net::NSURLWithGURL(
      self.testServer->GetURL("/simple_login_form_empty.html"));
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
          grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                         IDS_IOS_PASSWORD_BOTTOM_SHEET_SHOW_DETAILS),
                     grey_interactable(), nullptr)] performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  NSString* website = [URL.absoluteString
      stringByReplacingOccurrencesOfString:@"simple_login_form_empty.html"
                                withString:@""];
  DeleteCredential(@"user2", website);

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
  NSURL* URL = net::NSURLWithGURL(
      self.testServer->GetURL("/simple_login_form_empty.html"));
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

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:YES
          forHistogram:@"IOS.PasswordBottomSheet.UsernameTapped."
                       @"MinimizedState"],
      @"Unexpected histogram error for password bottom sheet username tapped "
      @"minimized state");

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      performAction:grey_tap()];

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:NO
          forHistogram:@"IOS.PasswordBottomSheet.UsernameTapped."
                       @"MinimizedState"],
      @"Unexpected histogram error for password bottom sheet username tapped "
      @"minimized state");

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:1
                         forHistogram:
                             @"PasswordManager.TouchToFill.CredentialIndex"],
      @"Unexpected histogram error for touch to fill credential index");

  [self verifyPasswordFieldsHaveBeenFilled:@"user2"];

  GREYWaitForAppToIdle(@"App failed to idle");
}

// TODO(crbug.com/1474949): Fix flaky test & re-enable.
#if TARGET_OS_SIMULATOR
#define MAYBE_testOpenPasswordBottomSheetExpand \
  DISABLED_testOpenPasswordBottomSheetExpand
#else
#define MAYBE_testOpenPasswordBottomSheetExpand \
  testOpenPasswordBottomSheetExpand
#endif
- (void)MAYBE_testOpenPasswordBottomSheetExpand {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  NSURL* URL = net::NSURLWithGURL(
      self.testServer->GetURL("/simple_login_form_empty.html"));
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
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertUnderTitleViewAccessibilityIdentifier)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user9")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user9")]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];

  [self verifyPasswordFieldsHaveBeenFilled:@"user9"];
}

- (void)testPasswordBottomSheetDismiss3TimesNotShownAnymore {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form_empty.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];

  // Dismiss #1.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD)]
      performAction:grey_tap()];

  WaitForKeyboardToAppear();

  // Dismiss #2.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD)]
      performAction:grey_tap()];

  WaitForKeyboardToAppear();

  // Dismiss #3.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD)]
      performAction:grey_tap()];

  WaitForKeyboardToAppear();

  // Verify that keyboard is shown.
  [self loadLoginPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];
  WaitForKeyboardToAppear();
}

// TODO(crbug.com/1474949): Fix flaky test & re-enable.
#if TARGET_OS_SIMULATOR
#define MAYBE_testOpenPasswordBottomSheetNoUsername \
  DISABLED_testOpenPasswordBottomSheetNoUsername
#else
#define MAYBE_testOpenPasswordBottomSheetNoUsername \
  testOpenPasswordBottomSheetNoUsername
#endif
- (void)MAYBE_testOpenPasswordBottomSheetNoUsername {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@""
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form_empty.html"))];
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

// Tests that the Password Bottom Sheet appears when tapping on a password
// related field and that the buttons are still visible after we chang the trait
// collection to larger content size.
- (void)testOpenPasswordBottomSheetUsePasswordAfterTraitCollectionChange {
  if (@available(iOS 17.0, *)) {
    [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
    [PasswordSuggestionBottomSheetAppInterface
        mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                     kSuccess];
    [PasswordManagerAppInterface
        storeCredentialWithUsername:@"user"
                           password:@"password"
                                URL:net::NSURLWithGURL(self.testServer->GetURL(
                                        "/simple_login_form_empty.html"))];
    [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                  enableSync:NO];
    [self loadLoginPage];

    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

    // Change trait collection to use accessibility large content size.
    ScopedTraitOverrider overrider(TopPresentedViewController());
    overrider.SetContentSizeCategory(UIContentSizeCategoryAccessibilityLarge);

    [ChromeEarlGreyUI waitForAppToIdle];

    // Verify that the "Use Password" and "No Thanks" buttons are still visible.
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityLabel(l10n_util::GetNSString(
                       IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
        assertWithMatcher:grey_notNil()];

    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD)]
        assertWithMatcher:grey_notNil()];

    // Verify the credit card tablew view is still visible.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user")]
        assertWithMatcher:grey_notNil()];

    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityLabel(l10n_util::GetNSString(
                       IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
        performAction:grey_tap()];

    [self verifyPasswordFieldsHaveBeenFilled:@"user"];
  } else {
    EARL_GREY_TEST_SKIPPED(@"Not available for under iOS 17.");
  }
}

- (void)testOpenPasswordBottomSheetWithSingleSharedPassword {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];

  // Save 1 password that has been received via sharing and the other not.
  NSURL* URL = net::NSURLWithGURL(
      self.testServer->GetURL("/simple_login_form_empty.html"));
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user1"
                                                  password:@"password1"
                                                       URL:URL
                                                    shared:YES];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL
                                                    shared:NO];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];
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
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user2")];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user2")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];
  [self verifyPasswordFieldsHaveBeenFilled:@"user2"];

  // Verify that after using the shared password regular bottom sheet is
  // displayed.
  [self loadLoginPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user1")];

  // Verify that the sharing notification is not visible anymore.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(titleMatcher,
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];
  [self verifyPasswordFieldsHaveBeenFilled:@"user1"];
}

- (void)testOpenPasswordBottomSheetWithMultipleSharedPasswords {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];

  NSURL* URL = net::NSURLWithGURL(
      self.testServer->GetURL("/simple_login_form_empty.html"));
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user1"
                                                  password:@"password1"
                                                       URL:URL
                                                    shared:YES];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"user2"
                                                  password:@"password2"
                                                       URL:URL
                                                    shared:YES];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

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

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];
  [self verifyPasswordFieldsHaveBeenFilled:@"user1"];
}

- (void)testOpenPasswordBottomSheetWithSharedPasswordsAndUseKeyboard {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];

  // Save a password that has been received via sharing.
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user1"
                         password:@"password1"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form_empty.html"))
                           shared:YES];

  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

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

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD)]
      performAction:grey_tap()];

  // Verify that after dismissing the shared notification bottom sheet, regular
  // bottom sheet is displayed.
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user1")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(titleMatcher,
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];

  [self verifyPasswordFieldsHaveBeenFilled:@"user1"];
}

@end
