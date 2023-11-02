// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/autofill/features.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ManualFallbackKeyboardIconMatcher;
using chrome_test_util::ManualFallbackPasswordIconMatcher;
using chrome_test_util::TapWebElementWithId;
using chrome_test_util::WebViewMatcher;

namespace {
// HTML test page with form fields.
const char kFormHTMLFile[] = "/username_password_field_form.html";
// The "username" field in the test page.
const char kFormElementUsername[] = "username";

// Save a set of credentials so that the manual fill password button is visible
// in keyboard accessories.
void EnableManualFillButtonForPassword() {
  [AutofillAppInterface saveExamplePasswordForm];
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

// Remove all saved passwords, credit cards and addresses so that no manual fill
// buttons will show in keyboard accessories.
void DisableManualFillButtons() {
  [AutofillAppInterface clearPasswordStore];
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface clearCreditCardStore];
}

// Taps a form field to bring up the system keyboard.
void BringUpKeyboard() {
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];
}

// Check that the branding visibility matches the parameter `visibility`.
void CheckBrandingVisiblityWithDefaultTimeout(BOOL visibility) {
  NSString* conditionDescription = visibility
                                       ? @"Branding icon should be visible"
                                       : @"Branding icon should be hidden";
  GREYCondition* visibilityCondition = [GREYCondition
      conditionWithName:conditionDescription
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey
                        selectElementWithMatcher:grey_accessibilityID(
                                                     @"kBrandingButtonAXId")]
                        assertWithMatcher:visibility
                                              ? grey_sufficientlyVisible()
                                              : grey_notVisible()
                                    error:&error];
                    return error == nil;
                  }];
  BOOL success = [visibilityCondition
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];
  GREYAssertTrue(success, conditionDescription);
}

}  // namespace

// Super class for integration Tests for Brandings View Controller. This class
// only defines setUp and tearDown methods; actual tests are implemented in
// subclasses with different feature flags.
@interface BrandingViewControllerTestCase : ChromeTestCase
@end

@implementation BrandingViewControllerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  GURL url = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];
  DisableManualFillButtons();
}

- (void)tearDown {
  DisableManualFillButtons();
  [super tearDown];
}

@end

// BrandingViewControllerTestCases with flag enabled.
@interface BrandingViewControllerEnabledTestCase
    : BrandingViewControllerTestCase
@end

@implementation BrandingViewControllerEnabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(autofill::features::kAutofillBrandingIOS);
  return config;
}

// Tests that the branding is visible when some manual fill button is visible.
- (void)testSomeManualFillButtonsVisible {
  EnableManualFillButtonForPassword();
  BringUpKeyboard();
  CheckBrandingVisiblityWithDefaultTimeout(YES);
}

// Tests that the branding is not visible when no manual fill button is visible.
- (void)testAllManualFillButtonsHidden {
  BringUpKeyboard();
  CheckBrandingVisiblityWithDefaultTimeout(NO);
}

// Tests that the branding is not visible when some manual fill button is
// enabled then disabled before the keyboard is presented.
- (void)testEnableAndDisableManualFillButtonsBeforeKeyboardPresented {
  EnableManualFillButtonForPassword();
  DisableManualFillButtons();
  BringUpKeyboard();
  CheckBrandingVisiblityWithDefaultTimeout(NO);
}

// Tests that the branding is visible when some manual fill button is enabled,
// disabled and re-enabled before the keyboard is presented.
- (void)testEnableDisableAndReenableManualFillButtonsBeforeKeyboardPresented {
  EnableManualFillButtonForPassword();
  DisableManualFillButtons();
  EnableManualFillButtonForProfile();
  BringUpKeyboard();
  CheckBrandingVisiblityWithDefaultTimeout(YES);
}

// Tests that the branding is visible when some manual fill button is enabled,
// then disappears when the manual fill button is disabled.
- (void)testDisableManualFillButtonsDuringKeyboardPresenting {
  EnableManualFillButtonForPassword();
  BringUpKeyboard();
  CheckBrandingVisiblityWithDefaultTimeout(YES);
  DisableManualFillButtons();
  CheckBrandingVisiblityWithDefaultTimeout(NO);
}

// Tests that the branding is invisible until some manual fill button is
// enabled, then disappears when the manual fill button is disabled.
- (void)testEnableAndDisableManualFillButtonsDuringKeyboardPresenting {
  BringUpKeyboard();
  CheckBrandingVisiblityWithDefaultTimeout(NO);
  EnableManualFillButtonForPassword();
  CheckBrandingVisiblityWithDefaultTimeout(YES);
  DisableManualFillButtons();
  CheckBrandingVisiblityWithDefaultTimeout(NO);
}

// Tests that the branding is visible even after one of the multiple manual fill
// buttons is disabled.
- (void)testEnableTwoManualFillButtonsAndDisableOneDuringKeyboardPresenting {
  BringUpKeyboard();
  CheckBrandingVisiblityWithDefaultTimeout(NO);
  EnableManualFillButtonForPassword();
  EnableManualFillButtonForCreditCard();
  CheckBrandingVisiblityWithDefaultTimeout(YES);
  // Hide manual fill button for password.
  [AutofillAppInterface clearPasswordStore];
  CheckBrandingVisiblityWithDefaultTimeout(YES);
  // Hide manual fill button for credit card.
  [AutofillAppInterface clearCreditCardStore];
  CheckBrandingVisiblityWithDefaultTimeout(NO);
}

// Tests that the branding is visible when the keyboard button is visible in the
// keyboard accessories, even if no manual fill buttons are visible. Note that
// this test is disabled on iPad since the keyboard button does not show there.
- (void)testDisableManualFillWhenKeyboardButtonPressed {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Keyboard button does not exist on iPad.");
  }

  EnableManualFillButtonForPassword();
  BringUpKeyboard();

  CheckBrandingVisiblityWithDefaultTimeout(YES);
  // Press the password button to display the keyboard button.
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Disable manual fill passwords; wait until the button is hidden.
  DisableManualFillButtons();
  GREYCondition* passwordButtonCondition = [GREYCondition
      conditionWithName:@"password button should be hidden"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey selectElementWithMatcher:
                                   ManualFallbackPasswordIconMatcher()]
                        assertWithMatcher:grey_notVisible()
                                    error:&error];
                    return error == nil;
                  }];
  BOOL passwordButtonHidden = [passwordButtonCondition
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];
  GREYAssertTrue(passwordButtonHidden,
                 @"Password button visible when no password is stored");
  // Check that keyboard button and the branding are still visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  CheckBrandingVisiblityWithDefaultTimeout(YES);
  // Tap on the keyboard button to reset keyboard accessories.
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      performAction:grey_tap()];
  CheckBrandingVisiblityWithDefaultTimeout(NO);
}

@end

// BrandingViewControllerTestCases with flag disabled.
@interface BrandingViewControllerDisabledTestCase
    : BrandingViewControllerTestCase
@end

@implementation BrandingViewControllerDisabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(autofill::features::kAutofillBrandingIOS);
  return config;
}

// Tests that the branding is invisible when the autofill branding flag is
// disabled, regardless of the visibility of manual fill buttons.
- (void)testBrandingDisabled {
  EnableManualFillButtonForPassword();
  BringUpKeyboard();
  CheckBrandingVisiblityWithDefaultTimeout(NO);
}

@end
