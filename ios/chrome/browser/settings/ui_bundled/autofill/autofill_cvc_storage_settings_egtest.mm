// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/metrics/user_action_tester.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::PaymentMethodsButton;
using chrome_test_util::TextFieldForCellWithLabelId;

namespace {

NSString* kCardIdentifier = @"Test User";

// Matcher for the CVC storage button on the Payment Methods screen.
id<GREYMatcher> CvcStorageButton() {
  return grey_text(l10n_util::GetNSString(
      IDS_PAYMENTS_AUTOFILL_ENABLE_SAVE_SECURITY_CODES_LABEL));
}

// Matcher for the delete button in the confirmation dialog.
id<GREYMatcher> DeleteConfirmationButton() {
  id<GREYMatcher> baseMatcher = grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_DELETE_SAVED_SECURITY_CODE)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_userInteractionEnabled(), nil);
  return grey_allOf(baseMatcher, grey_not(grey_descendant(baseMatcher)), nil);
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

// Tests that the delete CVCs button is not visible when no CVCs are saved.
- (void)testDeleteButtonIsNotVisible {
  [self openCvcStorageSettings];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillDeleteSecurityCodesButtonId)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the delete CVCs button is visible when CVCs are saved.
- (void)testDeleteButtonIsVisible {
  // Create & save local credit card.
  [AutofillAppInterface saveLocalCreditCardWithCvc];
  [self openCvcStorageSettings];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillDeleteSecurityCodesButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping the delete button and confirming deletes the CVCs.
- (void)testDeleteButtonDeletesCvcs {
  // Create & save local credit card with CVC.
  [AutofillAppInterface saveLocalCreditCardWithCvc];
  [self openCvcStorageSettings];
  // Verify delete button is visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillDeleteSecurityCodesButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap delete button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillDeleteSecurityCodesButtonId)]
      performAction:grey_tap()];

  // Tap confirmation button.
  id<GREYMatcher> confirmationButton = DeleteConfirmationButton();
  [[EarlGrey selectElementWithMatcher:confirmationButton]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:confirmationButton];

  // Verify delete button is not visible anymore.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillDeleteSecurityCodesButtonId)]
      assertWithMatcher:grey_notVisible()];

  // Go back to the payment methods screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton(
                                          0)] performAction:grey_tap()];

  // Verify that the card still exists but CVC indicator is gone.
  id<GREYMatcher> cardCellMatcher =
      grey_allOf(grey_accessibilityID(kCardIdentifier),
                 grey_accessibilityTrait(UIAccessibilityTraitButton),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:cardCellMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  NSString* cvcIndicator = l10n_util::GetNSString(
      IDS_AUTOFILL_SETTINGS_PAGE_CVC_TAG_FOR_CREDIT_CARD_LIST_ENTRY);

  [[EarlGrey selectElementWithMatcher:cardCellMatcher]
      assertWithMatcher:grey_not(grey_descendant(grey_text(cvcIndicator)))];
}

// Tests that the user actions for the delete CVCs flow are recorded correctly.
// TODO(crbug.com/451607065): Test is disabled on device.
#if !TARGET_OS_SIMULATOR
#define MAYBE_testDeleteActionsAreRecorded DISABLED_testDeleteActionsAreRecorded
#else
#define MAYBE_testDeleteActionsAreRecorded testDeleteActionsAreRecorded
#endif
- (void)MAYBE_testDeleteActionsAreRecorded {
  base::UserActionTester userActionTester;

  // Create & save local credit card.
  [AutofillAppInterface saveLocalCreditCardWithCvc];
  [self openCvcStorageSettings];

  // Verify the delete button is visible before proceeding.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillDeleteSecurityCodesButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the delete button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillDeleteSecurityCodesButtonId)]
      performAction:grey_tap()];

  EXPECT_EQ(1,
            userActionTester.GetActionCount("BulkCvcDeletionHyperlinkClicked"));

  // Tap outside the action sheet to dismiss it.
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_tapAtPoint(CGPointMake(1, 1))];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:DeleteConfirmationButton()];

  EXPECT_EQ(1,
            userActionTester.GetActionCount("BulkCvcDeletionHyperlinkClicked"));

  EXPECT_EQ(1, userActionTester.GetActionCount(
                   "BulkCvcDeletionConfirmationDialogCancelled"));

  // Tap the delete button again.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillDeleteSecurityCodesButtonId)]
      performAction:grey_tap()];

  EXPECT_EQ(1,
            userActionTester.GetActionCount("BulkCvcDeletionHyperlinkClicked"));
  EXPECT_EQ(1, userActionTester.GetActionCount(
                   "BulkCvcDeletionConfirmationDialogCancelled"));
  EXPECT_EQ(1,
            userActionTester.GetActionCount("BulkCvcDeletionHyperlinkClicked"));

  // Tap the "Delete" confirmation button.
  id<GREYMatcher> confirmationButton = DeleteConfirmationButton();
  [[EarlGrey selectElementWithMatcher:confirmationButton]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:confirmationButton];

  EXPECT_EQ(1,
            userActionTester.GetActionCount("BulkCvcDeletionHyperlinkClicked"));
  EXPECT_EQ(1, userActionTester.GetActionCount(
                   "BulkCvcDeletionConfirmationDialogCancelled"));
  EXPECT_EQ(1,
            userActionTester.GetActionCount("BulkCvcDeletionHyperlinkClicked"));
  EXPECT_EQ(1, userActionTester.GetActionCount(
                   "BulkCvcDeletionConfirmationDialogAccepted"));
}

@end
