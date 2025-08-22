// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::PaymentMethodsButton;

namespace {

// Matcher for the CVC storage button on the Payment Methods screen.
id<GREYMatcher> CvcStorageButton() {
  return grey_text(l10n_util::GetNSString(
      IDS_PAYMENTS_AUTOFILL_ENABLE_SAVE_SECURITY_CODES_LABEL));
}

}  // namespace

// Tests for the Autofill CVC storage section of the settings.
@interface AutofillCvcStorageSettingsTestCase : ChromeTestCase
@end

@implementation AutofillCvcStorageSettingsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillEnableCvcStorageAndFilling);
  return config;
}

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearCreditCardStore];
}

- (void)tearDownHelper {
  [AutofillAppInterface clearCreditCardStore];
  [super tearDownHelper];
}

// Helper to open the CVC storage settings.
- (void)openCvcStorageSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];
  [[EarlGrey selectElementWithMatcher:CvcStorageButton()]
      performAction:grey_tap()];
}

// Tests that the CVC storage switch is on when the pref is set to on.
- (void)testSwitchIsOnWhenPrefIsOn {
  [AutofillAppInterface setPaymentCvcStorageEnabled:YES];
  [self openCvcStorageSettings];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillSaveSecurityCodesSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the CVC storage switch is off if the pref is set to off.
- (void)testSwitchIsOffWhenPrefIsOff {
  [AutofillAppInterface setPaymentCvcStorageEnabled:NO];
  [self openCvcStorageSettings];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillSaveSecurityCodesSwitchViewId,
                                   /*is_toggled_on=*/NO, /*is_enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the CVC storage switch can be turned off.
- (void)testSwitchCanBeTurnedOff {
  [AutofillAppInterface setPaymentCvcStorageEnabled:YES];
  [self openCvcStorageSettings];

  // Verify switch is on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillSaveSecurityCodesSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/YES)]
      assertWithMatcher:grey_notNil()];

  // Turn switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillSaveSecurityCodesSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Verify switch is off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillSaveSecurityCodesSwitchViewId,
                                   /*is_toggled_on=*/NO, /*is_enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the delete CVCs button is visible.
- (void)testDeleteButtonIsVisible {
  [self openCvcStorageSettings];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillDeleteSecurityCodesButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
