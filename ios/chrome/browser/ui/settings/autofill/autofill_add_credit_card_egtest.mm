// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/settings/autofill/features.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AddPaymentMethodButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::IconViewForCellWithLabelId;
using chrome_test_util::PaymentMethodsButton;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::TextFieldForCellWithLabelId;

namespace {

// Identifiers for text field icons.
NSString* const kErrorIconIdentifier = @"_errorIcon";
NSString* const kEditIconIdentifier = @"_editIcon";

// Matcher for the 'Name on Card' field in the add credit card view.
id<GREYMatcher> NameOnCardField() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_AUTOFILL_CARDHOLDER));
}

// Matcher for the 'Card Number' field in the add credit card view.
id<GREYMatcher> CardNumberField() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_AUTOFILL_CARD_NUMBER));
}

// Matcher for the 'Month of Expiry' field in the add credit card view.
id<GREYMatcher> MonthOfExpiryField() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_AUTOFILL_EXP_MONTH));
}

// Matcher for the 'Year of Expiry' field in the add credit card view.
id<GREYMatcher> YearOfExpiryField() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_AUTOFILL_EXP_YEAR));
}

// Matcher for the 'Use Camera' button in the add credit card view.
id<GREYMatcher> UseCameraButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_AUTOFILL_ADD_CREDIT_CARD_OPEN_CAMERA_BUTTON_LABEL);
}

// Matcher for the 'Card Number' text field in the add credit card view.
id<GREYMatcher> CardNumberTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_CARD_NUMBER);
}

// Matcher for the 'Month of Expiry' text field in the add credit card view.
id<GREYMatcher> MonthOfExpiryTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_EXP_MONTH);
}

// Matcher for the 'Year of Expiry' text field in the add credit card view.
id<GREYMatcher> YearOfExpiryTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_EXP_YEAR);
}

// Matcher for the 'Card Number' icon view in the add credit card view.
id<GREYMatcher> CardNumberIconView(NSString* icon_type) {
  return IconViewForCellWithLabelId(IDS_IOS_AUTOFILL_CARD_NUMBER, icon_type);
}

}  // namespace

// Tests for Settings Autofill add credit cards section.
@interface AutofillAddCreditCardTestCase : ChromeTestCase
@end

@implementation AutofillAddCreditCardTestCase

- (void)setUp {
  [super setUp];
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{kSettingsAddPaymentMethod,
                                            kCreditCardScanner}
                                  disabled:{}
                              forceRestart:NO];
  GREYAssertTrue([ChromeEarlGrey isSettingsAddPaymentMethodEnabled],
                 @"SettingsAddPaymentMethod should be enabled");
  GREYAssertTrue([ChromeEarlGrey isCreditCardScannerEnabled],
                 @"CreditCardScanner should be enabled");
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];
  [[EarlGrey selectElementWithMatcher:AddPaymentMethodButton()]
      performAction:grey_tap()];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [super tearDown];
}

#pragma mark - Test that all fields on the 'Add Credit Card' screen appear

// Tests the different fixed elements (labels, buttons) are present on the
// screen.
- (void)testElementsPresent {
  [[EarlGrey selectElementWithMatcher:NameOnCardField()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:CardNumberField()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryField()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryField()]
      assertWithMatcher:grey_sufficientlyVisible()];
  if (@available(iOS 13, *)) {
    [[EarlGrey selectElementWithMatcher:UseCameraButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:UseCameraButton()]
        assertWithMatcher:grey_nil()];
  }

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::AddCreditCardCancelButton()]
      performAction:grey_tap()];
}

#pragma mark - Test top toolbar buttons

// Tests that the 'Add' button in the top toolbar is disabled by default.
- (void)testAddButtonDisabledOnDefault {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];
}

// Tests that the 'Cancel' button dismisses the screen.
- (void)testCancelButtonDismissesScreen {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardView()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::AddCreditCardCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardView()]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Test adding Valid/Inavlid card details

// Tests when a user tries to add an invalid card number, the "Add" button is
// not enabled.
- (void)testAddButtonDisabledOnInvalidNumber {
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_typeText(@"1234")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];
}

// Tests when a user tries to add an invalid card number, the "Add" button is
// not enabled.
- (void)testAddButtonDisabledOnInvalidExpiryDate {
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_typeText(@"4111111111111111")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_typeText(@"00")];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_typeText(@"0000")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];
}

// Tests when a user tries to add a valid card number, the screen is dismissed
// and the new card number appears on the Autofill Credit Card 'Payment Methods'
// screen.
- (void)testAddButtonOnValidNumber {
  [AutofillAppInterface clearCreditCardStore];
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_typeText(@"4111111111111111")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_typeText(@"12")];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_typeText(@"2999")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardView()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::AutofillCreditCardTableView()]
      assertWithMatcher:grey_notNil()];

  NSString* newCreditCardObjectLabel =
      @", Visa  ‪•⁠ ⁠•⁠ ⁠•⁠ ⁠•⁠ ⁠1111‬";
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          newCreditCardObjectLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Inline Testing

// Tests that an error icon is displayed when a field has invalid text. The icon
// is displayed if the field is not currently being editted.
- (void)testInvalidInputDisplaysInlineError {
  [[EarlGrey selectElementWithMatcher:CardNumberIconView(kEditIconIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Error icon displayed when field is invalid.
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_typeText(@"1234")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CardNumberIconView(kErrorIconIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:CardNumberIconView(kEditIconIdentifier)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Editing a field makes both icons disappear.
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CardNumberIconView(kErrorIconIdentifier)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [[EarlGrey selectElementWithMatcher:CardNumberIconView(kEditIconIdentifier)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Clearing the text enables the edit icon.
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_clearText()];
  [[EarlGrey selectElementWithMatcher:CardNumberIconView(kEditIconIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that add button is disabled until typing a single character makes all
// the fields valid.
- (void)testAddButtonDisabledTillValidForm {
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_typeText(@"4111111111111111")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_typeText(@"12")];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_typeText(@"299")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_typeText(@"9")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(), grey_enabled(),
                                   nil)];
}

@end
