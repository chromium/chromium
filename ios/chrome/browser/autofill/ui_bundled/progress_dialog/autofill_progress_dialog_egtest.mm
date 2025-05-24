// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/http/http_status_code.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const char kTestPageUrl[] = "/credit_card.html";
NSString* const kTriggeringRequestUrl =
    @"https://payments.google.com/payments/apis-secure/creditcardservice/"
    @"getrealpan?s7e_suffix=chromewallet";
NSString* const kSuccessResponseNoAuthNeeded =
    @"{ \"pan\": \"5411111111112109\" }";
const char kAutofillTestString[] = "Autofill Test";
const char kFormCardName[] = "CCName";

}  // namespace

@interface AutofillProgressDialogDismissEGTest : ChromeTestCase {
  NSString* _enrolledCardNameAndLastFour;
}
@end

@implementation AutofillProgressDialogDismissEGTest

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.features_enabled.push_back(
      autofill::features::kAutofillEnableFpanRiskBasedAuthentication);

  return config;
}

- (void)setUp {
  [super setUp];
  [AutofillAppInterface setUpFakeCreditCardServer];
  _enrolledCardNameAndLastFour = [AutofillAppInterface saveMaskedCreditCard];
  [AutofillAppInterface setMandatoryReauthEnabled:NO];
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Failed to start test server.");
  GURL testURL = self.testServer->GetURL(kTestPageUrl);
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:kAutofillTestString];
  [AutofillAppInterface considerCreditCardFormSecureForTesting];
  [self addTeardownBlock:^{
    [AutofillAppInterface clearAllServerDataForTesting];
    [AutofillAppInterface tearDownFakeCreditCardServer];
  }];
}

- (void)simulateUserFlowToShowDialogLoadingState {
  // Tap on the card name field in the web content.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];
  // Wait for the payments bottom sheet to appear.
  NSString* kPaymentsBottomSheetString =
      [NSString stringWithFormat:@"%@ %@", _enrolledCardNameAndLastFour,
                                 base::SysUTF8ToNSString(
                                     autofill::test::NextMonth() + "/" +
                                     autofill::test::NextYear().substr(2))];
  id<GREYMatcher> paymentsBottomSheetCardMatcher =
      grey_accessibilityID(kPaymentsBottomSheetString);
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:paymentsBottomSheetCardMatcher];
  [[EarlGrey selectElementWithMatcher:paymentsBottomSheetCardMatcher]
      performAction:grey_tap()];
  // The component may have an internal delay after card selection before
  // the "Continue" action is fully processed and the button state reflects
  // this. This wait ensures the component is ready, preventing premature taps.
  const base::TimeDelta total_delay_for_processing =
      autofill_ui_constants::kSelectSuggestionDelay + base::Milliseconds(500);

  base::test::ios::SpinRunLoopWithMinDelay(total_delay_for_processing);

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_PAYMENT_BOTTOM_SHEET_CONTINUE)]
      performAction:grey_tap()];

  // Wait for the progress dialog to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::StaticTextWithAccessibilityLabelId(
                          IDS_AUTOFILL_CARD_UNMASK_PROGRESS_DIALOG_TITLE)];
}

- (void)testDismissWithConfirmation_DisappearsAfterDelay {
  // Simulate flow to show dialog.
  [self simulateUserFlowToShowDialogLoadingState];

  // Verify dialog is initially visible.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_AUTOFILL_CARD_UNMASK_PROGRESS_DIALOG_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Fake the successful server response that triggers Dismiss.
  [AutofillAppInterface setPaymentsResponse:kSuccessResponseNoAuthNeeded
                                 forRequest:kTriggeringRequestUrl
                              withErrorCode:net::HTTP_OK];

  // This delay is the autodismiss delay (1 second) + extra time to avoid
  // flakiness on the simulators (2 seconds).
  const base::TimeDelta total_delay_for_dismiss =
      autofill_ui_constants::kProgressDialogConfirmationDismissDelay +
      base::Seconds(2);

  // Wait for the dialog to disappear after the delay.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          chrome_test_util::StaticTextWithAccessibilityLabelId(
              IDS_AUTOFILL_CARD_UNMASK_PROGRESS_DIALOG_TITLE)
                                     timeout:total_delay_for_dismiss];
}

@end
