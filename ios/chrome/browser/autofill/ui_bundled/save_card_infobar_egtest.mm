// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/i18n/time_formatting.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
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

NSString* const kSavedCardLabel =
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

id<GREYMatcher> LocalBannerLabelsMatcher() {
  NSString* bannerLabel =
      [NSString stringWithFormat:@"%@,%@",
                                 l10n_util::GetNSString(
                                     IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL),
                                 kSavedCardLabel];
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      grey_accessibilityLabel(bannerLabel), nil);
}

id<GREYMatcher> UploadBannerLabelsMatcher() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  NSString* title =
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3);
#else
  NSString* title =
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD);
#endif
  NSString* bannerLabel =
      [NSString stringWithFormat:@"%@,%@", title, kSavedCardLabel];
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      grey_accessibilityLabel(bannerLabel), nil);
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
  if ([self isRunningTest:@selector(testStickySavePromptJourney)]) {
    config.features_enabled.push_back(kAutofillStickyInfobarIos);
  }
  if ([self isRunningTest:@selector
            (testOfferUpstream_FullData_PaymentsAccepts_Xframe)] ||
      [self
          isRunningTest:@selector(testUserData_LocalSave_UserAccepts_Xframe)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAcrossIframesIos);
  }
  // testUserData_LocalSave_UserAccepts_Xframe
  return config;
}

- (void)setUp {
  [super setUp];
  // Observe histograms in tests.
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [AutofillAppInterface setUpFakeCreditCardServer];
}

- (void)tearDown {
  // Clear existing credit cards.
  [AutofillAppInterface clearCreditCardStore];

  // Clear existing profiles.
  [AutofillAppInterface clearProfilesStore];

  // Clear CreditCardSave StrikeDatabase.
  [AutofillAppInterface tearDownFakeCreditCardServer];

  // Release the histogram tester.
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
  [super tearDown];

}

#pragma mark - Page interaction helper methods

- (void)fillAndSubmitFormWithCardDetailsOnly {
  [ChromeEarlGrey tapWebStateElementWithID:@"fill_card_only"];
  [self submitForm];
}

- (void)fillAndSubmitForm {
  [ChromeEarlGrey tapWebStateElementWithID:@"fill_form"];
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

- (BOOL)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_nil()
                                                             error:&error];
    return error == nil;
  };
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
}

#pragma mark - Tests
// Upon completion, each test should have the SaveInfobar removed. This is
// because the tearDown() function, which is triggered after each test,
// removes SaveInfoBar and InfobarEvent::kOnStrikeChangeCompleteCalled will be
// expected.

// Ensures that submitting the form should query Google Payments; and the
// fallback local save infobar becomes visible if the request unexpectedly
// fails but the form data is complete.
- (void)testOfferLocalSave_FullData_RequestFails {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsSuccess
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_INTERNAL_SERVER_ERROR];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled),
    @(CreditCardSaveManagerObserverEvent::kOnOfferLocalSaveCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");
}

// Ensures that submitting the form should query Google Payments; and the
// fallback local save infobar becomes visible if the request is declined but
// the form data is complete.
- (void)testOfferLocalSave_FullData_PaymentsDeclines {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
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

  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");
}

// Ensures that submitting the form, even with only card number and expiration
// date, should query Google Payments; but the fallback local save infobar
// should not appear if the request is declined and the form data is incomplete.
- (void)testNotOfferLocalSave_PartialData_PaymentsDeclines {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsFailure
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled),
  ]
                                          timeout:kWaitForDownloadTimeout];

  [self fillAndSubmitFormWithCardDetailsOnly];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Make sure the save card infobar does not become visible.
  GREYAssertFalse(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar should not show.");
}

// Ensures that submitting the form should query Google Payments; and the
// upstreaming infobar should appear if the request is accepted.
- (void)testOfferUpstream_FullData_PaymentsAccepts {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsSuccess
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:UploadBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");
}

// Test saving credit card upstream with a xframe credit card form.
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

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:UploadBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");
}

// Ensures that submitting the form, even with only card number and expiration
// date, should query Google Payments and the upstreaming infobar should appear
// if the request is accepted.
- (void)testOfferUpstream_PartialData_PaymentsAccepts {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsSuccess
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:UploadBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");
}

// Ensures that the infobar goes away and UMA metrics are correctly logged if
// the user declines upload.
- (void)testUMA_Upstream_UserDeclines {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsSuccess
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
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
  GREYAssert(
      [self waitForUIElementToDisappearWithMatcher:UploadBannerLabelsMatcher()],
      @"Save card infobar failed to disappear.");

  // Ensure that UMA was logged correctly.
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
}

// Ensures that the infobar goes away, an UploadCardRequest RPC is sent to
// Google Payments, and UMA metrics are correctly logged if the user accepts
// upload.
- (void)testUMA_Upstream_UserAccepts {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsSuccess
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
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
                 @"Event was not triggered");

  // Wait until the save card infobar disappears.
  GREYAssert(
      [self waitForUIElementToDisappearWithMatcher:UploadBannerLabelsMatcher()],
      @"Save card infobar failed to disappear.");

  // Ensure that UMA was logged correctly.
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
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

// Ensures that the infobar goes away and no credit card is saved to Chrome if
// the user declines local save.
- (void)testUserData_LocalSave_UserDeclines {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
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
  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  // Dismiss infobar banner.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Wait until the save card infobar disappears.
  GREYAssert(
      [self waitForUIElementToDisappearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to disappear.");

  // Ensure credit card is not saved locally.
  GREYAssertEqual(0U, [AutofillAppInterface localCreditCount],
                  @"No credit card should have been saved.");
}

// Ensures that the infobar goes away and the credit card is saved to Chrome if
// the user accepts local save.
- (void)testUserData_LocalSave_UserAccepts {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Ensure there are no saved credit cards.
  GREYAssertEqual(0U, [AutofillAppInterface localCreditCount],
                  @"There should be no saved credit card.");

  // Set up the Google Payments server response.
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

  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  // Tap the save button.
  [[EarlGrey selectElementWithMatcher:LocalSaveButtonMatcher()]
      performAction:grey_tap()];

  // Wait until the save card infobar disappears.
  GREYAssert(
      [self waitForUIElementToDisappearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to disappear.");

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

  // Wait until the save card infobar becomes visible.
  GREYAssert(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to show.");

  // Tap the save button.
  [[EarlGrey selectElementWithMatcher:LocalSaveButtonMatcher()]
      performAction:grey_tap()];

  // Wait until the save card infobar disappears.
  GREYAssert(
      [self waitForUIElementToDisappearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to disappear.");

  // Ensure credit card is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface localCreditCount],
                  @"Credit card should have been saved.");
}

// Ensures that submitting the form should query Google Payments; but the
// fallback local save infobar should not appear if the maximum StrikeDatabase
// strike limit is reached.
// TODO(crbug.com/41437589): remove SetFormFillMaxStrikes() and incur
// the maximum number of strikes by showing and declining save infobar instead.
- (void)testNotOfferLocalSave_MaxStrikesReached {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  [AutofillAppInterface setPaymentsResponse:kResponseGetUploadDetailsFailure
                                 forRequest:kURLGetUploadDetailsRequest
                              withErrorCode:net::HTTP_OK];

  [AutofillAppInterface setFormFillMaxStrikes:3
                                      forCard:@"CreditCardSave__5454"];

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];

  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

  // Make sure the save card infobar does not become visible.
  GREYAssertFalse(
      [self waitForUIElementToAppearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar should not show.");
}

// Tests the sticky credit card prompt journey where the prompt remains there
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

  [AutofillAppInterface resetEventWaiterForEvents:@[
    @(CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled),
    @(CreditCardSaveManagerObserverEvent::
          kOnReceivedGetUploadDetailsResponseCalled),
    @(CreditCardSaveManagerObserverEvent::kOnOfferLocalSaveCalled)
  ]
                                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitForm];
  GREYAssertTrue([AutofillAppInterface waitForEvents],
                 @"Event was not triggered");

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
      [self waitForUIElementToDisappearWithMatcher:LocalBannerLabelsMatcher()],
      @"Save card infobar failed to disappear.");
}

@end
