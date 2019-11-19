// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/ios/ios_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/payments/core/journey_logger.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/test/app/histogram_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using payments::JourneyLogger;
}  // namespace

// Journey logger tests for Payment Request.
@interface PaymentRequestJourneyLoggerEGTest : PaymentRequestEGTestBase
@end

@implementation PaymentRequestJourneyLoggerEGTest {
  autofill::AutofillProfile _profile1;
  autofill::AutofillProfile _profile2;
  autofill::CreditCard _creditCard1;
  autofill::CreditCard _creditCard2;
}

#pragma mark - Helper methods

- (void)addProfiles {
  _profile1 = autofill::test::GetFullProfile();
  [self addAutofillProfile:_profile1];

  _profile2 = autofill::test::GetFullProfile2();
  [self addAutofillProfile:_profile2];
}

- (void)addCard1 {
  _creditCard1 = autofill::test::GetCreditCard();
  _creditCard1.set_billing_address_id(_profile1.guid());
  [self addCreditCard:_creditCard1];
}

- (void)addCard2 {
  _creditCard2 = autofill::test::GetCreditCard2();
  _creditCard2.set_billing_address_id(_profile2.guid());
  [self addCreditCard:_creditCard2];
}

#pragma mark - Tests

// Tests that the selected instrument metric is correctly logged when the
// Payment Request is completed with a credit card.
// TODO(crbug.com/795663): Fails on iphone11 devices.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testSelectedPaymentMethod DISABLED_testSelectedPaymentMethod
#else
#define MAYBE_testSelectedPaymentMethod testSelectedPaymentMethod
#endif
- (void)MAYBE_testSelectedPaymentMethod {
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/1007432): Enable this test.
    EARL_GREY_TEST_DISABLED(@"The test is flaky on iOS 13");
  }

  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:"payment_request_no_shipping_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
  [self payWithCreditCardUsingCVC:@"123"];

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  // Make sure transaction amount is logged correctly considering the completion
  // status.
  histogramTester.ExpectTotalCount("PaymentRequest.TransactionAmount.Triggered",
                                   1, failureBlock);
  histogramTester.ExpectTotalCount("PaymentRequest.TransactionAmount.Completed",
                                   1, failureBlock);
}

- (void)testOnlyBobpaySupported {
  if (!base::ios::IsRunningOnOrLater(10, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled on iOS versions below 10.3 because DOMException is not "
        @"available.");
  }

  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:"payment_request_bobpay_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
  [self waitForWebViewContainingTexts:{"rejected"}];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  histogramTester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel.NoShow",
      JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD, 1,
      failureBlock);

  // Make sure transaction amount metrics are not logged since we could not
  // trigger the payment sheet.
  histogramTester.ExpectTotalCount("PaymentRequest.TransactionAmount.Triggered",
                                   0, failureBlock);
  histogramTester.ExpectTotalCount("PaymentRequest.TransactionAmount.Completed",
                                   0, failureBlock);

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

// TODO(crbug.com/795663): Fails on iphone11 devices.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testShowSameRequest DISABLED_testShowSameRequest
#else
#define MAYBE_testShowSameRequest testShowSameRequest
#endif
- (void)MAYBE_testShowSameRequest {
  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:"payment_request_multiple_show_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
  [ChromeEarlGrey tapWebStateElementWithID:@"showAgain"];
  [self payWithCreditCardUsingCVC:@"123"];

  // Trying to show the same request twice is not considered a concurrent
  // request.
  GREYAssertTrue(
      histogramTester.GetAllSamples("PaymentRequest.CheckoutFunnel.NoShow")
          .empty(),
      @"");

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

// TODO(crbug.com/602666): add a test to verify that the correct metrics get
// recorded if the page tries to show() a second PaymentRequest, similar to
// PaymentRequestJourneyLoggerMultipleShowTest.StartNewRequest from
// payment_request_journey_logger_browsertest.cc.

// Tests that the correct number of suggestions shown for each section is logged
// when a Payment Request is completed.
// TODO(crbug.com/795663): Fails on iphone11 devices.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testAllSectionStats_NumberOfSuggestionsShown_Completed \
  DISABLED_testAllSectionStats_NumberOfSuggestionsShown_Completed
#else
#define MAYBE_testAllSectionStats_NumberOfSuggestionsShown_Completed \
  testAllSectionStats_NumberOfSuggestionsShown_Completed
#endif
- (void)MAYBE_testAllSectionStats_NumberOfSuggestionsShown_Completed {
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/1007432): Enable this test.
    EARL_GREY_TEST_DISABLED(@"The test is flaky on iOS 13");
  }

  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:
            "payment_request_contact_details_and_free_shipping_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
  [self payWithCreditCardUsingCVC:@"123"];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Expect the appropriate number of suggestions shown to be logged.
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.Completed", 1, 1,
      failureBlock);
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2, 1,
      failureBlock);
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 2, 1,
      failureBlock);

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                 @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

// Tests that the correct number of suggestions shown for each section is logged
// when a Payment Request is aborted by the user.
- (void)testAllSectionStats_NumberOfSuggestionsShown_UserAborted {
  if (!base::ios::IsRunningOnOrLater(10, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled on iOS versions below 10.3 because DOMException is not "
        @"available.");
  }

  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:
            "payment_request_contact_details_and_free_shipping_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_CANCEL)] performAction:grey_tap()];
  [self waitForWebViewContainingTexts:{"User closed the Payment Request UI."}];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Expect the appropriate number of suggestions shown to be logged.
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.UserAborted", 1, 1,
      failureBlock);
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 2,
      1, failureBlock);
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.UserAborted", 2, 1,
      failureBlock);

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                 @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

// Tests that the correct number of suggestions shown for each section is logged
// when a Payment Request is completed.
// TODO(crbug.com/795663): Fails on iphone11 devices.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testNoShippingSectionStats_NumberOfSuggestionsShown_Completed \
  DISABLED_testNoShippingSectionStats_NumberOfSuggestionsShown_Completed
#else
#define MAYBE_testNoShippingSectionStats_NumberOfSuggestionsShown_Completed \
  testNoShippingSectionStats_NumberOfSuggestionsShown_Completed
#endif
- (void)MAYBE_testNoShippingSectionStats_NumberOfSuggestionsShown_Completed {
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/1007432): Enable this test.
    EARL_GREY_TEST_DISABLED(@"The test is flaky on iOS 13");
  }

  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:"payment_request_contact_details_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
  [self payWithCreditCardUsingCVC:@"123"];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Expect the appropriate number of suggestions shown to be logged.
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.Completed", 1, 1,
      failureBlock);
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 2, 1,
      failureBlock);

  // There should be no log for shipping address since it was not requested.
  histogramTester.ExpectTotalCount(
      "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 0,
      failureBlock);

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                 @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

// Tests that the correct number of suggestions shown for each section is logged
// when a Payment Request is aborted by the user.
- (void)testNoShippingSectionStats_NumberOfSuggestionsShown_UserAborted {
  if (!base::ios::IsRunningOnOrLater(10, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled on iOS versions below 10.3 because DOMException is not "
        @"available.");
  }

  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:"payment_request_contact_details_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_CANCEL)] performAction:grey_tap()];
  [self waitForWebViewContainingTexts:{"User closed the Payment Request UI."}];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Expect the appropriate number of suggestions shown to be logged.
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.UserAborted", 1, 1,
      failureBlock);
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.UserAborted", 2, 1,
      failureBlock);

  // There should be no log for shipping address since it was not requested.
  histogramTester.ExpectTotalCount(
      "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 0,
      failureBlock);

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                 @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

// Tests that the correct number of suggestions shown for each section is logged
// when a Payment Request is completed.
// TODO(crbug.com/795663): Fails on iphone11 devices.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testNoContactDetailSectionStats_NumberOfSuggestionsShown_Completed \
  DISABLED_testNoContactDetailSectionStats_NumberOfSuggestionsShown_Completed
#else
#define MAYBE_testNoContactDetailSectionStats_NumberOfSuggestionsShown_Completed \
  testNoContactDetailSectionStats_NumberOfSuggestionsShown_Completed
#endif
- (void)
    MAYBE_testNoContactDetailSectionStats_NumberOfSuggestionsShown_Completed {
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/1007432): Enable this test.
    EARL_GREY_TEST_DISABLED(@"The test is flaky on iOS 13");
  }

  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:"payment_request_free_shipping_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
  [self payWithCreditCardUsingCVC:@"123"];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Expect the appropriate number of suggestions shown to be logged.
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.Completed", 1, 1,
      failureBlock);
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2, 1,
      failureBlock);

  // There should be no log for contact info since it was not requested.
  histogramTester.ExpectTotalCount(
      "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 0,
      failureBlock);

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

// Tests that the correct number of suggestions shown for each section is logged
// when a Payment Request is aborted by the user.
- (void)testNoContactDetailSectionStats_NumberOfSuggestionsShown_UserAborted {
  if (!base::ios::IsRunningOnOrLater(10, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled on iOS versions below 10.3 because DOMException is not "
        @"available.");
  }

  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:"payment_request_free_shipping_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_CANCEL)] performAction:grey_tap()];
  [self waitForWebViewContainingTexts:{"User closed the Payment Request UI."}];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Expect the appropriate number of suggestions shown to be logged.
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.UserAborted", 1, 1,
      failureBlock);
  histogramTester.ExpectUniqueSample(
      "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 2,
      1, failureBlock);

  // There should be no log for contact info since it was not requested.
  histogramTester.ExpectTotalCount(
      "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.UserAborted", 0,
      failureBlock);

  // Make sure transaction amount is logged correctly considering the completion
  // status.
  histogramTester.ExpectTotalCount("PaymentRequest.TransactionAmount.Triggered",
                                   1, failureBlock);
  histogramTester.ExpectTotalCount("PaymentRequest.TransactionAmount.Completed",
                                   0, failureBlock);

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

// Tests that the correct number of suggestions shown for each section is logged
// when a Payment Request is aborted by the user.
- (void)testNotShown_OnlyNotShownMetricsLogged {
  chrome_test_util::HistogramTester histogramTester;

  [self loadTestPage:"payment_request_can_make_payment_metrics_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"queryNoShow"];

  // Navigate away to abort the Payment Request and trigger the logs.
  [self loadTestPage:"payment_request_email_test.html"];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Abort should be logged.
  histogramTester.ExpectBucketCount("PaymentRequest.CheckoutFunnel.Aborted",
                                    JourneyLogger::ABORT_REASON_USER_NAVIGATION,
                                    1, failureBlock);

  // Some events should be logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");

  // Only USER_ABORTED and CAN_MAKE_PAYMENT_FALSE should be logged.
  GREYAssertEqual(JourneyLogger::EVENT_USER_ABORTED |
                      JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE |
                      JourneyLogger::EVENT_REQUEST_METHOD_OTHER |
                      JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD |
                      JourneyLogger::EVENT_NEEDS_COMPLETION_PAYMENT,
                  buckets[0].min, @"");

  // Make sure that the metrics that required the Payment Request to be shown
  // are not logged.
  histogramTester.ExpectTotalCount("PaymentRequest.SelectedPaymentMethod", 0,
                                   failureBlock);
  histogramTester.ExpectTotalCount("PaymentRequest.NumberOfSelectionAdds", 0,
                                   failureBlock);
  histogramTester.ExpectTotalCount("PaymentRequest.NumberOfSelectionChanges", 0,
                                   failureBlock);
  histogramTester.ExpectTotalCount("PaymentRequest.NumberOfSelectionEdits", 0,
                                   failureBlock);
  histogramTester.ExpectTotalCount("PaymentRequest.NumberOfSuggestionsShown", 0,
                                   failureBlock);

  // Make sure transaction amount metrics are not logged since the user
  // navigated away before triggering .show().
  histogramTester.ExpectTotalCount("PaymentRequest.TransactionAmount.Triggered",
                                   0, failureBlock);
  histogramTester.ExpectTotalCount("PaymentRequest.TransactionAmount.Completed",
                                   0, failureBlock);
}

- (void)testUserHadCompleteSuggestionsForEverything {
  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard1];

  [self loadTestPage:"payment_request_email_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Navigate away to abort the Payment Request and trigger the logs.
  [self loadTestPage:"payment_request_email_test.html"];

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

- (void)testUserHadIncompleteSuggestionsForEverything_NoCard {
  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];  // The user has no form of payment on file.

  [self loadTestPage:"payment_request_email_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Navigate away to abort the Payment Request and trigger the logs.
  [self loadTestPage:"payment_request_email_test.html"];

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

- (void)testUserHadIncompleteSuggestionsForEverything_CardNetworkNotSupported {
  chrome_test_util::HistogramTester histogramTester;

  [self addProfiles];
  [self addCard2];  // AMEX is not supported by the merchant.

  [self loadTestPage:"payment_request_email_test.html"];
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Navigate away to abort the Payment Request and trigger the logs.
  [self loadTestPage:"payment_request_email_test.html"];

  // Make sure the correct events were logged.
  std::vector<chrome_test_util::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COULD_NOT_SHOW, @"");
}

@end
