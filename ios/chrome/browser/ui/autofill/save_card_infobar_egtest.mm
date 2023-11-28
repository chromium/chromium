// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/test/ios/wait_util.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
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

}  // namepsace

@interface SaveCardInfobarEGTest : WebHttpServerChromeTestCase

@end

@implementation SaveCardInfobarEGTest

// TODO(crbug.com/1245213)
// Some tests are not compatible with explicit save prompts for addresses.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector(testUserData_LocalSave_UserAccepts)] ||
      [self
          isRunningTest:@selector(testOfferLocalSave_FullData_RequestFails)] ||
      [self isRunningTest:@selector(testUserData_LocalSave_UserDeclines)] ||
      [self isRunningTest:@selector
            (testOfferLocalSave_FullData_PaymentsDeclines)]) {
  }
  return config;
}

- (void)setUp {
  [super setUp];
  // Observe histograms in tests.
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [AutofillAppInterface setUpSaveCardInfobarEGTestHelper];
}

- (void)tearDown {
  // Clear existing credit cards.
  [AutofillAppInterface clearCreditCardStore];

  // Clear existing profiles.
  [AutofillAppInterface clearProfilesStore];

  // Clear CreditCardSave StrikeDatabase.
  [AutofillAppInterface tearDownSaveCardInfobarEGTestHelper];

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

// Ensures that submitting the form should query Google Payments; but the
// fallback local save infobar should not appear if the maximum StrikeDatabase
// strike limit is reached.
// TODO(crbug.com/925670): remove SetFormFillMaxStrikes() and incur
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

@end
