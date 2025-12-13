// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/third_party/earl_grey2/src/AppFramework/Matcher/GREYMatchers.h"
#import "ios/third_party/earl_grey2/src/CommonLib/Matcher/GREYElementMatcherBlock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::PaymentMethodsButton;
using chrome_test_util::TextFieldForCellWithLabelId;

// Tests for Settings Autofill edit credit cards screen.
@interface AutofillEditCreditCardTestCase : ChromeTestCase
@end

namespace {

// Matcher for the 'Nickname' text field in the edit credit card view.
id<GREYMatcher> NicknameTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_NICKNAME);
}

// Matcher for the edit button in the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
      grey_not(chrome_test_util::TabGridEditButton()),
      grey_kindOfClass([UIButton class]),
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), nil);
}

// Returns an action to scroll down (swipe up).
id<GREYAction> ScrollDown() {
  return grey_scrollInDirection(kGREYDirectionDown, 150);
}

// Matcher for the 'Year of Expiry' text field in the edit credit card view.
id<GREYMatcher> YearOfExpiryTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_EXP_YEAR);
}

// Matcher for the 'CVC' text field in the edit credit card view.
id<GREYMatcher> CvcTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_SECURITY_CODE);
}

}  // namespace

@implementation AutofillEditCreditCardTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillEnableCvcStorageAndFilling);
  // Add feature configs here.
  return config;
}

- (void)setUp {
  [super setUp];

  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface setMandatoryReauthEnabled:YES];

  [AutofillAppInterface clearCreditCardStore];
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(lastDigits)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

- (void)tearDownHelper {
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearMockReauthenticationModule];
  [super tearDownHelper];
}

#pragma mark - Test that all fields on the 'Add Credit Card' screen appear

// Tests that editing the credit card nickname is possible.
- (void)testValidNickname {
  [self typeNickname:@"Nickname"];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that invalid nicknames are not allowed when editing a card.
- (void)testInvalidNickname {
  [self typeNickname:@"1233"];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

// Tests that clearing a nickname is allowed.
- (void)testEmptyNickname {
  [self typeNickname:@"To be removed"];

  [[EarlGrey selectElementWithMatcher:NicknameTextField()]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];
}

// Tests that editing the credit card CVC is possible.
- (void)testValidCvc {
  [self typeCvc:@"123"];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that invalid CVC are not allowed when editing a card.
- (void)testInvalidCVC {
  [self typeCvc:@"00000"];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

// Tests that clearing a CVC is allowed.
- (void)testEmptyCvc {
  [self typeCvc:@"123"];

  [[EarlGrey selectElementWithMatcher:CvcTextField()]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];
}

// Tests that the Done button in the navigation bar is disabled on entering
// invalid year of expiry in the edit credit card form.
- (void)testDoneOnInvalidYearInEditCreditCard {
  [[[EarlGrey selectElementWithMatcher:YearOfExpiryTextField()]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::AutofillCreditCardEditTableView()]
      performAction:grey_replaceText(@"2000")];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
}

#pragma mark - Helper methods

// Scrolls to nickname text field and types the string.
- (void)typeNickname:(NSString*)nickname {
  [[[EarlGrey selectElementWithMatcher:NicknameTextField()]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::AutofillCreditCardEditTableView()]
      performAction:grey_replaceText(nickname)];
}

// Scrolls to CVC text field and types the string.
- (void)typeCvc:(NSString*)cvc {
  [[[EarlGrey selectElementWithMatcher:CvcTextField()]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::AutofillCreditCardEditTableView()]
      performAction:grey_replaceText(cvc)];
}

@end

@interface AutofillEditCreditCardCvcMetricTestCase : ChromeTestCase
@end

@implementation AutofillEditCreditCardCvcMetricTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillEnableCvcStorageAndFilling);
  return config;
}

- (void)setUp {
  [super setUp];
  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface setMandatoryReauthEnabled:YES];
  [AutofillAppInterface clearCreditCardStore];
}

- (void)tearDownHelper {
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearMockReauthenticationModule];
  [super tearDownHelper];
}

// Helper to navigate to the edit screen for a given card
- (void)navigateToEditCard:(NSString*)lastDigits cvcIsSaved:(BOOL)cvcIsSaved {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];

  // Construct the expected accessibility label
  NSString* expectedLabel = lastDigits;
  if (cvcIsSaved) {
    expectedLabel = [expectedLabel stringByAppendingString:@" | CVC saved"];
  }

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(expectedLabel)]
      performAction:grey_tap()];

  // Tap the Edit button to proceed to the edit screen.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

// Scrolls to CVC text field and types the string.
- (void)typeCvc:(NSString*)cvc {
  [[[EarlGrey selectElementWithMatcher:CvcTextField()]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::AutofillCreditCardEditTableView()]
      performAction:grey_replaceText(cvc)];
}

// Tests that the correct metric is logged when a CVC is added.
- (void)testMetricCvcAdded {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self navigateToEditCard:lastDigits cvcIsSaved:NO];

  base::UserActionTester userActionTester;
  [self typeCvc:@"123"];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  EXPECT_EQ(1, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasAdded"));
  EXPECT_EQ(0, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasLeftBlank"));
}

// Tests that the correct metric is logged when a card without a CVC is saved
// without adding one.
- (void)testMetricCvcLeftBlank {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self navigateToEditCard:lastDigits cvcIsSaved:NO];

  base::UserActionTester userActionTester;

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  EXPECT_EQ(1, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasLeftBlank"));
  EXPECT_EQ(0, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasAdded"));
}

// Tests that the correct metric is logged when a CVC is removed.
- (void)testMetricCvcRemoved {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCardWithCvc];
  [self navigateToEditCard:lastDigits cvcIsSaved:YES];

  base::UserActionTester userActionTester;
  [self typeCvc:@""];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  EXPECT_EQ(1, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasRemoved"));
  EXPECT_EQ(0, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasUpdated"));
  EXPECT_EQ(0, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasUnchanged"));
}

// Tests that the correct metric is logged when a CVC is updated.
- (void)testMetricCvcUpdated {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCardWithCvc];
  [self navigateToEditCard:lastDigits cvcIsSaved:YES];

  base::UserActionTester userActionTester;
  [self typeCvc:@"456"];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  EXPECT_EQ(1, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasUpdated"));
  EXPECT_EQ(0, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasUnchanged"));
  EXPECT_EQ(0, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasRemoved"));
}

// Tests that the correct metric is logged when a CVC is not changed.
- (void)testMetricCvcUnchanged {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCardWithCvc];
  [self navigateToEditCard:lastDigits cvcIsSaved:YES];

  base::UserActionTester userActionTester;

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  EXPECT_EQ(1, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasUnchanged"));
  EXPECT_EQ(0, userActionTester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasUpdated"));
}

@end
