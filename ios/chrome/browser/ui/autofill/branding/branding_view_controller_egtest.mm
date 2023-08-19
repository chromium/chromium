// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/autofill/features.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

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

// Returns the config that has enabled the autofill branding feature with
// provided frequency type for phone and tablet.
AppLaunchConfiguration ConfigWithBrandingEnabledWithFrequencyType(
    std::string phone,
    std::string tablet) {
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      "--enable-features=" +
      std::string(autofill::features::kAutofillBrandingIOS.name) + ":" +
      std::string(
          autofill::features::kAutofillBrandingIOSParamFrequencyTypePhone) +
      "/" + phone + "/" +
      std::string(
          autofill::features::kAutofillBrandingIOSParamFrequencyTypeTablet) +
      "/" + tablet);
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

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
  ConditionBlock keyboardShown = ^{
    return [EarlGrey isKeyboardShownWithError:nil];
  };
  GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                     base::test::ios::kWaitForUIElementTimeout, keyboardShown),
                 @"Keyboard not brought up.");
}

// Dismisses the keyboard, if it exist.
void DismissKeyboard() {
  [EarlGrey dismissKeyboardWithError:nil];
  ConditionBlock keyboardDismissed = ^{
    return ![EarlGrey isKeyboardShownWithError:nil];
  };
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, keyboardDismissed),
      @"Keyboard not dismissed.");
}

// Check that the branding visibility matches the parameter `visibility`.
void CheckBrandingHasVisiblity(BOOL visibility) {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"kBrandingButtonAXId")]
      assertWithMatcher:visibility ? grey_sufficientlyVisible()
                                   : grey_notVisible()];
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
  // Clear feature related local state prefs.
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kAutofillBrandingIconDisplayCount];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kAutofillBrandingKeyboardAccessoriesTapped];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kAutofillBrandingIconAnimationRemainingCount];
  // Turn on test server and load test page.
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

// BrandingViewControllerTestCases that tests keyboard accessory behaviors with
// the branding shown.
@interface BrandingViewControllerAlwaysVisibleTestCase
    : BrandingViewControllerTestCase
@end

@implementation BrandingViewControllerAlwaysVisibleTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  return ConfigWithBrandingEnabledWithFrequencyType(
      /*phone=*/autofill::features::
          kAutofillBrandingIOSParamFrequencyTypeAlways,
      /*tablet=*/autofill::features::
          kAutofillBrandingIOSParamFrequencyTypeAlways);
}

// Tests that the branding is visible when some manual fill button is visible.
- (void)testSomeManualFillButtonsVisible {
  EnableManualFillButtonForPassword();
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
  EnableManualFillButtonForPassword();
  DisableManualFillButtons();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that the branding is visible when some manual fill button is enabled,
// disabled and re-enabled before the keyboard is presented.
- (void)testEnableDisableAndReenableManualFillButtonsBeforeKeyboardPresented {
  EnableManualFillButtonForPassword();
  DisableManualFillButtons();
  EnableManualFillButtonForProfile();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
}

// Tests that the branding is visible when some manual fill button is enabled,
// then disappears when the manual fill button is disabled.
- (void)testDisableManualFillButtonsDuringKeyboardPresenting {
  EnableManualFillButtonForPassword();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  DisableManualFillButtons();
  DismissKeyboard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that the branding is invisible until some manual fill button is
// enabled, then disappears when the manual fill button is disabled.
- (void)testEnableAndDisableManualFillButtonsDuringKeyboardPresenting {
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
  EnableManualFillButtonForPassword();
  DismissKeyboard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  DisableManualFillButtons();
  DismissKeyboard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that the branding is visible even after one of the multiple manual fill
// buttons is disabled.
- (void)testEnableTwoManualFillButtonsAndDisableOneDuringKeyboardPresenting {
  EnableManualFillButtonForPassword();
  EnableManualFillButtonForCreditCard();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  // Hide manual fill button for password.
  [AutofillAppInterface clearPasswordStore];
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

// BrandingViewControllerTestCases with existing experiment variations in
// about_flags.mm.
@interface BrandingViewControllerTestCaseForVariations
    : BrandingViewControllerTestCase
@end

@implementation BrandingViewControllerTestCaseForVariations

// Tests that the branding is invisible when the autofill branding flag is
// disabled, regardless of the visibility of manual fill buttons.
- (void)testBrandingDisabled {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(autofill::features::kAutofillBrandingIOS);
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  EnableManualFillButtonForPassword();
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that when the "two impressions" variation is turned on, the autofill
// branding icon only shows twice.
- (void)testBrandingTwoImpressions {
  AppLaunchConfiguration config = ConfigWithBrandingEnabledWithFrequencyType(
      /*phone=*/autofill::features::kAutofillBrandingIOSParamFrequencyTypeTwice,
      /*tablet=*/autofill::features::
          kAutofillBrandingIOSParamFrequencyTypeTwice);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  EnableManualFillButtonForPassword();
  // First time.
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];
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

// Tests that when the "until interacted" variation is turned on, the autofill
// branding icon shows until the user interacts with it.
- (void)testBrandingDismissWhenInteracted {
  AppLaunchConfiguration config = ConfigWithBrandingEnabledWithFrequencyType(
      /*phone=*/autofill::features::
          kAutofillBrandingIOSParamFrequencyTypeUntilInteracted,
      /*tablet=*/autofill::features::
          kAutofillBrandingIOSParamFrequencyTypeUntilInteracted);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  EnableManualFillButtonForPassword();
  // First time: no interaction.
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  DismissKeyboard();
  // Second time: no interaction
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  DismissKeyboard();
  // Third time: interact.
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];
  DismissKeyboard();
  // Fourth time: branding should not be visible.
  BringUpKeyboard();
  CheckBrandingHasVisiblity(NO);
}

// Tests that when the "dismiss when interacted" variation is turned on, the
// autofill branding icon shows until the user interacts with it.
- (void)testBrandingSlideToLeadingEdgeWhenInteractedOnPhone {
  AppLaunchConfiguration config = ConfigWithBrandingEnabledWithFrequencyType(
      /*phone=*/autofill::features::
          kAutofillBrandingIOSParamFrequencyTypeDismissWhenInteracted,
      /*tablet=*/autofill::features::
          kAutofillBrandingIOSParamFrequencyTypeAlways);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  EnableManualFillButtonForPassword();
  // First time: no interaction.
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  DismissKeyboard();
  // Second time: interact.
  BringUpKeyboard();
  CheckBrandingHasVisiblity(YES);
  [[EarlGrey selectElementWithMatcher:ManualFallbackPasswordIconMatcher()]
      performAction:grey_tap()];
  // On iPhone, the branding should be sliding away from the leading edge after
  // a slight wait time.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));
  [ChromeEarlGreyUI waitForAppToIdle];
  CheckBrandingHasVisiblity([ChromeEarlGrey isIPadIdiom]);
  DismissKeyboard();
  // Third time: branding should not be visible on iPhone, but visible on iPad.
  BringUpKeyboard();
  CheckBrandingHasVisiblity([ChromeEarlGrey isIPadIdiom]);
}

@end
