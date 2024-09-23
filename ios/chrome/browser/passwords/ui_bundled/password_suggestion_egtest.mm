// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
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
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kNewPasswordFieldID[] = "pw";
constexpr char kConfirmationPasswordFieldID[] = "cpw";

using password_manager_test_utils::DeleteCredential;

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

// Returns the matcher for the use password button.
id<GREYMatcher> UseSuggestedPasswordButton() {
  return chrome_test_util::StaticTextWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_USE_SUGGESTED_STRONG_PASSWORD));
}

// Returns the matcher for the use keyboard button.
id<GREYMatcher> ProactivePasswordGenerationUseKeyboardButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD);
}

}  // namespace

@interface PasswordSuggestionEGTest : ChromeTestCase
@end

@implementation PasswordSuggestionEGTest

- (void)setUp {
  [super setUp];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Sign in to a chrome account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];

  // Also reset the dismiss count pref to 0 to make sure the bottom sheet is
  // enabled by default.
  [ChromeEarlGrey clearUserPrefWithName:
                      prefs::kIosPasswordGenerationBottomSheetDismissCount];

  // Clear password store to make sure that the sheet can be displayed in the
  // test.
  [PasswordSettingsAppInterface clearPasswordStores];
}

- (void)tearDown {
  [ChromeEarlGrey clearUserPrefWithName:
                      prefs::kIosPasswordGenerationBottomSheetDismissCount];
  // The test may leave stored crendentials behind so clear them.
  [PasswordSettingsAppInterface clearPasswordStores];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      password_manager::features::kIOSProactivePasswordGenerationBottomSheet);
  return config;
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadSignupPage {
  // Loads simple page with a signup form. It is on localhost so it is
  // considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(
                              "/new_password_and_confirmation_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Signup form."];
}

- (void)loadSignupAutofocusPage {
  // Loads simple page with a signup form that is auto-focused. It is on
  // localhost so it is considered a secure context.
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL(
                  "/new_password_and_confirmation_form_autofocus.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Signup form."];
}

- (void)verifyNewPasswordFieldsHaveBeenFilled {
  // Verify that the new password field is not empty.
  NSString* newPasswordfilledFieldCondition =
      [NSString stringWithFormat:@"document.getElementById('%s').value !== ''",
                                 kNewPasswordFieldID];

  // Verify that the new password field contains at least one non-space
  // character.
  NSString* newPasswordNonSpaceChar =
      [NSString stringWithFormat:@"document.getElementById('%s').value.replace("
                                 @"/\\s+/g, '').length === "
                                 @"document.getElementById('%s').value.length",
                                 kNewPasswordFieldID, kNewPasswordFieldID];

  // Verify that the new password field and confirmation password field have the
  // same value.
  NSString* passwordValuesMatch = [NSString
      stringWithFormat:@"document.getElementById('%s').value === "
                       @"document.getElementById('%s').value",
                       kNewPasswordFieldID, kConfirmationPasswordFieldID];

  NSString* condition = [NSString
      stringWithFormat:@"%@ && %@ && %@", newPasswordfilledFieldCondition,
                       newPasswordNonSpaceChar, passwordValuesMatch];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

- (void)openAndDismissBottomSheet {
  [self loadSignupPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kNewPasswordFieldID)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:UseSuggestedPasswordButton()];

  [[EarlGrey
      selectElementWithMatcher:ProactivePasswordGenerationUseKeyboardButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForKeyboardToAppear];
}

#pragma mark - Tests

// Tests that the bottom sheet populates the new password and confirm password
// fields.
- (void)testFillNewPasswordWithProactiveBottomSheet {
  [self loadSignupPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kNewPasswordFieldID)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:UseSuggestedPasswordButton()];

  [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordButton()]
      performAction:grey_tap()];

  [self verifyNewPasswordFieldsHaveBeenFilled];
}

// Tests that the bottom sheet opens on autofocus events.
- (void)testAutofocusOnProactiveBottomSheet {
  [self loadSignupAutofocusPage];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:UseSuggestedPasswordButton()];
}

// Tests that the keyboard appears if the "Use Keyboard" button is
// tapped.
- (void)testShowKeyboardFromButtonOnProactiveBottomSheet {
  [self loadSignupPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kNewPasswordFieldID)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:UseSuggestedPasswordButton()];

  [[EarlGrey
      selectElementWithMatcher:ProactivePasswordGenerationUseKeyboardButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForKeyboardToAppear];
}

// Tests that the bottom sheet does not show after it has been
// dismissed three consecutive times.
- (void)testSilenceProactiveBottomSheet {
  // Dismiss #1
  [self loadSignupPage];
  [self openAndDismissBottomSheet];

  // Dismiss #2.
  [ChromeEarlGrey reload];
  [self openAndDismissBottomSheet];

  // Dismiss #3.
  [ChromeEarlGrey reload];
  [self openAndDismissBottomSheet];

  // Verify that the keyboard is shown instead of the bottom sheet when
  // silenced.
  [ChromeEarlGrey reload];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kNewPasswordFieldID)];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Re-enable proactive password generation bottom sheet by using the
  // suggested password from the keyboard accessory.
  id<GREYMatcher> suggest_password_chip =
      grey_accessibilityLabel(@"Suggest Strong Password");

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:suggest_password_chip];

  [[EarlGrey selectElementWithMatcher:suggest_password_chip]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:UseSuggestedPasswordButton()];

  [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordButton()]
      performAction:grey_tap()];

  [self verifyNewPasswordFieldsHaveBeenFilled];

  // Clear the saved password so the proactive sheet can be displayed again.
  [PasswordSettingsAppInterface clearPasswordStores];

  // Verify that the bottom sheet is unsilenced when triggered again.
  [ChromeEarlGrey reload];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kNewPasswordFieldID)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:UseSuggestedPasswordButton()];

  [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordButton()]
      performAction:grey_tap()];

  [self verifyNewPasswordFieldsHaveBeenFilled];
}

// Tests dynamic sizing.
- (void)testProactiveBottomSheetWithDynamicTypeSizing {
  if (@available(iOS 17.0, *)) {
    [self loadSignupPage];

    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:chrome_test_util::TapWebElementWithId(
                          kNewPasswordFieldID)];

    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:UseSuggestedPasswordButton()];

    // Change trait collection to use accessibility large content size.
    ScopedTraitOverrider overrider(TopPresentedViewController());
    overrider.SetContentSizeCategory(UIContentSizeCategoryAccessibilityLarge);

    [ChromeEarlGreyUI waitForAppToIdle];

    // Verify that the "Use Suggested Password" and "Use Keyboard" buttons are
    // still visible.
    [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordButton()]
        assertWithMatcher:grey_sufficientlyVisible()];

    [[EarlGrey
        selectElementWithMatcher:ProactivePasswordGenerationUseKeyboardButton()]
        assertWithMatcher:grey_sufficientlyVisible()];

    [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordButton()]
        performAction:grey_tap()];

    [self verifyNewPasswordFieldsHaveBeenFilled];
  } else {
    EARL_GREY_TEST_SKIPPED(@"Not available for under iOS 17.");
  }
}

// Tests that the bottom sheet does not show if the user isn't signed in.
- (void)testUserSignedOut {
  [ChromeEarlGrey signOutAndClearIdentities];

  [self loadSignupPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kNewPasswordFieldID)];

  [ChromeEarlGrey waitForKeyboardToAppear];
}

- (void)testProactiveBottomSheetNotShownWhenCredentialsAvailable {
  [self loadSignupPage];

  // Fill new password with bottom sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kNewPasswordFieldID)];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:UseSuggestedPasswordButton()];
  [[EarlGrey selectElementWithMatcher:UseSuggestedPasswordButton()]
      performAction:grey_tap()];
  [self verifyNewPasswordFieldsHaveBeenFilled];

  // Reload page so new password is taken into consideration.
  [ChromeEarlGrey reload];

  // Focus on the new password and validate that the keyboard is shown instead
  // of the bottom sheet because there is now a saved credential for the site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kNewPasswordFieldID)];
  [ChromeEarlGrey waitForKeyboardToAppear];
  id<GREYMatcher> suggest_password_chip =
      grey_accessibilityLabel(@"Suggest Strong Password");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:suggest_password_chip];
}

@end
