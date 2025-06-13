// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/authentication_egtest_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_view_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Matcher for a navigation bar with the unmask prompt title text.
id<GREYMatcher> CardUnmaskPromptNavigationBarTitle() {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
      IDS_AUTOFILL_CARD_UNMASK_PROMPT_NAVIGATION_TITLE_VERIFICATION);
}

// Matcher for the text message challenge option label.
id<GREYMatcher> CardUnmaskTextMessageChallengeOptionLabel() {
  return chrome_test_util::StaticTextWithAccessibilityLabelId(
      IDS_AUTOFILL_AUTHENTICATION_MODE_TEXT_MESSAGE);
}

// Matcher for the "Confirm" button.
id<GREYMatcher> CardUnmaskAuthenticationSelectionConfirmButton() {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_SEND),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Matcher for the OTP input textfield.
id<GREYMatcher> OtpTextfield() {
  return grey_allOf(
      grey_accessibilityID(kOtpInputTextfieldAccessibilityIdentifier),
      grey_kindOfClass([UITextField class]), nil);
}

// Matcher for the "Confirm" button.
id<GREYMatcher> OtpInputDialogConfirmButton() {
  return grey_allOf(
      grey_accessibilityID(
          kOtpInputNavigationBarConfirmButtonAccessibilityIdentifier),
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_AUTOFILL_CARD_UNMASK_CONFIRM_BUTTON),
      nil);
}

// Matcher for the "Pending" button that shows an activity indicator.
id<GREYMatcher> OtpInputDialogPendingButton() {
  return grey_accessibilityID(
      kOtpInputNavigationBarPendingButtonAccessibilityIdentifier);
}

}  // namespace

@interface CardInfoRetrievalEgtest : ChromeTestCase
@end

@implementation CardInfoRetrievalEgtest {
  NSString* _enrolledCardNameAndLastFour;
  NSString* _enrolledCardExpirationDate;
}

#pragma mark - Setup

- (void)setUp {
  [super setUp];
  [AutofillAppInterface setUpFakeCreditCardServer];

  _enrolledCardNameAndLastFour =
      [AutofillAppInterface saveMaskedCreditCardEnrolledInCardInfoRetrieval];
  std::string expirationDate = autofill::test::NextMonth() + "/" +
                               autofill::test::NextYear().substr(2, 2);
  _enrolledCardExpirationDate =
      [NSString stringWithUTF8String:expirationDate.c_str()];

  [AutofillAppInterface setMandatoryReauthEnabled:NO];
  [self setUpServer];
  [self setUpTestPage];
}

- (void)setUpServer {
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Failed to start test server.");
}

- (void)setUpTestPage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kCreditCardUrl)];
  [ChromeEarlGrey waitForWebStateContainingText:kAutofillTestString];

  [AutofillAppInterface considerCreditCardFormSecureForTesting];
}

- (void)tearDownHelper {
  [AutofillAppInterface clearAllServerDataForTesting];
  [AutofillAppInterface tearDownFakeCreditCardServer];
  [super tearDownHelper];
}

- (void)simulateSelectingCard {
  // Tap on the card name field in the web content.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  // Wait for the payments bottom sheet to appear.
  id<GREYMatcher> paymentsBottomSheetServerCard = grey_accessibilityID(
      [NSString stringWithFormat:@"%@ %@", _enrolledCardNameAndLastFour,
                                 _enrolledCardExpirationDate]);
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:paymentsBottomSheetServerCard];
  [[EarlGrey selectElementWithMatcher:paymentsBottomSheetServerCard]
      performAction:grey_tap()];

  // Wait enough time so the min delay is past before being allowed to fill
  // credit card information from the sheet.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_PAYMENT_BOTTOM_SHEET_CONTINUE)]
      performAction:grey_tap()];

  // Wait for the verifying progress dialog to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::StaticTextWithAccessibilityLabelId(
              IDS_AUTOFILL_CARD_INFO_RETRIEVAL_ENROLLED_CARD_UNMASK_PROGRESS_DIALOG_TITLE)];
}

- (void)testCardInfoRetrievalAutofillSuccessServerCard {
  [self simulateSelectingCard];

  // Fake the successful server response that triggers Dismiss.
  [AutofillAppInterface
      setPaymentsResponse:kUnmaskCardSuccessResponseNoAuthNeeded
               forRequest:kUnmaskCardRequestUrl
            withErrorCode:net::HTTP_OK];

  // Wait for the verifying progress dialog to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          chrome_test_util::StaticTextWithAccessibilityLabelId(
              IDS_AUTOFILL_CARD_INFO_RETRIEVAL_ENROLLED_CARD_UNMASK_PROGRESS_DIALOG_TITLE)];
}

- (void)testCardInfoRetrievalAutofillOtpChallengeServerCard {
  [self simulateSelectingCard];

  // Inject the card unmask response with OTP challenge unmask options.
  [AutofillAppInterface setPaymentsResponse:kUnmaskCardResponseOtpSuccess
                                 forRequest:kUnmaskCardRequestUrl
                              withErrorCode:net::HTTP_OK];

  // Wait for the card unmask authentication selection to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:CardUnmaskPromptNavigationBarTitle()];

  // Reset the access token as the risk based unmask request (the one that shows
  // the authentication selection dialog) will reset it when finished.
  [AutofillAppInterface setAccessToken];

  // Inject the select challenge option response.
  [AutofillAppInterface
      setPaymentsResponse:kSelectChallengeOptionResponseSuccess
               forRequest:kSelectChallengeOptionRequestUrl
            withErrorCode:net::HTTP_OK];

  // Select the OTP verification option and click "Send" button.
  [[EarlGrey
      selectElementWithMatcher:CardUnmaskTextMessageChallengeOptionLabel()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:CardUnmaskAuthenticationSelectionConfirmButton()]
      performAction:grey_tap()];

  // Wait for the OTP input dialog to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::StaticTextWithAccessibilityLabelId(
                          IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_TITLE)];

  // Type in a valid OTP into the textfield will enable the confirm button.
  [[EarlGrey selectElementWithMatcher:OtpTextfield()]
      performAction:grey_replaceText(@"123456")];

  [[EarlGrey selectElementWithMatcher:OtpInputDialogConfirmButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled)),
                                   grey_sufficientlyVisible(), nil)];

  // Reset payments response so that outdated unmask card response is not used.
  [AutofillAppInterface clearPaymentsResponses];
  // Tap the confirm button, it should be updated to an activity indicator.
  [[EarlGrey selectElementWithMatcher:OtpInputDialogConfirmButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OtpInputDialogConfirmButton()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [[EarlGrey selectElementWithMatcher:OtpInputDialogPendingButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"UIActivityIndicatorView")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
