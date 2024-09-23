// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::IconViewForCellWithLabelId;
using chrome_test_util::PaymentMethodsButton;
using chrome_test_util::SettingsToolbarAddButton;
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

// Matcher for the 'Nickname' field in the add credit card view.
id<GREYMatcher> NicknameField() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_AUTOFILL_NICKNAME));
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

// Matcher for the 'Nickname' text field in the add credit card view.
id<GREYMatcher> NicknameTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_NICKNAME);
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

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Add feature configs here.
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];
  [[EarlGrey selectElementWithMatcher:SettingsToolbarAddButton()]
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
  [[EarlGrey selectElementWithMatcher:NicknameField()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::AddCreditCardCancelButton()]
      performAction:grey_tap()];
}

#pragma mark - Test top toolbar buttons

// Tests that the 'Add' button in the top toolbar is disabled by default.
- (void)testAddButtonDisabledOnDefault {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

// Tests that the 'Cancel' button dismisses the screen.
// TODO(crbug.com/40157443): test flaky on iPads.
- (void)testCancelButtonDismissesScreen {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPad.");
  }

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
      performAction:grey_replaceText(@"1234")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

// Tests when a user tries to add an invalid card number, the "Add" button is
// not enabled.
- (void)testAddButtonDisabledOnInvalidExpiryDate {
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_replaceText(@"4111111111111111")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_replaceText(@"00")];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_replaceText(@"0000")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

// Tests when a user tries to add an invalid card nickname, the "Add" button is
// not enabled.
- (void)testAddButtonDisabledOnInvalidNickname {
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_replaceText(@"4111111111111111")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_replaceText(@"12")];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_replaceText(@"2030")];
  [[EarlGrey selectElementWithMatcher:NicknameTextField()]
      performAction:grey_replaceText(@"1234")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

// Tests when a user tries to add an empty card nickname, the "Add" button is
// enabled.
- (void)testAddButtonEnabledOnEmptyNickname {
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_replaceText(@"4111111111111111")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_replaceText(@"12")];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_replaceText(@"2030")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];
}

// Tests when a user tries to add a valid card number, the screen is dismissed
// and the new card number appears on the Autofill Credit Card 'Payment Methods'
// screen.
// TODO(crbug.com/40157443): test flaky on iPads.
- (void)testAddButtonOnValidNumber {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPad.");
  }
  [AutofillAppInterface clearCreditCardStore];
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_replaceText(@"4111111111111111")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_replaceText(@"12")];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_replaceText(@"2999")];

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

// Tests when a user add a card with a nickname, the screen is dismissed
// and the new card number appears on the Autofill Credit Card 'Payment Methods'
// screen with the nickname.
- (void)testAddButtonOnValidNickname {
  [AutofillAppInterface clearCreditCardStore];
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_replaceText(@"4111111111111111")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_replaceText(@"12")];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_replaceText(@"2999")];
  [[EarlGrey selectElementWithMatcher:NicknameTextField()]
      performAction:grey_replaceText(@"Fav Card")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      performAction:grey_tap()];

  NSString* newCreditCardObjectLabel =
      @", Fav Card  ‪•⁠ ⁠•⁠ ⁠•⁠ ⁠•⁠ ⁠1111‬";
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
      performAction:grey_replaceText(@"1234")];
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
  // TODO(crbug.com/40916973): Revert to grey_clearText when fixed in EG.
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_replaceText(@"")];
  [[EarlGrey selectElementWithMatcher:CardNumberIconView(kEditIconIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that add button is disabled until typing a single character makes all
// the fields valid.
- (void)testAddButtonDisabledTillValidForm {
  [[EarlGrey selectElementWithMatcher:CardNumberTextField()]
      performAction:grey_replaceText(@"4111111111111111")];
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryTextField()]
      performAction:grey_replaceText(@"12")];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_replaceText(@"299")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
  [[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
      performAction:grey_replaceText(@"2999")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];
}

@end
