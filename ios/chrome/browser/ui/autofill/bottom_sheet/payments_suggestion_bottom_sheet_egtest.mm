// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util_mac.h"

using chrome_test_util::TextFieldForCellWithLabelId;

namespace {

const char kFormCardName[] = "CCName";

using base::test::ios::kWaitForActionTimeout;

BOOL WaitForKeyboardToAppear() {
  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard"
                  block:^BOOL {
                    return [EarlGrey isKeyboardShownWithError:nil];
                  }];
  return [waitForKeyboard waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
}

}  // namespace

@interface PaymentsSuggestionBottomSheetEGTest : ChromeTestCase
@end

@implementation PaymentsSuggestionBottomSheetEGTest {
  // Last digits of the credit card
  NSString* _lastDigits;
}

- (void)setUp {
  [super setUp];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  [AutofillAppInterface clearCreditCardStore];
  _lastDigits = [AutofillAppInterface saveLocalCreditCard];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kIOSPaymentsBottomSheet);
  return config;
}

// Matcher for the bottom sheet's "Continue" button.
id<GREYMatcher> ContinueButton() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PAYMENT_BOTTOM_SHEET_CONTINUE));
}

// Matcher for the bottom sheet's "No Thanks" button.
id<GREYMatcher> NoThanksButton() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PAYMENT_BOTTOM_SHEET_NO_THANKS));
}

// Matcher for the toolbar's edit button.
id<GREYMatcher> SettingToolbarEditButton() {
  return grey_accessibilityID(kSettingsToolbarEditButtonId);
}

// Matcher for the toolbar's done button.
id<GREYMatcher> SettingToolbarDoneButton() {
  return grey_accessibilityID(kSettingsToolbarEditDoneButtonId);
}

// Matcher for the toolbar's delete button.
id<GREYMatcher> SettingToolbarDeleteButton() {
  return grey_accessibilityID(kSettingsToolbarDeleteButtonId);
}

// Matcher for the card nickname's text field.
id<GREYMatcher> NicknameTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_NICKNAME);
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadPaymentsPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/credit_card.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Autofill Test"];
}

#pragma mark - Tests

// Tests that the Payments Bottom Sheet appears when tapping on a credit card
// related field.
- (void)testOpenPaymentsBottomSheetUseCreditCard {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = ContinueButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];

  [[EarlGrey selectElementWithMatcher:continueButton] performAction:grey_tap()];
}

// Tests that the Payments Bottom Sheet updates its contents when a new credit
// card becomes available in the personal data manager.
- (void)testUpdateBottomSheetOnAddServerCreditCard {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = ContinueButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];

  // Add a credit card to the Personal Data Manager.
  id<GREYMatcher> serverCreditCardEntry =
      grey_text([AutofillAppInterface saveMaskedCreditCard]);

  // Make sure the Bottom Sheet has been updated with the new credit card.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:serverCreditCardEntry];

  // Make sure the initial credit card is still there.
  id<GREYMatcher> localCreditCardEntry = grey_text(_lastDigits);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:localCreditCardEntry];

  // Select and use the new credit card.
  [[EarlGrey selectElementWithMatcher:serverCreditCardEntry]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:continueButton] performAction:grey_tap()];
}

// Tests that accessing a long press menu does not disable the bottom sheet.
- (void)testOpenPaymentsBottomSheetAfterLongPress {
  [self loadPaymentsPage];

  // Open the Payments Bottom Sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = ContinueButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];

  // Long press to open context menu.
  id<GREYMatcher> creditCardEntry = grey_text(_lastDigits);

  [[EarlGrey selectElementWithMatcher:creditCardEntry]
      performAction:grey_longPress()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithAccessibilityLabel(
                  l10n_util::GetNSString(
                      IDS_IOS_PAYMENT_BOTTOM_SHEET_MANAGE_PAYMENT_METHODS)),
              grey_interactable(), nullptr)] performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Close the context menu.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Try to open the Payments Bottom Sheet again.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  // Make sure the bottom sheet re-opens.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];
}

// Verify that the Payments Bottom Sheet works in incognito mode.
- (void)testOpenPaymentsBottomSheetIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = ContinueButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];

  [[EarlGrey selectElementWithMatcher:continueButton] performAction:grey_tap()];
}

// Verify that the Payments Bottom Sheet "No Thanks" button opens the keyboard.
- (void)testOpenPaymentsBottomSheetTapNoThanksShowKeyboard {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> noThanksButton = NoThanksButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:noThanksButton];

  [[EarlGrey selectElementWithMatcher:noThanksButton] performAction:grey_tap()];

  WaitForKeyboardToAppear();
}

// Verify that the Payments Bottom Sheet "Show Details" button opens the proper
// menu and allows the nickname to be edited.
- (void)testOpenPaymentsBottomSheetShowDetailsEditNickname {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = ContinueButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];

  // Long press to open context menu.
  id<GREYMatcher> creditCardEntry = grey_text(_lastDigits);

  [[EarlGrey selectElementWithMatcher:creditCardEntry]
      performAction:grey_longPress()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(
                                l10n_util::GetNSString(
                                    IDS_IOS_PAYMENT_BOTTOM_SHEET_SHOW_DETAILS)),
                            grey_interactable(), nullptr)]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Edit the card's nickname.
  [[EarlGrey selectElementWithMatcher:SettingToolbarEditButton()]
      performAction:grey_tap()];

  NSString* nickname = @"Card Nickname";
  [[EarlGrey selectElementWithMatcher:NicknameTextField()]
      performAction:grey_replaceText(nickname)];

  [[EarlGrey selectElementWithMatcher:SettingToolbarDoneButton()]
      performAction:grey_tap()];

  // Close the context menu.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Reopen the bottom sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  // Make sure the nickname is active.
  NSString* nicknameAndCardNumber =
      [nickname stringByAppendingString:[_lastDigits substringFromIndex:4]];
  id<GREYMatcher> nicknamedCreditCard = grey_text(nicknameAndCardNumber);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:nicknamedCreditCard];
}

// Verify that the Payments Bottom Sheet "Manage Payments Methods" button opens
// the proper menu and allows a credit card to be deleted.
- (void)testOpenPaymentsBottomSheetPaymentsMethodsDelete {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = ContinueButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];

  // Long press to open context menu.
  id<GREYMatcher> creditCardEntry = grey_text(_lastDigits);

  [[EarlGrey selectElementWithMatcher:creditCardEntry]
      performAction:grey_longPress()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithAccessibilityLabel(
                  l10n_util::GetNSString(
                      IDS_IOS_PAYMENT_BOTTOM_SHEET_MANAGE_PAYMENT_METHODS)),
              grey_interactable(), nullptr)] performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Delete the credit card
  [[EarlGrey selectElementWithMatcher:SettingToolbarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:creditCardEntry]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingToolbarDeleteButton()]
      performAction:grey_tap()];

  // Close the context menu.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Try to reopen the bottom sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  // With no suggestions left, the keyboard should open instead.
  WaitForKeyboardToAppear();

  // Make sure the bottom sheet isn't there.
  [[EarlGrey selectElementWithMatcher:continueButton]
      assertWithMatcher:grey_nil()];
}

@end
