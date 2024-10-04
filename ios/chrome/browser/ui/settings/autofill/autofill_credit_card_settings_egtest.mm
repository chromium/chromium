// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::PaymentMethodsButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsToolbarAddButton;
using chrome_test_util::TabGridEditButton;

namespace {

// Expectation of how the saved Autofill credit card looks like, a map from cell
// name IDs to expected contents.
struct DisplayStringIDToExpectedResult {
  int display_string_id;
  NSString* expected_result;
};

using autofill::autofill_metrics::MandatoryReauthAuthenticationFlowEvent;

const DisplayStringIDToExpectedResult kExpectedFields[] = {
    {IDS_IOS_AUTOFILL_CARDHOLDER, @"Test User"},
    {IDS_IOS_AUTOFILL_CARD_NUMBER, @"4111111111111111"},
    {IDS_IOS_AUTOFILL_EXP_MONTH,
     base::SysUTF8ToNSString(autofill::test::NextMonth())},
    {IDS_IOS_AUTOFILL_EXP_YEAR,
     base::SysUTF8ToNSString(autofill::test::NextYear())}};

NSString* const kCreditCardLabelTemplate = @"Test User, %@";

NSString* const kServerCardHolderName = @"Bonnie Parker";

NSString* const kMandatoryReauthOptOutHistogramName =
    @"Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent.SettingsPage."
    @"OptOut";
NSString* const kMandatoryReauthOptInHistogramName =
    @"Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent.SettingsPage."
    @"OptIn";
NSString* const kMandatoryReauthEditCardHistogramName =
    @"Autofill.PaymentMethods.MandatoryReauth.AuthEvent.SettingsPage.EditCard";
NSString* const kMandatoryReauthDeleteCardHistogramName =
    @"Autofill.PaymentMethods.MandatoryReauth.AuthEvent.SettingsPage."
    @"DeleteCard";

// Return the edit button from the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
      grey_not(TabGridEditButton()),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Matcher for the Delete button in the list view, located at the bottom of the
// screen.
id<GREYMatcher> BottomToolbar() {
  return grey_accessibilityID(kAutofillPaymentMethodsToolbarId);
}

}  // namespace

// Various tests for the Autofill credit cards section of the settings.
@interface AutofillCreditCardSettingsTestCase : ChromeTestCase
@end

@implementation AutofillCreditCardSettingsTestCase

- (void)setUp {
  [super setUp];

  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface setMandatoryReauthEnabled:YES];

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearMockReauthenticationModule];

  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");

  [super tearDown];
}

// Returns the label for `creditCard` in the settings page for Autofill credit
// cards.
- (NSString*)creditCardLabel:(NSString*)lastDigits {
  return [NSString stringWithFormat:kCreditCardLabelTemplate, lastDigits];
}

// Helper to open the settings page for Autofill credit cards.
- (void)openCreditCardsSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];
}

// Helper to open the settings page for the Autofill credit card with `label`.
- (void)openEditCreditCard:(NSString*)label {
  [self openCreditCardsSettings];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:grey_tap()];
}

// Helper to open the settings page for the Autofill credit card list in edit
// mode.
- (void)openCreditCardListInEditMode {
  [self openCreditCardsSettings];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

// Close the settings.
- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Test that the page for viewing Autofill credit card details is as expected
// when Mandatory Reauth is enabled.
- (void)testCreditCardViewPageMandatoryReauthEnabled {
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openEditCreditCard:[self creditCardLabel:lastDigits]];

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
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           MandatoryReauthAuthenticationFlowEvent::kFlowStarted)
          forHistogram:kMandatoryReauthEditCardHistogramName],
      @"Mandatory reauth edit card flow started event count incorrect");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(MandatoryReauthAuthenticationFlowEvent::
                                            kFlowSucceeded)
          forHistogram:kMandatoryReauthEditCardHistogramName],
      @"Mandatory reauth edit card flow result event count incorrect");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:0
             forBucket:static_cast<int>(
                           MandatoryReauthAuthenticationFlowEvent::kFlowFailed)
          forHistogram:kMandatoryReauthEditCardHistogramName],
      @"Mandatory reauth edit card flow result event count incorrect");
  [self exitSettingsMenu];
}

// Test that the page for viewing Autofill credit card details is as expected
// when Mandatory Reauth is disabled.
- (void)testCreditCardViewPageMandatoryReauthDisabled {
  [AutofillAppInterface setMandatoryReauthEnabled:FALSE];
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openEditCreditCard:[self creditCardLabel:lastDigits]];

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
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for viewing Autofill credit card details is not reached
// if the Mandatory Reauth feature is enabled and the user fails the
// authentication prompt.
- (void)testCreditCardViewPageMandatoryReauthFailed {
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kFailure];
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openEditCreditCard:[self creditCardLabel:lastDigits]];

  // Confirm that we have not reached the card details page by confirming that
  // none of its fields are present.
  for (const DisplayStringIDToExpectedResult& expectation : kExpectedFields) {
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityLabel([NSString
                       stringWithFormat:@"%@, %@",
                                        l10n_util::GetNSString(
                                            expectation.display_string_id),
                                        expectation.expected_result])]
        assertWithMatcher:grey_nil()];
  }

  // Confirm we are still on the credit card settings page by confirming the
  // presence of the mandatory reauth toggle.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillCreditCardSwitchViewId, YES, YES)]
      assertWithMatcher:grey_notNil()];

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           MandatoryReauthAuthenticationFlowEvent::kFlowStarted)
          forHistogram:kMandatoryReauthEditCardHistogramName],
      @"Mandatory reauth edit card flow started event count incorrect");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           MandatoryReauthAuthenticationFlowEvent::kFlowFailed)
          forHistogram:kMandatoryReauthEditCardHistogramName],
      @"Mandatory reauth edit card flow result event count incorrect");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:0
             forBucket:static_cast<int>(MandatoryReauthAuthenticationFlowEvent::
                                            kFlowSucceeded)
          forHistogram:kMandatoryReauthEditCardHistogramName],
      @"Mandatory reauth edit card flow result event count incorrect");
  [self exitSettingsMenu];
}

// Test that reaching the credit card details page for a server card does not
// require reauthentication.
- (void)testServerCardViewSkipsMandatoryReauth {
  [AutofillAppInterface saveMaskedCreditCard];
  [self openEditCreditCard:kServerCardHolderName];

  // Confirm we have arrived at the card details page by specifying the presence
  // of the cardholder name field and its correct value.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel([NSString
                     stringWithFormat:@"%@, %@",
                                      l10n_util::GetNSString(
                                          IDS_IOS_AUTOFILL_CARDHOLDER),
                                      kServerCardHolderName])]
      assertWithMatcher:grey_notNil()];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [AutofillAppInterface clearAllServerDataForTesting];
  [self exitSettingsMenu];
}

// Test that the page for viewing Autofill credit card details is accessible.
// TODO(crbug.com/366085550): Re-enable the test.
- (void)DISABLED_testAccessibilityOnCreditCardViewPage {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];
  [self openEditCreditCard:[self creditCardLabel:lastDigits]];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for editing Autofill credit card details is accessible.
- (void)testAccessibilityOnCreditCardEditPage {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];
  [self openEditCreditCard:[self creditCardLabel:lastDigits]];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Leave edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Checks that the Autofill credit cards list view is in edit mode and the
// Autofill credit cards / mandatory reauth switches are disabled.
- (void)testListViewEditMode {
  [AutofillAppInterface saveLocalCreditCard];
  for (ReauthenticationResult result :
       {ReauthenticationResult::kFailure, ReauthenticationResult::kSuccess,
        ReauthenticationResult::kSkipped}) {
    // Reset the HistogramTester at the beginning of each run.
    GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                  @"Cannot reset histogram tester.");
    GREYAssertNil([MetricsAppInterface setupHistogramTester],
                  @"Cannot setup histogram tester.");

    [self openCreditCardsSettings];

    [AutofillAppInterface mockReauthenticationModuleExpectedResult:result];

    // Switch on edit mode.
    [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
        performAction:grey_tap()];

    // Check the Autofill credit card switch is enabled if the reauthentication
    // result is a failure.
    bool enabled = (result == ReauthenticationResult::kFailure);
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                            kAutofillCreditCardSwitchViewId,
                                            YES, enabled)]
        assertWithMatcher:grey_notNil()];

    // Check the Autofill mandatory reauth switch is enabled if the
    // reauthentication result is a failure.
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                     kAutofillMandatoryReauthSwitchViewId, YES,
                                     enabled)] assertWithMatcher:grey_notNil()];

    GREYAssertNil([MetricsAppInterface
                      expectTotalCount:2
                          forHistogram:kMandatoryReauthDeleteCardHistogramName],
                  @"Mandatory reauth delete card event count incorrect");
    GREYAssertNil(
        [MetricsAppInterface
             expectCount:1
               forBucket:
                   static_cast<int>(
                       MandatoryReauthAuthenticationFlowEvent::kFlowStarted)
            forHistogram:kMandatoryReauthDeleteCardHistogramName],
        @"Mandatory reauth delete card flow-started event count incorrect");

    MandatoryReauthAuthenticationFlowEvent event;
    switch (result) {
      case ReauthenticationResult::kFailure:
        event = MandatoryReauthAuthenticationFlowEvent::kFlowFailed;
        break;
      case ReauthenticationResult::kSuccess:
        event = MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded;
        break;
      case ReauthenticationResult::kSkipped:
        event = MandatoryReauthAuthenticationFlowEvent::kFlowSkipped;
        break;
    }
    GREYAssertNil(
        [MetricsAppInterface
             expectCount:1
               forBucket:static_cast<int>(event)
            forHistogram:kMandatoryReauthDeleteCardHistogramName],
        @"Mandatory reauth delete card flow result event count incorrect");

    [self exitSettingsMenu];
  }
}

// Checks that the Autofill credit card switch can be toggled on/off and the
// list of Autofill credit cards is not affected by it.
- (void)testToggleCreditCardSwitch {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openCreditCardsSettings];

  // Toggle the Autofill credit cards switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillCreditCardSwitchViewId, YES, YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Expect Autofill credit cards to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      assertWithMatcher:grey_notNil()];

  // Toggle the Autofill credit cards switch back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillCreditCardSwitchViewId, NO, YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Expect Autofill credit cards to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Checks that the Autofill credit card switch can be turned off and the add
// payment method button in the toolbar is disabled.
- (void)testToggleCreditCardSwitchPaymentMethodDisabled {
  [self openCreditCardsSettings];

  // Toggle the Autofill credit cards switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillCreditCardSwitchViewId, YES, YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Expect Add Payment Method button to be disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsToolbarAddButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Toggle the Autofill credit cards switch back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillCreditCardSwitchViewId, NO, YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Expect Add Payment Method button to be visible.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsToolbarAddButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [self exitSettingsMenu];
}

// Checks that when the Autofill credit card switch can be turned off and the
// edit button is pressed, Add Payment Method button is removed from the
// toolbar.
- (void)testToggleCreditCardSwitchInEditModePaymentMethodRemoved {
  [AutofillAppInterface saveLocalCreditCard];
  [self openCreditCardsSettings];

  // Toggle the Autofill credit cards switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillCreditCardSwitchViewId, YES, YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  // Open Edit Mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  // Expect Add Payment Method to be removed.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsToolbarAddButton()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Checks that the toolbar always appears in edit mode.
- (void)testToolbarInEditModeAddPaymentMethodFeatureEnabled {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];
  [self openCreditCardListInEditMode];

  [[EarlGrey selectElementWithMatcher:BottomToolbar()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BottomToolbar()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BottomToolbar()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks the 'Delete' button is always visible.
// The button is enabled when a card is selected and disabled when a card is not
// selected.
- (void)testToolbarDeleteButtonWithAddPaymentMethodFeatureEnabled {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];
  [self openCreditCardListInEditMode];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Checks that deleting a card exits from edit mode.
- (void)testDeletingCreditCard {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];
  [self openCreditCardListInEditMode];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_nil()];
  // If the done button in the nav bar is enabled it is no longer in edit
  // mode.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that switching the mandatory reauth toggle triggers the reauth. If
// reauth succeeded, reauth preference and the toggle state are updated.
- (void)testUpdateReauthToggle {
  [AutofillAppInterface setMandatoryReauthEnabled:YES];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];

  [self openCreditCardsSettings];

  // Check the reauth switch is there.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          YES, YES)]
      assertWithMatcher:grey_notNil()];

  // Config the next reauth attempt's result to failure.
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kFailure];

  // Switch off the reauth toggle.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          YES, YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          YES, YES)]
      assertWithMatcher:grey_notNil()];

  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:2
                        forHistogram:kMandatoryReauthOptOutHistogramName],
                @"Mandatory reauth toggle event count incorrect");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           MandatoryReauthAuthenticationFlowEvent::kFlowStarted)
          forHistogram:kMandatoryReauthOptOutHistogramName],
      @"Mandatory reauth toggle flow started event count incorrect");

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           MandatoryReauthAuthenticationFlowEvent::kFlowFailed)
          forHistogram:kMandatoryReauthOptOutHistogramName],
      @"Mandatory reauth toggle flow result event count incorrect");

  // Config the next reauth attempt's result to success.
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  // Switch off the reauth toggle.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          YES, YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          NO, YES)]
      assertWithMatcher:grey_notNil()];

  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:4
                        forHistogram:kMandatoryReauthOptOutHistogramName],
                @"Mandatory reauth toggle event count incorrect");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:2
             forBucket:static_cast<int>(
                           MandatoryReauthAuthenticationFlowEvent::kFlowStarted)
          forHistogram:kMandatoryReauthOptOutHistogramName],
      @"Mandatory reauth toggle flow started event count incorrect");

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           MandatoryReauthAuthenticationFlowEvent::kFlowFailed)
          forHistogram:kMandatoryReauthOptOutHistogramName],
      @"Mandatory reauth toggle flow result event count incorrect");

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(MandatoryReauthAuthenticationFlowEvent::
                                            kFlowSucceeded)
          forHistogram:kMandatoryReauthOptOutHistogramName],
      @"Mandatory reauth toggle flow result event count incorrect");

  // Switch on the reauth toggle. Reauth will be skipped due to previous
  // success.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          NO, YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          YES, YES)]
      assertWithMatcher:grey_notNil()];

  GREYAssertNil(
      [MetricsAppInterface expectTotalCount:2
                               forHistogram:kMandatoryReauthOptInHistogramName],
      @"Mandatory reauth toggle event count incorrect");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           MandatoryReauthAuthenticationFlowEvent::kFlowStarted)
          forHistogram:kMandatoryReauthOptInHistogramName],
      @"Mandatory reauth toggle flow started event count incorrect");

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(MandatoryReauthAuthenticationFlowEvent::
                                            kFlowSucceeded)
          forHistogram:kMandatoryReauthOptInHistogramName],
      @"Mandatory reauth toggle flow result event count incorrect");

  [self exitSettingsMenu];
}

// Checks that switching the mandatory reauth toggle when reauth is not
// available will disable the toggle and set it to switched-off state.
- (void)testDisableReauthToggle {
  [AutofillAppInterface setMandatoryReauthEnabled:YES];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [self openCreditCardsSettings];

  // Check the reauth switch is there.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          YES, YES)]
      assertWithMatcher:grey_notNil()];

  // Mock that reauth is disabled.
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:NO];

  // Try to switch off reauth toggle.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          YES, YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Reauth toggle should be disabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kAutofillMandatoryReauthSwitchViewId,
                                          NO, NO)]
      assertWithMatcher:grey_notNil()];

  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:0
                        forHistogram:kMandatoryReauthOptOutHistogramName],
                @"Mandatory reauth toggle event count incorrect");
  GREYAssertNil(
      [MetricsAppInterface expectTotalCount:0
                               forHistogram:kMandatoryReauthOptInHistogramName],
      @"Mandatory reauth toggle event count incorrect");
}

@end
