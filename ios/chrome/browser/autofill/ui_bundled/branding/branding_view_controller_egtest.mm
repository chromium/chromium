// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_matchers.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::TapWebElementWithId;
using chrome_test_util::WebViewMatcher;

namespace {
// HTML test page with form fields.
const char kFormHTMLFile[] = "/username_password_field_form.html";
// The "username" field in the test page.
const char kFormElementUsername[] = "username";

// Save a set of credentials to the profile store so that the manual fill
// password button is visible in keyboard accessories.
void EnableManualFillButtonForPasswordInProfileStore() {
  [AutofillAppInterface saveExamplePasswordFormToProfileStore];
}

// Save an address so that the manual fill address button is visible in keyboard
// accessories.
void EnableManualFillButtonForProfile() {
  [AutofillAppInterface saveExampleProfile];
}

// Save a credit card information so that the manual fill credit card button is
// visible in keyboard accessories.
void EnableManualFillButtonForCreditCard() {
  [AutofillAppInterface saveLocalCreditCard];
}

// Remove all passwords in profile store, all credit cards and all addresses so
// that no manual fill buttons will show in keyboard accessories.
void DisableManualFillButtonsInProfileStore() {
  [AutofillAppInterface clearProfilePasswordStore];
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface clearCreditCardStore];
}

// Taps a form field to bring up the system keyboard.
void BringUpKeyboard() {
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];
  [ChromeEarlGrey waitForKeyboardToAppear];
}

// Dismisses the keyboard, if it exist.
void DismissKeyboard() {
  [EarlGrey dismissKeyboardWithError:nil];
  [ChromeEarlGrey waitForKeyboardToDisappear];
}

// Check that the branding visibility matches the parameter `visibility`.
void CheckBrandingHasVisiblity(BOOL visibility) {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"kBrandingButtonAXId")]
      assertWithMatcher:visibility ? grey_sufficientlyVisible()
                                   : grey_notVisible()];
}

// Opens the manual fallback menu by pressing the right keyboard accessory
// button.
void OpenManualFallback() {
  id<GREYMatcher> button_to_tap;
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    button_to_tap = grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_AUTOFILL_PASSWORD_AUTOFILL_DATA)),
                               grey_ancestor(grey_accessibilityID(
                                   kFormInputAccessoryViewAccessibilityID)),
                               nil);
  } else {
    button_to_tap = manual_fill::PasswordIconMatcher();
  }

  [[EarlGrey selectElementWithMatcher:button_to_tap] performAction:grey_tap()];
}

}  // namespace

// Super class for integration Tests for Brandings View Controller. This class
// only defines setUp and tearDown methods; actual tests are implemented in
// subclasses with different feature flags.
@interface BrandingViewControllerTestCase : ChromeTestCase
@end

@implementation BrandingViewControllerTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (void)setUp {
  [super setUp];
  // Reset pref and restart, if needed.
  if ([ChromeEarlGrey
          localStateIntegerPref:prefs::kAutofillBrandingIconDisplayCount] > 0) {
    [ChromeEarlGrey
        resetDataForLocalStatePref:prefs::kAutofillBrandingIconDisplayCount];
    [ChromeEarlGrey resetDataForLocalStatePref:
                        prefs::kAutofillBrandingIconAnimationRemainingCount];
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];
  }
  DisableManualFillButtonsInProfileStore();
  // Turn on test server and load test page.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  GURL url = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];
}

- (void)tearDown {
  DisableManualFillButtonsInProfileStore();
  // Clear feature related local state prefs.
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAutofillBrandingIconDisplayCount];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kAutofillBrandingIconAnimationRemainingCount];
  [super tearDown];
}

// Tests that the autofill branding icon only shows twice.
- (void)testBrandingTwoImpressions {
  EnableManualFillButtonForPasswordInProfileStore();
  // First time.
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  OpenManualFallback();

  if (!base::ios::IsRunningOnIOS16OrLater() && [ChromeEarlGrey isIPadIdiom]) {
    [ChromeEarlGreyUI dismissByTappingOnTheWindowOfPopover:
                          manual_fill::PasswordTableViewMatcher()];
  }

  DismissKeyboard();
  // Second time: branding is still visible after user interacts with a keyboard
  // accessory element.
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  DismissKeyboard();
  // Third time.
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that the branding is visible when some manual fill button is visible.
- (void)testSomeManualFillButtonsVisible {
  EnableManualFillButtonForPasswordInProfileStore();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
}

// Tests that the branding is not visible when no manual fill button is visible.
- (void)testAllManualFillButtonsHidden {
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that the branding is not visible when some manual fill button is
// enabled then disabled before the keyboard is presented.
- (void)testEnableAndDisableManualFillButtonsBeforeKeyboardPresented {
  EnableManualFillButtonForPasswordInProfileStore();
  DisableManualFillButtonsInProfileStore();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that the branding is visible when some manual fill button is enabled,
// disabled and re-enabled before the keyboard is presented.
- (void)testEnableDisableAndReenableManualFillButtonsBeforeKeyboardPresented {
  EnableManualFillButtonForPasswordInProfileStore();
  DisableManualFillButtonsInProfileStore();
  EnableManualFillButtonForProfile();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
}

// Tests that the branding is visible when some manual fill button is enabled,
// then disappears when the manual fill button is disabled.
- (void)testDisableManualFillButtonsDuringKeyboardPresenting {
  EnableManualFillButtonForPasswordInProfileStore();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  DisableManualFillButtonsInProfileStore();
  DismissKeyboard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that the branding is invisible until some manual fill button is
// enabled, then disappears when the manual fill button is disabled.
- (void)testEnableAndDisableManualFillButtonsDuringKeyboardPresenting {
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
  EnableManualFillButtonForPasswordInProfileStore();
  DismissKeyboard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  DisableManualFillButtonsInProfileStore();
  DismissKeyboard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that the branding is visible even after one of the multiple manual fill
// buttons is disabled.
- (void)testEnableTwoManualFillButtonsAndDisableOneDuringKeyboardPresenting {
  EnableManualFillButtonForPasswordInProfileStore();
  EnableManualFillButtonForCreditCard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  // Hide manual fill button for password.
  [AutofillAppInterface clearProfilePasswordStore];
  DismissKeyboard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  // Hide manual fill button for credit card.
  [AutofillAppInterface clearCreditCardStore];
  DismissKeyboard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

@end
