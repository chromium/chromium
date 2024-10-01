// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/authentication_egtest_util.h"
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

namespace {

// Matcher for a navigation bar with the "Verification" title.
id<GREYMatcher> CardUnmaskPromptNavigationBarTitle() {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
      IDS_AUTOFILL_CARD_UNMASK_PROMPT_NAVIGATION_TITLE_VERIFICATION);
}

// Matcher for the text message challenge option label.
id<GREYMatcher> CardUnmaskTextMessageChallengeOptionLabel() {
  return chrome_test_util::StaticTextWithAccessibilityLabelId(
      IDS_AUTOFILL_AUTHENTICATION_MODE_TEXT_MESSAGE);
}

// Matcher for the "Send" button.
id<GREYMatcher> CardUnmaskAuthenticationSelectionSendButton() {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_SEND),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
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

// Matcher for the "Cancel" button.
id<GREYMatcher> CancelButton() {
  return grey_allOf(
      chrome_test_util::CancelButton(),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Matcher for the OTP input textfield.
id<GREYMatcher> OtpTextfield() {
  return grey_allOf(
      grey_accessibilityID(kOtpInputTextfieldAccessibilityIdentifier),
      grey_kindOfClass([UITextField class]), nil);
}

// Matcher for "Get new code" link in footer.
id<GREYMatcher> OtpNewCodeLink() {
  return grey_allOf(grey_ancestor(grey_accessibilityID(
                        kOtpInputFooterAccessibilityIdentifier)),
                    grey_kindOfClassName(@"UIAccessibilityLinkSubelement"),
                    grey_accessibilityTrait(UIAccessibilityTraitLink), nil);
}

}  // namespace

@interface OtpInputDialogEgtest : ChromeTestCase
@end

@implementation OtpInputDialogEgtest {
  NSString* _enrolledCardNameAndLastFour;
}

#pragma mark - Setup

- (void)setUp {
  [super setUp];
  [AutofillAppInterface setUpFakeCreditCardServer];
  _enrolledCardNameAndLastFour =
      [AutofillAppInterface saveMaskedCreditCardEnrolledInVirtualCard];
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

- (void)tearDown {
  [AutofillAppInterface clearAllServerDataForTesting];
  [AutofillAppInterface tearDownFakeCreditCardServer];
  [super tearDown];
}

- (void)showOtpInputDialog {
  // Tap on the card name field in the web content.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  // Wait for the payments bottom sheet to appear.
  id<GREYMatcher> paymentsBottomSheetVirtualCard = grey_accessibilityID(
      [NSString stringWithFormat:@"%@ %@", _enrolledCardNameAndLastFour,
                                 @"Virtual card"]);
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:paymentsBottomSheetVirtualCard];
  [[EarlGrey selectElementWithMatcher:paymentsBottomSheetVirtualCard]
      performAction:grey_tap()];

  // Wait enough time so the min delay is past before being allowed to fill
  // credit card information from the sheet.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_PAYMENT_BOTTOM_SHEET_CONTINUE)]
      performAction:grey_tap()];

  // Wait for the progress dialog to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::StaticTextWithAccessibilityLabelId(
                          IDS_AUTOFILL_CARD_UNMASK_PROGRESS_DIALOG_TITLE)];

  // Inject the card unmask response with card unmask options.
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
      selectElementWithMatcher:CardUnmaskAuthenticationSelectionSendButton()]
      performAction:grey_tap()];

  // Wait for the OTP input dialog to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::StaticTextWithAccessibilityLabelId(
                          IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_TITLE)];
}

// Test to ensure the dialog contains the correct content.
- (void)testOtpInputDialogContent {
  [self showOtpInputDialog];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:OtpTextfield()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:OtpInputDialogConfirmButton()]
      assertWithMatcher:grey_allOf(grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:CancelButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:OtpNewCodeLink()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Test to ensure the dialog goes away once the cancel button is clicked.
- (void)testOtpInputDialogCancel {
  [self showOtpInputDialog];

  // Tap the cancel button.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

  // Expect the card unmask OTP inpud dialog view to disappear.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      CardUnmaskPromptNavigationBarTitle()];
}

// Test to ensure the dialog's confirm button works correctly.
- (void)testOtpInputDialogConfirm {
  [self showOtpInputDialog];

  [[EarlGrey selectElementWithMatcher:OtpInputDialogConfirmButton()]
      assertWithMatcher:grey_allOf(grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   grey_sufficientlyVisible(), nil)];

  // Type in a valid OTP into the textfield will enable the confirm button.
  [[EarlGrey selectElementWithMatcher:OtpTextfield()]
      performAction:grey_replaceText(@"123456")];
  [[EarlGrey selectElementWithMatcher:OtpInputDialogConfirmButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled)),
                                   grey_sufficientlyVisible(), nil)];

  [AutofillAppInterface setAccessToken];
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

// TODO(crbug.com/324611581): Add test for the error message.

@end
