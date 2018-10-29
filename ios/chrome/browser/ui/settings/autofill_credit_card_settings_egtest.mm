// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;

namespace {

// Expectation of how the saved Autofill credit card looks like, a map from cell
// name IDs to expected contents.
struct DisplayStringIDToExpectedResult {
  int display_string_id;
  NSString* expected_result;
};

const DisplayStringIDToExpectedResult kExpectedFields[] = {
    {IDS_IOS_AUTOFILL_CARDHOLDER, @"Test User"},
    {IDS_IOS_AUTOFILL_CARD_NUMBER, @"4111111111111111"},
    {IDS_IOS_AUTOFILL_EXP_MONTH, @"11"},
    {IDS_IOS_AUTOFILL_EXP_YEAR, @"2022"}};

NSString* const kCreditCardLabelTemplate = @"Test User, %@";

}  // namespace

// Various tests for the Autofill credit cards section of the settings.
@interface AutofillCreditCardSettingsTestCase : ChromeTestCase
@end

@implementation AutofillCreditCardSettingsTestCase {
  // The PersonalDataManager instance for the current browser state.
  autofill::PersonalDataManager* _personalDataManager;
}

- (void)setUp {
  [super setUp];

  _personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  _personalDataManager->SetSyncingForTest(true);
}

- (void)tearDown {
  // Clear existing credit cards.
  for (const auto* creditCard : _personalDataManager->GetCreditCards()) {
    _personalDataManager->RemoveByGUID(creditCard->guid());
  }

  [super tearDown];
}

- (autofill::CreditCard)addCreditCard {
  autofill::CreditCard creditCard = autofill::test::GetCreditCard();
  size_t creditCardCount = _personalDataManager->GetCreditCards().size();
  _personalDataManager->AddCreditCard(creditCard);
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool() {
                   return creditCardCount <
                          _personalDataManager->GetCreditCards().size();
                 }),
             @"Failed to add credit card.");
  return creditCard;
}

// Returns the label for |creditCard| in the settings page for Autofill credit
// cards.
- (NSString*)creditCardLabel:(const autofill::CreditCard&)creditCard {
  return [NSString stringWithFormat:kCreditCardLabelTemplate,
                                    base::SysUTF16ToNSString(
                                        creditCard.NetworkAndLastFourDigits())];
}

// Helper to open the settings page for Autofill credit cards.
- (void)openCreditCardsSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          l10n_util::GetNSString(
                                              IDS_AUTOFILL_PAYMENT_METHODS))]
      performAction:grey_tap()];
}

// Helper to open the settings page for the Autofill credit card with |label|.
- (void)openEditCreditCard:(NSString*)label {
  [self openCreditCardsSettings];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:grey_tap()];
}

// Close the settings.
- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Test that the page for viewing Autofill credit card details is as expected.
- (void)testCreditCardViewPage {
  autofill::CreditCard creditCard = [self addCreditCard];
  [self openEditCreditCard:[self creditCardLabel:creditCard]];

  // Check that all fields and values match the expectations.
  for (const DisplayStringIDToExpectedResult& expectation : kExpectedFields) {
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityLabel([NSString
                       stringWithFormat:@"%@, %@",
                                        l10n_util::GetNSString(
                                            expectation.display_string_id),
                                        expectation.expected_result])]
        assertWithMatcher:grey_notNil()];
  }

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for viewing Autofill credit card details is accessible.
- (void)testAccessibilityOnCreditCardViewPage {
  autofill::CreditCard creditCard = [self addCreditCard];
  [self openEditCreditCard:[self creditCardLabel:creditCard]];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for editing Autofill credit card details is accessible.
- (void)testAccessibilityOnCreditCardEditPage {
  autofill::CreditCard creditCard = [self addCreditCard];
  [self openEditCreditCard:[self creditCardLabel:creditCard]];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Checks that the Autofill credit cards list view is in edit mode and the
// Autofill credit cards switch is disabled.
- (void)testListViewEditMode {
  autofill::CreditCard creditCard = [self addCreditCard];
  [self openCreditCardsSettings];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];

  // Check the Autofill credit card switch is disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                   @"cardItem_switch", YES, NO)]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Checks that the Autofill credit card switch can be toggled on/off and the
// list of Autofill credit cards is not affected by it.
- (void)testToggleCreditCardSwitch {
  autofill::CreditCard creditCard = [self addCreditCard];
  [self openCreditCardsSettings];

  // Toggle the Autofill credit cards switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                   @"cardItem_switch", YES, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Expect Autofill credit cards to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:creditCard])]
      assertWithMatcher:grey_notNil()];

  // Toggle the Autofill credit cards switch back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                   @"cardItem_switch", NO, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Expect Autofill credit cards to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:creditCard])]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

@end
