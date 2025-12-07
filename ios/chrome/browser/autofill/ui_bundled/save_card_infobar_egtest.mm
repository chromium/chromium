// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/i18n/time_formatting.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/browser/badges/model/features.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_save_card_modal_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// URLs of the test pages.
const char kCreditCardUploadForm[] =
    "https://components/test/data/autofill/"
    "credit_card_upload_form_address_and_cc.html";

// Google Payments server requests and responses.
NSString* const kURLGetUploadDetailsRequest =
    @"https://payments.google.com/payments/apis/chromepaymentsservice/"
     "getdetailsforsavecard";
NSString* const kResponseGetUploadDetailsSuccess =
    @"{\"legal_message\":{\"line\":[{\"template\":\"Legal message template with"
     " link: "
     "{0}.\",\"template_parameter\":[{\"display_text\":\"Link\",\"url\":"
     "\"https:"
     "//www.example.com/\"}]}]},\"context_token\":\"dummy_context_token\"}";
NSString* const kResponseGetUploadDetailsFailure =
    @"{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An"
     " unexpected error has occurred. Please try again later.\"}}";

// Url injected for saving card on the payments server.
NSString* const kSaveCardUrl =
    @"https://payments.google.com/payments/apis-secure/chromepaymentsservice/"
    @"savecard?s7e_suffix=chromewallet";
NSString* const kSaveCardResponse = @"{\"instrument_id\":\"1\"}";

NSString* const kFillFullFormId = @"fill_form";
NSString* const kFillPartialFormId = @"fill_card_only";

const std::u16string kNetwork = u"Mastercard";
const std::u16string kCardLastFourDigits = u"5454";
NSString* const kSaveCardLabel =
    @"Mastercard  ‪•⁠ ⁠•⁠ ⁠•⁠ ⁠•⁠ ⁠5454‬";

id<GREYMatcher> LocalSaveButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT);
}

id<GREYMatcher> UploadBannerSaveButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_AUTOFILL_SAVE_ELLIPSIS);
}

id<GREYMatcher> UploadModalSaveButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_AUTOFILL_SAVE_CARD);
}

id<GREYMatcher> BottomSheetAcceptButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT);
}

id<GREYMatcher> LocalBottomSheetCancelButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_AUTOFILL_NO_THANKS_MOBILE_LOCAL_SAVE);
}

id<GREYMatcher> UploadBottomSheetCancelButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_AUTOFILL_NO_THANKS_MOBILE_UPLOAD_SAVE);
}

// Matcher for the activity indicator.
id<GREYMatcher> ActivityIndicatorMatcher() {
  return grey_allOf(grey_kindOfClassName(@"UIActivityIndicatorView"),
                    grey_ancestor(chrome_test_util::ButtonStackPrimaryButton()),
                    nil);
}

id<GREYMatcher> LocalBannerLabelsMatcher() {
  NSString* bannerLabel = [NSString
      stringWithFormat:
          @"%@,%@",
          l10n_util::GetNSString(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL_ON_THIS_DEVICE),
          kSaveCardLabel];
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      grey_accessibilityLabel(bannerLabel), nil);
}

id<GREYMatcher> UploadBannerLabelsMatcher(bool is_bottomsheet_enabled = true) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  NSString* title = l10n_util::GetNSString(
      is_bottomsheet_enabled
          ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_SECURITY
          : IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3);
#else
  NSString* title =
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD);
#endif
  NSString* bannerLabel =
      [NSString stringWithFormat:@"%@,%@", title, kSaveCardLabel];
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      grey_accessibilityLabel(bannerLabel), nil);
}

id<GREYMatcher> LocalBottomSheetTitleMatcher() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL));
}

id<GREYMatcher> UploadBottomSheetTitleMatcher() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_SECURITY));
#else
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD));
#endif
}

id<GREYMatcher> BottomSheetCardDescriptionMatcher() {
  NSString* cardDescriptionLabel =
      base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_CARD_DESCRIPTION, kNetwork,
          kCardLastFourDigits,
          l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_CREDIT_CARD_EXPIRATION_DATE_ABBR_V2, u"12",
              base::UTF8ToUTF16(autofill::test::NextYear()))));

  return grey_allOf(grey_accessibilityID(kSaveCardLabel),
                    grey_accessibilityLabel(cardDescriptionLabel), nil);
}

id<GREYMatcher> CVCTextField() {
  return grey_accessibilityID(kSaveCardModalCVCTextFieldIdentifier);
}

// Simulates typing text on the keyboard and avoid having the first character
// typed uppercased. Will retype the first letter to make sure it is lowercased
// when `retypeFirstLetter` is set to true which is the default value.
//
// TODO(crbug.com/40916974): This should be replaced by grey_typeText when
// fixed.
void TypeText(NSString* nsText, bool retypeFirstLetter = true) {
  std::string text = base::SysNSStringToUTF8(nsText);
  for (size_t i = 0; i < text.size(); ++i) {
    // Type each character in the provided text.
    NSString* letter = base::SysUTF8ToNSString(text.substr(i, 1));
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:letter flags:0];
    if (i == 0 && retypeFirstLetter) {
      // Undo and retype the first letter to not have it uppercased.
      [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"z"
                                              flags:UIKeyModifierCommand];
      [ChromeEarlGrey simulatePhysicalKeyboardEvent:letter flags:0];
    }
  }
}

// Fills and submits the xframe credit card form that corresponds to
// xframe_credit_card.html. Fill the form by typing on each field from the
// keyboard since filling with a script doesn't allow capturing the form
// values for saving.
//
// TODO(crbug.com/360712075): Figure out why filling the xframe credit card
// form from a script doesn't allow capturing all the data for saving. The
// value of the cardholder name field on the main frame is captured but not
// the other fields that are in iframes.
void FillAndSubmitXframeCreditCardForm() {
  // Focus on the name credit card field.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:
                      @"document.getElementById('CCName').focus();"];
  // Type the name and respect cases.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"J" flags:UIKeyModifierShift];
  TypeText(@"ohn ", false);
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"S" flags:UIKeyModifierShift];
  TypeText(@"mith", false);
  // Wait some time to make sure typing is done.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2));

  // Fill the credit card fields that are hosting in iframes.
  // Set the year to fill as 4 years from now.
  NSString* year_to_fill =
      base::SysUTF8ToNSString(base::UnlocalizedTimeFormatWithPattern(
          base::Time::Now() + base::Days(366 * 4), "yyyy"));
  std::vector<std::tuple<NSString*, NSString*, NSString*>> typingInstructions =
      {std::make_tuple(@"cc-number-frame", @"CCNo", @"5454545454545454"),
       std::make_tuple(@"cc-exp-frame", @"CCExpiresMonth", @"12"),
       std::make_tuple(@"cc-exp-frame", @"CCExpiresYear", year_to_fill),
       std::make_tuple(@"cc-cvc", @"cvc", @"123")};
  for (auto [frame_id, field_id, value] : typingInstructions) {
    // Pop the keyboard by focusing the on the field to fill.
    NSString* script = [NSString
        stringWithFormat:@"document.getElementById('%@')."
                         @"contentDocument.getElementById('%@').focus();",
                         frame_id, field_id];
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
    // Type value with keyboard.
    TypeText(value);
    // Wait some time to make sure typing is done.
    base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2));
  }
  [ChromeEarlGrey tapWebStateElementWithID:@"cc-form-submit"];
}

}  // namespace

@interface SaveCardInfobarEGTest : WebHttpServerChromeTestCase

@end

@implementation SaveCardInfobarEGTest

// TODO(crbug.com/40196025)
// Some tests are not compatible with explicit save prompts for addresses.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector(testOfferLocalSave_WithInfobar)]) {
    // This test needs the badge.
    config.features_disabled.push_back(kAutofillBadgeRemoval);
  }

  if ([self isRunningTest:@selector(testStickySavePromptJourney)]) {
    config.features_enabled.push_back(kAutofillStickyInfobarIos);
  }
  if ([self
          isRunningTest:@selector
          (testOfferUpstream_FullData_PaymentsAccepts_Xframe_WithBottomSheetDisabled)] ||
      [self isRunningTest:@selector
            (testOfferUpstream_FullData_PaymentsAccepts_Xframe)] ||
      [self
          isRunningTest:@selector(testUserData_LocalSave_UserAccepts_Xframe)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAcrossIframesIos);
  }
  // testUserData_LocalSave_UserAccepts_Xframe

  if ([self
          isRunningTest:@selector
          (testOfferUpstream_FullData_PaymentsAccepts_WithBottomSheetDisabled)] ||
      [self
          isRunningTest:@selector
          (testOfferUpstream_FullData_PaymentsAccepts_Xframe_WithBottomSheetDisabled)]) {
    config.features_disabled.push_back(
        autofill::features::kAutofillSaveCardBottomSheet);
  } else {
    config.features_enabled.push_back(
        autofill::features::kAutofillSaveCardBottomSheet);
  }

  config.features_enabled.push_back(
      autofill::features::kAutofillLocalSaveCardBottomSheet);

  config.features_enabled.push_back(
      autofill::features::kAutofillEnableCvcStorageAndFilling);

  return config;
}

- (void)setUp {
  [super setUp];
  // Observe histograms in tests.
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  [AutofillAppInterface setUpFakeCreditCardServer];
}

- (void)tearDownHelper {
  // Clear existing credit cards.
  [AutofillAppInterface clearCreditCardStore];

  // Clear existing profiles.
  [AutofillAppInterface clearProfilesStore];

  // Clear CreditCardSave StrikeDatabase.
  [AutofillAppInterface tearDownFakeCreditCardServer];

  // Release the histogram tester.
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [super tearDownHelper];
}

#pragma mark - Page interaction helper methods

- (void)fillAndSubmitFormWithID:(NSString*)formID {
  [ChromeEarlGrey tapWebStateElementWithID:formID];
  [self submitForm];
}

- (void)submitForm {
  [ChromeEarlGrey tapWebStateElementWithID:@"submit"];
}

#pragma mark - Helper methods

- (BOOL)waitForUIElementToAppearWithMatcher:(id<GREYMatcher>)matcher {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
}

- (BOOL)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher
                           showingConfirmation:(BOOL)showingConfirmation {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_nil()
                                                             error:&error];
    return error == nil;
  };

  // Waiting slightly longer for confirmation to dismiss than the actual timeout
  // duration to avoid flakiness since it can take longer on the bots running
  // the simulator.
  return WaitUntilConditionOrTimeout(showingConfirmation
                                         ? kConfirmationDismissDelay * 1.5
                                         : kWaitForUIElementTimeout,
                                     condition);
}

// Sets up the Google Payments server response to offer upload or local save on
// submitting the credit card form.
- (void)fillAndSubmitFormWithID:(NSString*)formID
               paymentsResponse:(NSString*)fakeResponse
                      errorCode:(int)errorCode
                   forLocalSave:(BOOL)localSave {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:fakeResponse
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:errorCode];

  NSMutableArray* events = [NSMutableArray
      arrayWithObjects:@(CreditCardSaveManagerObserverEvent::
                             kOnDecideToRequestUploadSaveCalled),
                       @(CreditCardSaveManagerObserverEvent::
                             kOnReceivedGetUploadDetailsResponseCalled),
                       nil];
  if (localSave) {
    [events addObject:
                @(CreditCardSaveManagerObserverEvent::kOnOfferLocalSaveCalled)];
  }

  [AutofillAppInterface resetEventWaiterForEvents:events
                                          timeout:kWaitForDownloadTimeout];

  [self fillAndSubmitFormWithID:formID];

  GREYAssertTrue(
      [AutofillAppInterface waitForEvents],
      @"Request upload save or get upload details response not called");
}

- (void)dismissUploadSaveCardBottomSheetWithoutAccepting {
  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  // Push the cancel button.
  [[EarlGrey selectElementWithMatcher:UploadBottomSheetCancelButtonMatcher()]
      performAction:grey_tap()];

  // Synchronization off due to an infinite spinner.
  ScopedSynchronizationDisabler disabler;

  // Assert save card bottomsheet dimisses.
  GREYAssertTrue(
      [self
          waitForUIElementToDisappearWithMatcher:UploadBottomSheetTitleMatcher()
                             showingConfirmation:NO],
      @"Save card bottomsheet failed to dismiss.");

  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Strike not added on bottomsheet dismissed");
}

- (void)dismissLocalSaveCardBottomSheetWithoutAccepting {
  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  // Push the cancel button.
  [[EarlGrey selectElementWithMatcher:LocalBottomSheetCancelButtonMatcher()]
      performAction:grey_tap()];

  // Assert save card bottomsheet dimisses.
  GREYAssertTrue(
      [self
          waitForUIElementToDisappearWithMatcher:LocalBottomSheetTitleMatcher()
                             showingConfirmation:NO],
      @"Local save card bottomsheet failed to dismiss.");

  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Strike not added on local save bottomsheet dismissed");
}

- (void)dismissLocalSaveCardBannerWithoutAccepting {
  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  GREYAssert(
      [self waitForUIElementToDisappearWithMatcher:LocalBannerLabelsMatcher()
                               showingConfirmation:NO],
      @"Local save card infobar banner failed to disappear.");

  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Strike not added on local save card infobar dismissed");
}

- (void)removeInfoBar {
  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Strike not added on infobar dismissed");
}

- (void)triggerCreditCardSaveModal {
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];
  GREYAssertTrue(
      [self
          waitForUIElementToAppearWithMatcher:UploadBottomSheetTitleMatcher()],
      @"Save card bottom sheet failed to appear on the first attempt.");
  [self dismissUploadSaveCardBottomSheetWithoutAccepting];

  // Trigger the infobar banner.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];
  GREYAssertTrue(
      [self waitForUIElementToAppearWithMatcher:UploadBannerLabelsMatcher()],
      @"Save card infobar failed to show on the second attempt.");

  // Tap the banner's save button to open the modal.
  [[EarlGrey selectElementWithMatcher:UploadBannerSaveButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the modal is visible by asserting its save button is present.
  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      assertWithMatcher:grey_enabled()];
}

#pragma mark - Tests

// Ensures that submitting the form should query Google Payments; and the
// fallback local save infobar becomes visible if the request unexpectedly
// fails but the form data is complete.
- (void)testOfferLocalSave_FullData_RequestFails {
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_INTERNAL_SERVER_ERROR
                   forLocalSave:YES];

  // Wait until the save card bottomsheet becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBottomSheetTitleMatcher()],
      @"Local save card bottomsheet failed to show.");

  [self dismissLocalSaveCardBottomSheetWithoutAccepting];
}

// Ensures that submitting the form should query Google Payments; and the
// fallback local save infobar becomes visible if the request is declined but
// the form data is complete.
- (void)testOfferLocalSave_FullData_PaymentsDeclines {
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:YES];

  // Wait until the save card bottomsheet becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBottomSheetTitleMatcher()],
      @"Local save card bottomsheet failed to show.");

  [self dismissLocalSaveCardBottomSheetWithoutAccepting];
}

// Ensures that submitting the form, even with only card number and expiration
// date, should query Google Payments; but the fallback local save infobar
// should not appear if the request is declined and the form data is incomplete.
- (void)testNotOfferLocalSave_PartialData_PaymentsDeclines {
  [self fillAndSubmitFormWithID:kFillPartialFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Make sure the save card bottomsheet does not become visible.
  GREYAssertFalse(
      [self waitForUIElementToAppearWithMatcher:LocalBottomSheetTitleMatcher()],
      @"Local save card bottomsheet should not show.");
}

// Test upstream card upload is offered in infobar when submitting the credit
// card form with full data and Google Payments server is queried to request
// card upload.
- (void)testOfferUpstream_FullData_PaymentsAccepts_WithBottomSheetDisabled {
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:
                UploadBannerLabelsMatcher(/*is_bottomsheet_enabled=*/false)],
      @"Save card infobar failed to show.");

  [self removeInfoBar];
}

// Test upstream card upload is offered when submitting the credit card form
// with full data and Google Payments server is queried to request card upload.
- (void)testOfferUpstream_FullData_PaymentsAccepts {
  // Form submitted with full credit card data and no previous strikes offers
  // upstream save in a bottomsheet.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Wait for the save card bottomsheet to appear.
  GREYAssertTrue(
      [self
          waitForUIElementToAppearWithMatcher:UploadBottomSheetTitleMatcher()],
      @"Save card bottomsheet failed to appear.");
  [self dismissUploadSaveCardBottomSheetWithoutAccepting];
}

// Test upstream card upload is offered in infobar when submitting xframe credit
// card form and Google Payments server is queried to request card upload.
- (void)
    testOfferUpstream_FullData_PaymentsAccepts_Xframe_WithBottomSheetDisabled {
  // Serve ios http files.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Load xframe credit card page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/xframe_credit_card.html")];

  // Set up the Google Payments server response so upload is deemed successful.
  // Return success.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsSuccess
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  // Fill and submit form.
  FillAndSubmitXframeCreditCardForm();

  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:
                UploadBannerLabelsMatcher(/*is_bottomsheet_enabled=*/false)],
      @"Save card infobar failed to show.");

  [self removeInfoBar];
}

// Test upstream card upload is offered when submitting xframe credit card form
// and Google Payments server is queried to request card upload.
- (void)testOfferUpstream_FullData_PaymentsAccepts_Xframe {
  // Serve ios http files.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Load xframe credit card page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/xframe_credit_card.html")];

  // Set up the Google Payments server response so upload is deemed successful.
  // Return success.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsSuccess
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  // Fill and submit form.
  FillAndSubmitXframeCreditCardForm();

  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait for the save card bottomsheet to appear.
  GREYAssertTrue(
      [self
          waitForUIElementToAppearWithMatcher:UploadBottomSheetTitleMatcher()],
      @"Save card bottomsheet failed to appear.");

  [self dismissUploadSaveCardBottomSheetWithoutAccepting];
}

// Test upstream card upload is offered when submitting the credit card form
// with partial data (only card number and expiration date) and that Google
// Payments server is queried to request card upload. Due to partial data,
// instead of bottomsheet, infobar will be shown.
- (void)testOfferUpstream_PartialData_PaymentsAccepts {
  [self fillAndSubmitFormWithID:kFillPartialFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:UploadBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [self removeInfoBar];
}

// Ensures that UMA metrics are correctly logged when the user declines upload
// on a bottomsheet and an infobar.
- (void)testUMA_Upstream_UserDeclinesBottomSheetAndInfobar {
  // Form submitted with full credit card data and no previous strikes offers
  // upstream save in a bottomsheet.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Wait for the save card bottomsheet to appear.
  GREYAssertTrue(
      [self
          waitForUIElementToAppearWithMatcher:UploadBottomSheetTitleMatcher()],
      @"Save card bottomsheet failed to appear.");

  // Dismissing the bottomsheet incurs a strike on the card. For the second card
  // upload offer, an infobar banner will be shown. This is in accordance with
  // the strike logic used to conditionally show bottomsheet and fallback to
  // infobar UI until max strike limit is reached.
  [self dismissUploadSaveCardBottomSheetWithoutAccepting];

  // Ensure UMA logs that upload was offered.
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"Autofill.UploadOfferedCardOrigin"];
  if (error) {
    GREYFail([error description]);
  }

  // Wait a bit before re-submitting the form.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2));

  // Submit the credit card form again to be offered card upload in a save card
  // infobar.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Wait until the save card infobar becomes visible.
  GREYAssertTrue(
      [self waitForUIElementToAppearWithMatcher:UploadBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  // Dismiss Infobar banner
  [[EarlGrey selectElementWithMatcher:UploadBannerLabelsMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Wait until the save card infobar disappears.
  GREYAssertTrue(
      [self waitForUIElementToDisappearWithMatcher:UploadBannerLabelsMatcher()
                               showingConfirmation:NO],
      @"Save card infobar failed to disappear.");

  // Ensure UMA logs that upload was offered twice and card upload was not
  // accepted.
  error = [MetricsAppInterface
      expectTotalCount:2
          forHistogram:@"Autofill.UploadOfferedCardOrigin"];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"Autofill.UploadAcceptedCardOrigin"];
  if (error) {
    GREYFail([error description]);
  }
}

// Ensures that UMA metrics are correctly logged when the user declines upload
// on a bottomsheet and accepts when offered infobar. On accept, ensures that an
// UploadCardRequest RPC is sent to Google Payments Server.
- (void)testUMA_Upstream_UserDeclinesBottomSheetAcceptsInfobar {
  // Form submitted with full credit card data and no previous strikes offers
  // card upload in a bottomsheet.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Wait for the save card bottomsheet to appear.
  GREYAssertTrue(
      [self
          waitForUIElementToAppearWithMatcher:UploadBottomSheetTitleMatcher()],
      @"Save card bottomsheet failed to appear.");

  // Dismissing the bottomsheet incurs a strike on the card. For the second card
  // upload offer, an infobar banner will be shown. This is in accordance with
  // the strike logic used to conditionally show bottomsheet and fallback to
  // infobar UI until max strike limit is reached.
  [self dismissUploadSaveCardBottomSheetWithoutAccepting];

  // Ensure UMA logs that upload was offered once and card upload was not
  // accepted.
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"Autofill.UploadOfferedCardOrigin"];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"Autofill.UploadAcceptedCardOrigin"];
  if (error) {
    GREYFail([error description]);
  }

  // Wait a bit before re-submitting the form.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2));

  // Submit the credit card form again to be offered card upload in a save card
  // infobar.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Wait until the save card infobar becomes visible.
  GREYAssertTrue(
      [self waitForUIElementToAppearWithMatcher:UploadBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnSentUploadCardRequestCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  // Tap the banner save button.
  [[EarlGrey selectElementWithMatcher:UploadBannerSaveButtonMatcher()]
      performAction:grey_tap()];

  // Tap the modal save button.
  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      performAction:grey_tap()];

  if (![[AutofillAppInterface paymentsRiskData] length]) {
    // There is no provider for risk data so the request will not be sent.
    // Provide dummy risk data for this test.
    [AutofillAppInterface setPaymentsRiskData:@"Dummy risk data for tests"];
  }
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Upload card request was not called.");

  // Wait until the save card infobar disappears.
  GREYAssert(
      [self waitForUIElementToDisappearWithMatcher:UploadBannerLabelsMatcher()
                               showingConfirmation:NO],
      @"Save card infobar failed to disappear.");

  // Ensure UMA logs that upload was offered twice and card upload was accepted
  // once.
  error = [MetricsAppInterface
      expectTotalCount:2
          forHistogram:@"Autofill.UploadOfferedCardOrigin"];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"Autofill.UploadAcceptedCardOrigin"];
  if (error) {
    GREYFail([error description]);
  }
}

- (void)testSaveCardBottomSheetShowsLoadingAndConfirmationAfterAcceptPushed {
  // TODO(crbug.com/439735103): Re-enable the test on iOS26.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }

  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsSuccess
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Wait for the save card bottomsheet to appear.
  GREYAssertTrue(
      [self
          waitForUIElementToAppearWithMatcher:UploadBottomSheetTitleMatcher()],
      @"Save card bottomsheet failed to appear.");

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityValue(l10n_util::GetNSString(
              IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_SECURITY))]
      assertWithMatcher:grey_sufficientlyVisible()];
#endif

  [[EarlGrey selectElementWithMatcher:BottomSheetCardDescriptionMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:BottomSheetAcceptButtonMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];

  [[EarlGrey selectElementWithMatcher:UploadBottomSheetCancelButtonMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];

  // Push the accept button on the save card bottomsheet.
  [[EarlGrey selectElementWithMatcher:BottomSheetAcceptButtonMatcher()]
      performAction:grey_tap()];

  // Assert an activity indicator view is being shown in the loading state.
  GREYAssertTrue(
      [self waitForUIElementToAppearWithMatcher:ActivityIndicatorMatcher()],
      @"Save card bottomsheet failed to show activity indicator in loading "
      @"state.");
  [[EarlGrey selectElementWithMatcher:ActivityIndicatorMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert the accept button is disabled and has accessibility label for
  // loading state.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
      assertWithMatcher:
          grey_allOf(
              grey_not(grey_enabled()),
              grey_accessibilityLabel(l10n_util::GetNSString(
                  IDS_AUTOFILL_SAVE_CARD_PROMPT_LOADING_THROBBER_ACCESSIBLE_NAME)),
              nil)];

  // Assert the cancel button is disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Inject a response from the payments server when saving the card.
  [AutofillAppInterface setPaymentsResponse:kSaveCardResponse
                                 forRequest:kSaveCardUrl
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnSentUploadCardRequestCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  // Inject risk data required for the card upload request to be initiated.
  [AutofillAppInterface setPaymentsRiskData:@"Fake risk data for tests"];

  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Upload card request was not called.");

  // Assert the accept button is still disabled and has accessibility label for
  // confirmation state.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
      assertWithMatcher:
          grey_allOf(
              grey_not(grey_enabled()),
              grey_accessibilityLabel(l10n_util::GetNSString(
                  IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME)),
              nil)];

  // Assert a checkmark symbol is being shown in the confirmation state.
  [[[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackCheckmarkSymbol()]
      inRoot:chrome_test_util::ButtonStackPrimaryButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert the cancel button is disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Wait for bottomsheet to auto-dismiss.
  GREYAssertTrue(
      [self
          waitForUIElementToDisappearWithMatcher:UploadBottomSheetTitleMatcher()
                             showingConfirmation:YES],
      @"Save card bottomsheet failed to auto-dismiss in confirmation state.");
}

// Ensures that the bottomsheet goes away and no credit card is saved to Chrome
// if the user declines local save.
- (void)testUserData_LocalSave_UserDeclines {
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:YES];

  // Wait until the save card bottomsheet becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBottomSheetTitleMatcher()],
      @"Local save card bottomsheet failed to show.");

  [self dismissLocalSaveCardBottomSheetWithoutAccepting];

  // Ensure credit card is not saved locally.
  GREYAssertEqual(0U, [AutofillAppInterface localCreditCount],
                  @"No credit card should have been saved.");
}

// Ensures that the bottomsheet goes away and the credit card is saved to Chrome
// if the user accepts local save.
- (void)testUserData_LocalSave_UserAccepts {
  // Ensure there are no saved credit cards.
  GREYAssertEqual(0U, [AutofillAppInterface localCreditCount],
                  @"There should be no saved credit card.");

  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:YES];

  // Wait until the save card bottomsheet becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBottomSheetTitleMatcher()],
      @"Save card bottomsheet failed to show.");

  // Tap the save button.
  [[EarlGrey selectElementWithMatcher:BottomSheetAcceptButtonMatcher()]
      performAction:grey_tap()];

  // Wait for bottomsheet to auto-dismiss.
  GREYAssertTrue(
      [self
          waitForUIElementToDisappearWithMatcher:LocalBottomSheetTitleMatcher()
                             showingConfirmation:YES],
      @"Local save card bottomsheet failed to auto-dismiss in confirmation "
      @"state.");

  // Ensure credit card is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface localCreditCount],
                  @"Credit card should have been saved.");
}

// Test saving credit card locally as fallback with a xframe credit card form.
- (void)testUserData_LocalSave_UserAccepts_Xframe {
  // Serve ios http files.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Load xframe credit card page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/xframe_credit_card.html")];

  // Ensure there are no already saved credit cards at this point.
  GREYAssertEqual(0U, [AutofillAppInterface localCreditCount],
                  @"There should be no saved credit card.");

  // Set up the Google Payments server response so the local save fallback is
  // used. Return failure.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsFailure
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled),
    @(CreditCardSaveManagerObserverEvent::kOnOfferLocalSaveCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  // Fill and submit form. Fill address form to allow local fallback.
  [ChromeEarlGrey tapWebStateElementWithID:@"fill-address-btn"];
  FillAndSubmitXframeCreditCardForm();
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card bottomsheet becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBottomSheetTitleMatcher()],
      @"Save card bottomsheet failed to show.");

  // Tap the save button.
  [[EarlGrey selectElementWithMatcher:BottomSheetAcceptButtonMatcher()]
      performAction:grey_tap()];

  // Wait for bottomsheet to auto-dismiss.
  GREYAssertTrue(
      [self
          waitForUIElementToDisappearWithMatcher:LocalBottomSheetTitleMatcher()
                             showingConfirmation:YES],
      @"Local save card bottomsheet failed to auto-dismiss in confirmation "
      @"state.");

  // Ensure credit card is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface localCreditCount],
                  @"Credit card should have been saved.");
}

// Ensures that submitting the form should query Google Payments; but the
// fallback local save prompt should not appear if the maximum
// StrikeDatabase strike limit is reached.
- (void)testNotOfferLocalSave_MaxStrikesReached {
  // Incur the maximum number of strikes by showing and declining save
  // bottomsheet and infobar.

  // Show and dismiss bottomsheet to incur strike 1.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:YES];

  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBottomSheetTitleMatcher()],
      @"Local save card bottomsheet failed to show.");

  [self dismissLocalSaveCardBottomSheetWithoutAccepting];

  // Show and dismiss infobar to incur strike 2.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:YES];

  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Local save card infobar banner failed to show.");

  [self dismissLocalSaveCardBannerWithoutAccepting];

  // Show and dismiss bottomsheet to incur strike 3 which is the max strike
  // limit.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:YES];

  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Local save card infobar banner failed to show.");

  [self dismissLocalSaveCardBannerWithoutAccepting];

  // Submit the form for the fourth time.
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:NO];

  // Make sure the save card infobar does not become visible.
  GREYAssertFalse(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Local save card infobar banner should not show.");
}

// Test the sticky credit card prompt journey where the prompt remains there
// when navigating without an explicit user gesture, and then the prompt is
// dismissed when navigating with a user gesture. Test with the credit card save
// prompt but the type of credit card prompt doesn't matter in this test case.
- (void)testStickySavePromptJourney {
  const GURL testPageURL =
      web::test::HttpServer::MakeUrl(kCreditCardUploadForm);

  [ChromeEarlGrey loadURL:testPageURL];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsFailure
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  // Set strike count as 1 to directly show the infobar.
  [AutofillAppInterface setFormFillMaxStrikes:1
                                      forCard:@"CreditCardSave__5454"];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled),
    @(CreditCardSaveManagerObserverEvent::kOnOfferLocalSaveCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitFormWithID:kFillFullFormId];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Request upload save or get upload details response or offer "
                 @"local save not called");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  {
    // Reloading page from script shouldn't dismiss the infobar.
    NSString* script = @"location.reload();";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Assigning url from script to the page aka open an url shouldn't dismiss
    // the infobar.
    NSString* script = @"window.location.assign(window.location.href);";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Pushing new history entry without reloading content shouldn't dismiss the
    // infobar.
    NSString* script = @"history.pushState({}, '', 'destination2.html');";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Replacing history entry without reloading content shouldn't dismiss the
    // infobar.
    NSString* script = @"history.replaceState({}, '', 'destination3.html');";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }

  // Wait some time for things to settle.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));

  // Verify that the prompt is still there after the non-user initiated
  // navigations.
  [[EarlGrey selectElementWithMatcher:LocalBannerLabelsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Navigate with an emulated user gesture.
  [ChromeEarlGrey loadURL:testPageURL];

  // Wait until the save card infobar disappears.
  GREYAssertTrue(
      [self waitForUIElementToDisappearWithMatcher:LocalBannerLabelsMatcher()
                               showingConfirmation:NO],
      @"Save card infobar failed to disappear.");
}

// Test local save bottomsheet is shown and directly shows confirmation state on
// being accepted.
- (void)testLocalSaveBottomSheet {
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:YES];

  // Wait until the save card bottomsheet becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBottomSheetTitleMatcher()],
      @"Local save card bottomsheet failed to show.");

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_AUTOFILL_CHROME_LOGO_ACCESSIBLE_NAME))]
      assertWithMatcher:grey_sufficientlyVisible()];
#endif

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityValue(l10n_util::GetNSString(
                     IDS_AUTOFILL_SAVE_CARD_WITH_CVC_PROMPT_EXPLANATION_LOCAL))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:BottomSheetCardDescriptionMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:BottomSheetAcceptButtonMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];

  [[EarlGrey selectElementWithMatcher:LocalBottomSheetCancelButtonMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];

  {
    // Disable the synchronization, otherwise the test runner waits for the
    // animation for the accept button and the following assertions will fail on
    // iOS 26.
    ScopedSynchronizationDisabler disabler;

    // Push the accept button on the save card bottomsheet.
    [[EarlGrey selectElementWithMatcher:BottomSheetAcceptButtonMatcher()]
        performAction:grey_tap()];

    // Assert the accept button is disabled and has accessibility label for
    // confirmation state.
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
        assertWithMatcher:
            grey_allOf(
                grey_not(grey_enabled()),
                grey_accessibilityLabel(l10n_util::GetNSString(
                    IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME)),
                nil)];

    // Assert a checkmark symbol is being shown in the confirmation state.
    [[[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonStackCheckmarkSymbol()]
        inRoot:chrome_test_util::ButtonStackPrimaryButton()]
        assertWithMatcher:grey_sufficientlyVisible()];

    // Assert the cancel button is disabled.
    [[EarlGrey selectElementWithMatcher:LocalBottomSheetCancelButtonMatcher()]
        assertWithMatcher:grey_not(grey_enabled())];
  }

  // Wait for bottomsheet to auto-dismiss.
  GREYAssertTrue(
      [self
          waitForUIElementToDisappearWithMatcher:LocalBottomSheetTitleMatcher()
                             showingConfirmation:YES],
      @"Local save card bottomsheet failed to auto-dismiss in confirmation "
      @"state.");
}

// Test local save bottomsheet doesn't show loading state after being accepted.
- (void)testLocalSaveBottomSheetDoesNotShowLoading {
  [self fillAndSubmitFormWithID:kFillFullFormId
               paymentsResponse:kResponseGetUploadDetailsFailure
                      errorCode:net::HTTP_OK
                   forLocalSave:YES];

  // Wait until the save card bottomsheet becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBottomSheetTitleMatcher()],
      @"Local save card bottomsheet failed to show.");

  // Push the accept button on the save card bottomsheet.
  [[EarlGrey selectElementWithMatcher:BottomSheetAcceptButtonMatcher()]
      performAction:grey_tap()];

  GREYAssertFalse(
      [self waitForUIElementToAppearWithMatcher:ActivityIndicatorMatcher()],
      @"Local save card bottomsheet should not show activity indicator.");

  // Wait for bottomsheet to auto-dismiss.
  GREYAssertTrue(
      [self
          waitForUIElementToDisappearWithMatcher:LocalBottomSheetTitleMatcher()
                             showingConfirmation:YES],
      @"Local save card bottomsheet failed to auto-dismiss in confirmation "
      @"state.");
}

// Test infobar is offered for card with non zero strike and card is saved to
// Chrome if user accepts the infobar.
- (void)testOfferLocalSave_WithInfobar {
  // Ensure there are no saved credit cards.
  GREYAssertEqual(0U, [AutofillAppInterface localCreditCount],
                  @"There should be no saved credit card.");

  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsFailure
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  // Set strike count as 1 to directly show the infobar.
  [AutofillAppInterface setFormFillMaxStrikes:1
                                      forCard:@"CreditCardSave__5454"];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled),
    @(CreditCardSaveManagerObserverEvent::kOnOfferLocalSaveCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  [self fillAndSubmitFormWithID:kFillFullFormId];

  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Request upload save or get upload details response or offer "
                 @"local save not called");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Local save card infobar failed to show.");

  // Verify that the badge is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBadgeButtonSaveCardAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Tap the save button.
  [[EarlGrey selectElementWithMatcher:LocalSaveButtonMatcher()]
      performAction:grey_tap()];

  // Wait until the save card infobar disappears.
  GREYAssert(
      [self waitForUIElementToDisappearWithMatcher:LocalBannerLabelsMatcher()
                               showingConfirmation:NO],
      @"Local save card infobar failed to disappear.");

  // Ensure credit card is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface localCreditCount],
                  @"Credit card should have been saved.");
}

// TODO(crbug.com/440644620): Find a solution to page loading flakes.
// Tests that the save card flow via the infobar still works correctly when the
// badge is removed.
- (void)FLAKY_testInfobarWithoutBadge {
  // Ensure there are no saved credit cards.
  GREYAssertEqual(0U, [AutofillAppInterface localCreditCount],
                  @"There should be no saved credit card.");

  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsFailure
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  // Set strike count as 1 to directly show the infobar.
  [AutofillAppInterface setFormFillMaxStrikes:1
                                      forCard:@"CreditCardSave__5454"];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled),
    @(CreditCardSaveManagerObserverEvent::kOnOfferLocalSaveCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  [self fillAndSubmitFormWithID:kFillFullFormId];

  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Request upload save or get upload details response or offer "
                 @"local save not called");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Local save card infobar failed to show.");

  // Verify that the badge isn't displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBadgeButtonSaveCardAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  // Tap the save button.
  [[EarlGrey selectElementWithMatcher:LocalSaveButtonMatcher()]
      performAction:grey_tap()];

  // Wait until the save card infobar disappears.
  GREYAssert(
      [self waitForUIElementToDisappearWithMatcher:LocalBannerLabelsMatcher()
                               showingConfirmation:NO],
      @"Local save card infobar failed to disappear.");

  // Ensure credit card is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface localCreditCount],
                  @"Credit card should have been saved.");
}

// Tests CVC validation with short invalid input in the credit card upload save
// modal.
- (void)testUpstreamCVCValidation_WithShortInvalidInput {
  // Action: Trigger the upstream save modal to be displayed.
  [self triggerCreditCardSaveModal];

  // The modal is now visible. The save button should initially be enabled
  // because the pre-filled CVC is valid.
  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      assertWithMatcher:grey_enabled()];

  [[EarlGrey selectElementWithMatcher:CVCTextField()]
      performAction:grey_replaceText(@"12")];

  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests CVC validation with long invalid input in the credit card upload save
// modal.
- (void)testUpstreamCVCValidation_WithLongInvalidInput {
  // Trigger the upstream save modal to be displayed.
  [self triggerCreditCardSaveModal];

  // The modal is now visible. The save button should initially be enabled
  // because the pre-filled CVC is valid.
  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      assertWithMatcher:grey_enabled()];

  [[EarlGrey selectElementWithMatcher:CVCTextField()]
      performAction:grey_replaceText(@"12345")];

  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests CVC validation with non-digit invalid input in the credit card upload
// save modal.
- (void)testUpstreamCVCValidation_WithNonDigitInput {
  // Trigger the upstream save modal to be displayed.
  [self triggerCreditCardSaveModal];

  // The modal is now visible. The save button should initially be enabled
  // because the pre-filled CVC is valid.
  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      assertWithMatcher:grey_enabled()];

  [[EarlGrey selectElementWithMatcher:CVCTextField()]
      performAction:grey_replaceText(@"a")];

  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests CVC validation with 4 digits valid input in the credit card upload save
// modal.
- (void)testUpstreamCVCValidation_WithValidInput {
  // Trigger the upstream save modal to be displayed.
  [self triggerCreditCardSaveModal];

  // The modal is now visible. The save button should initially be enabled
  // because the pre-filled CVC is valid.
  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      assertWithMatcher:grey_enabled()];

  [[EarlGrey selectElementWithMatcher:CVCTextField()]
      performAction:grey_replaceText(@"1234")];

  [[EarlGrey selectElementWithMatcher:UploadModalSaveButtonMatcher()]
      assertWithMatcher:grey_enabled()];
}

@end
