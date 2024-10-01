// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
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

const char kCreditCardUrl[] = "/credit_card.html";
const char kFormCardName[] = "CCName";
const char kFormCardNumber[] = "CCNo";
const char kFormCardExpirationMonth[] = "CCExpiresMonth";
const char kFormCardExpirationYear[] = "CCExpiresYear";

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

  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  [AutofillAppInterface clearCreditCardStore];
  _lastDigits = [AutofillAppInterface saveLocalCreditCard];

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");

  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearMockReauthenticationModule];

  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
  [super tearDown];
}

// Matcher for the bottom sheet's "Continue" button.
id<GREYMatcher> ContinueButton() {
  return chrome_test_util::StaticTextWithAccessibilityLabelId(
      IDS_IOS_PAYMENT_BOTTOM_SHEET_CONTINUE);
}

// Matcher for the bottom sheet's "Use Keyboard" button.
id<GREYMatcher> UseKeyboardButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_PAYMENT_BOTTOM_SHEET_USE_KEYBOARD);
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

id<GREYMatcher> SubtitleString(const GURL& url) {
  return grey_text(l10n_util::GetNSStringF(
      IDS_IOS_PAYMENT_BOTTOM_SHEET_SUBTITLE,
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          url)));
}

id<GREYMatcher> ExpirationDateLabel() {
  return grey_text(ExpirationDateNSString());
}

NSString* ExpirationDateNSString() {
  return base::SysUTF8ToNSString(autofill::test::NextMonth() + "/" +
                                 autofill::test::NextYear().substr(2));
}

// Waits on a responsive Continue button to fill the credit card and returns a
// matcher to that button.
id<GREYMatcher> WaitOnResponsiveContinueButton() {
  id<GREYMatcher> continueButton = ContinueButton();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:continueButton];
  // Wait enough time so the min delay is past before being allowed to fill
  // credit card information from the sheet.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  return continueButton;
}

// Verifies that the number of accepted suggestions recorded for the given
// `suggestion_index` is as expected.
void CheckAutofillSuggestionAcceptedIndexMetricsCount(
    NSInteger suggestion_index) {
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:suggestion_index
                         forHistogram:
                             @"Autofill.SuggestionAcceptedIndex.CreditCard"],
      @"Unexpected histogram count for accepted card suggestion index.");

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:suggestion_index
                         forHistogram:@"Autofill.UserAcceptedSuggestionAtIndex."
                                      @"CreditCard.BottomSheet"],
      @"Unexpected histogram count for bottom sheet accepted card suggestion "
      @"index.");
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadPaymentsPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kCreditCardUrl)];
  [ChromeEarlGrey waitForWebStateContainingText:"Autofill Test"];

  // Localhost is not considered secure, therefore form security needs to be
  // overridden for the tests to work. This will allow us to fill the textfields
  // on the web page.
  [AutofillAppInterface considerCreditCardFormSecureForTesting];
}

// Verify credit card infos are filled.
- (void)verifyCreditCardInfosHaveBeenFilled:(autofill::CreditCard)card {
  std::string locale = l10n_util::GetLocaleOverride();

  // Credit card name.
  NSString* name = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_NAME_FULL, locale));
  NSString* condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormCardName, name];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];

  // Credit card number.
  NSString* number = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_NUMBER, locale));
  condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormCardNumber, number];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];

  // Credit card expiration month.
  NSString* expMonth = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_EXP_MONTH, locale));
  condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormCardExpirationMonth, expMonth];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];

  // Credit card expiration year.
  NSString* expYear = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR, locale));
  condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormCardExpirationYear, expYear];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

#pragma mark - Tests

// Tests that the Payments Bottom Sheet appears when tapping on a credit card
// related field.
// TODO(crbug.com/370679361) re-enable it.
- (void)DISABLED_testOpenPaymentsBottomSheetUseCreditCard {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = WaitOnResponsiveContinueButton();

  // Verify that the credit card is visible to the user.
  [[EarlGrey selectElementWithMatcher:grey_text(_lastDigits)]
      assertWithMatcher:grey_notNil()];

  // Make sure the user is seeing 1 card on the bottom sheet.
  GREYAssertEqual(1, [AutofillAppInterface localCreditCount],
                  @"Wrong number of stored credit cards.");

  [[EarlGrey selectElementWithMatcher:continueButton] performAction:grey_tap()];

  // No histogram logged because there is only 1 credential shown to the user.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"Autofill.TouchToFill.CreditCard.SelectedIndex"],
      @"Unexpected histogram error for touch to fill credit card selected");

  // Verify that the acceptance of the card suggestion at index 0 was correctly
  // recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/0);

  // Verify that the page is filled properly.
  [self verifyCreditCardInfosHaveBeenFilled:autofill::test::GetCreditCard()];
}

// Tests that the expected metric is logged when accepting a suggestion from
// the bottom sheet that is not the first one in the list.
- (void)testAcceptedSuggestionIndexLogged {
  // Add a credit card to the Personal Data Manager.
  [AutofillAppInterface saveMaskedCreditCard];

  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = WaitOnResponsiveContinueButton();

  // Make sure the user is seeing 2 cards on the bottom sheet.
  GREYAssertEqual(2, [AutofillAppInterface localCreditCount],
                  @"Wrong number of stored credit cards.");

  // Select and use the second credit card.
  [[EarlGrey selectElementWithMatcher:grey_text(_lastDigits)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:continueButton] performAction:grey_tap()];

  // Verify that the acceptance of the card suggestion at index 1 was correctly
  // recorded.
  CheckAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/1);

  // Verify that the page is filled properly.
  [self verifyCreditCardInfosHaveBeenFilled:autofill::test::GetCreditCard()];
}

// Tests that the Payments Bottom Sheet updates its contents when a new credit
// card becomes available in the personal data manager.
- (void)testUpdateBottomSheetOnAddServerCreditCard {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = WaitOnResponsiveContinueButton();

  // Add a credit card to the Personal Data Manager.
  id<GREYMatcher> serverCreditCardEntry =
      grey_text([AutofillAppInterface saveMaskedCreditCard]);

  // Make sure the Bottom Sheet has been updated with the new credit card.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:serverCreditCardEntry];

  // Make sure the initial credit card is still there.
  id<GREYMatcher> localCreditCardEntry = grey_text(_lastDigits);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:localCreditCardEntry];

  // Make sure the user is seeing 2 cards on the bottom sheet.
  GREYAssertEqual(2, [AutofillAppInterface localCreditCount],
                  @"Wrong number of stored credit cards.");

  // Select and use the new credit card.
  [[EarlGrey selectElementWithMatcher:serverCreditCardEntry]
      performAction:grey_tap()];

  // Verify that the accessory view (checkmark) is visible.
  id<GREYMatcher> accessoryView = grey_allOf(
      grey_kindOfClassName(@"UIImageView"),
      grey_ancestor(grey_kindOfClassName(@"_UITableCellAccessoryButton")),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:accessoryView]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:continueButton] performAction:grey_tap()];

  // Verify the CVC requester is visible.
  [[EarlGrey selectElementWithMatcher:grey_text(@"Verification")]
      assertWithMatcher:grey_notNil()];

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:0
                         forHistogram:
                             @"Autofill.TouchToFill.CreditCard.SelectedIndex"],
      @"Unexpected histogram error for touch to fill credit card selected "
      @"index");

  // TODO(crbug.com/40577448): Figure out a way to enter CVC and get the
  // unlocked card result.
}

// Tests that accessing a long press menu does not disable the bottom sheet.
// TODO(crbug.com/40071541): Test fails on iPhone simulator only.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testOpenPaymentsBottomSheetAfterLongPress \
  DISABLED_testOpenPaymentsBottomSheetAfterLongPress
#else
#define MAYBE_testOpenPaymentsBottomSheetAfterLongPress \
  testOpenPaymentsBottomSheetAfterLongPress
#endif
- (void)MAYBE_testOpenPaymentsBottomSheetAfterLongPress {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPhone 14 Pro Max 16.4.");
  }
  [self loadPaymentsPage];

  // Open the Payments Bottom Sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> continueButton = WaitOnResponsiveContinueButton();

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

  id<GREYMatcher> continueButton = WaitOnResponsiveContinueButton();

  [[EarlGrey selectElementWithMatcher:continueButton] performAction:grey_tap()];
}

// Verify that the Payments Bottom Sheet "Use Keyboard" button opens the
// keyboard. Also checks that the bottom sheet's subtitle and the credit card's
// expiration appear as expected before dismissing the bottom sheet.
- (void)testOpenPaymentsBottomSheetTapUseKeyboardShowKeyboard {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> useKeyboardButton = UseKeyboardButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:useKeyboardButton];

  // Verify that the subtitle string appears.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SubtitleString(
                                              self.testServer->GetURL(
                                                  kCreditCardUrl))];

  // Verify that the credit card's expiration date appears.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:ExpirationDateLabel()];

  // Dismiss the bottom sheet.
  [[EarlGrey selectElementWithMatcher:useKeyboardButton]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForKeyboardToAppear];
}

// Verify that the Payments Bottom Sheet "Show Details" button opens the proper
// menu and allows the nickname to be edited.
- (void)testOpenPaymentsBottomSheetShowDetailsEditNickname {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  WaitOnResponsiveContinueButton();

  // Long press to open context menu.
  id<GREYMatcher> creditCardEntry = grey_text(_lastDigits);

  [[EarlGrey selectElementWithMatcher:creditCardEntry]
      performAction:grey_longPress()];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                         IDS_IOS_PAYMENT_BOTTOM_SHEET_SHOW_DETAILS),
                     grey_interactable(), nullptr)] performAction:grey_tap()];

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
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
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

  id<GREYMatcher> continueButton = WaitOnResponsiveContinueButton();

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
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Make sure the bottom sheet isn't there.
  [[EarlGrey selectElementWithMatcher:continueButton]
      assertWithMatcher:grey_nil()];
}

// Verify that tapping outside the Payments Bottom Sheet opens the keyboard.
- (void)testTapOutsideThePaymentsBottomSheetShowsKeyboard {
  [self loadPaymentsPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  // Dismiss the bottom sheet by tapping outside.
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForKeyboardToAppear];
}

// Tests that both the virtual card and the original card are shown
// in the Payments Bottom Sheet.
- (void)testPaymentsBottomSheetShowsVirtualCard {
  // Add a credit card enrolled in VCN to the Personal Data Manager.
  NSString* enrolledCardNameAndLastFour =
      [AutofillAppInterface saveMaskedCreditCardEnrolledInVirtualCard];

  [self loadPaymentsPage];

  // Trigger autofill.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  // Confirm virtual card is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID([NSString
                                   stringWithFormat:@"%@ %@",
                                                    enrolledCardNameAndLastFour,
                                                    @"Virtual card"])]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Confirm original card is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID([NSString
                                   stringWithFormat:@"%@ %@",
                                                    enrolledCardNameAndLastFour,
                                                    ExpirationDateNSString()])]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss the bottom sheet by tapping outside.
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_tap()];
}

@end
