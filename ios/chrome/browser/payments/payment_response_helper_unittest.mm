// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_response_helper.h"

#include <memory>
#include <string>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/payment_request_data_util.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PaymentResponseHelperConsumerMock
    : OCMockComplexTypeHelper<PaymentResponseHelperConsumer>
@end

@implementation PaymentResponseHelperConsumerMock

typedef void (^mock_payment_response_helper_did_complete_with_payment_response)(
    const payments::PaymentResponse&);

- (void)paymentResponseHelperDidReceivePaymentMethodDetails {
}

- (void)paymentResponseHelperDidFailToReceivePaymentMethodDetails {
}

- (void)paymentResponseHelperDidCompleteWithPaymentResponse:
    (const payments::PaymentResponse&)paymentResponse {
  return static_cast<
      mock_payment_response_helper_did_complete_with_payment_response>(
      [self blockForSelector:_cmd])(paymentResponse);
}

@end

namespace payments {

class PaymentRequestPaymentResponseHelperTest : public PlatformTest {
 protected:
  PaymentRequestPaymentResponseHelperTest()
      : profile_(autofill::test::GetFullProfile()),
        credit_card_(autofill::test::GetCreditCard()),
        chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {
    personal_data_manager_.SetAutofillProfileEnabled(true);
    personal_data_manager_.SetAutofillCreditCardEnabled(true);
    personal_data_manager_.SetAutofillWalletImportEnabled(true);
    personal_data_manager_.AddProfile(profile_);
    payment_request_ = std::make_unique<TestPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        chrome_browser_state_.get(), &web_state_, &personal_data_manager_);
  }

  std::string GetMethodName() {
    return autofill::data_util::GetPaymentRequestData(credit_card_.network())
        .basic_card_issuer_network;
  }

  std::string GetStringifiedDetails() {
    autofill::AutofillProfile billing_address;
    std::unique_ptr<base::DictionaryValue> response_value =
        data_util::GetBasicCardResponseFromAutofillCreditCard(
            credit_card_, base::ASCIIToUTF16("123"), billing_address, "en-US")
            ->ToDictionaryValue();
    std::string stringified_details;
    base::JSONWriter::Write(*response_value, &stringified_details);
    return stringified_details;
  }

  TestPaymentRequest* payment_request() { return payment_request_.get(); }

 private:
  base::test::TaskEnvironment scoped_task_evironment_;

  autofill::AutofillProfile profile_;
  autofill::CreditCard credit_card_;
  web::TestWebState web_state_;
  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestPaymentRequest> payment_request_;
};

// Tests that calling the PaymentResponseHelper's delegate method which signals
// that the full payment method details have been successfully received, causes
// the PaymentResponseHelper's delegate method to get called with the
// appropriate payment response.
TEST_F(PaymentRequestPaymentResponseHelperTest, PaymentResponse) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentResponseHelperConsumer)];
  id consumer_mock([[PaymentResponseHelperConsumerMock alloc]
      initWithRepresentedObject:consumer]);
  SEL selector =
      @selector(paymentResponseHelperDidCompleteWithPaymentResponse:);
  [consumer_mock onSelector:selector
       callBlockExpectation:^(const PaymentResponse& response) {
         // Check if all the expected values were set.
         EXPECT_EQ(GetMethodName(), response.method_name);
         EXPECT_EQ(GetStringifiedDetails(), response.details);

         EXPECT_TRUE(!!response.shipping_address);
         EXPECT_EQ("US", response.shipping_address->country);
         ASSERT_EQ(2U, response.shipping_address->address_line.size());
         EXPECT_EQ("666 Erebus St.",
                   response.shipping_address->address_line[0]);
         EXPECT_EQ("Apt 8", response.shipping_address->address_line[1]);
         EXPECT_EQ("CA", response.shipping_address->region);
         EXPECT_EQ("Elysium", response.shipping_address->city);
         EXPECT_EQ(std::string(),
                   response.shipping_address->dependent_locality);
         EXPECT_EQ("91111", response.shipping_address->postal_code);
         EXPECT_EQ(std::string(), response.shipping_address->sorting_code);
         EXPECT_EQ("Underworld", response.shipping_address->organization);
         EXPECT_EQ("John H. Doe", response.shipping_address->recipient);
         EXPECT_EQ("16502111111", response.shipping_address->phone);

         EXPECT_EQ(base::ASCIIToUTF16("John H. Doe"), response.payer_name);
         EXPECT_EQ(base::ASCIIToUTF16("+16502111111"), response.payer_phone);
         EXPECT_EQ(base::ASCIIToUTF16("johndoe@hades.com"),
                   response.payer_email);
       }];

  PaymentResponseHelper payment_response_helper(consumer_mock,
                                                payment_request());
  payment_response_helper.OnInstrumentDetailsReady(
      GetMethodName(), GetStringifiedDetails(), PayerData());
}

// Tests that the generated PaymentResponse has a shipping address only if one
// is requested.
TEST_F(PaymentRequestPaymentResponseHelperTest, PaymentResponseNoShipping) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentResponseHelperConsumer)];
  id consumer_mock([[PaymentResponseHelperConsumerMock alloc]
      initWithRepresentedObject:consumer]);
  SEL selector =
      @selector(paymentResponseHelperDidCompleteWithPaymentResponse:);
  [consumer_mock onSelector:selector
       callBlockExpectation:^(const PaymentResponse& response) {
         EXPECT_FALSE(!!response.shipping_address);
         EXPECT_EQ(base::ASCIIToUTF16("John H. Doe"), response.payer_name);
         EXPECT_EQ(base::ASCIIToUTF16("+16502111111"), response.payer_phone);
         EXPECT_EQ(base::ASCIIToUTF16("johndoe@hades.com"),
                   response.payer_email);
       }];

  payment_request()->web_payment_request().options.request_shipping = false;
  PaymentResponseHelper payment_response_helper(consumer_mock,
                                                payment_request());
  payment_response_helper.OnInstrumentDetailsReady(
      GetMethodName(), GetStringifiedDetails(), PayerData());
}

// Tests that the generated PaymentResponse has contact information only if it
// is requested.
TEST_F(PaymentRequestPaymentResponseHelperTest, PaymentResponseNoContact) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentResponseHelperConsumer)];
  id consumer_mock([[PaymentResponseHelperConsumerMock alloc]
      initWithRepresentedObject:consumer]);
  SEL selector =
      @selector(paymentResponseHelperDidCompleteWithPaymentResponse:);
  [consumer_mock onSelector:selector
       callBlockExpectation:^(const PaymentResponse& response) {
         EXPECT_EQ(base::string16(), response.payer_name);
         EXPECT_EQ(base::string16(), response.payer_phone);
         EXPECT_EQ(base::string16(), response.payer_email);
       }];

  payment_request()->web_payment_request().options.request_payer_name = false;
  payment_request()->web_payment_request().options.request_payer_phone = false;
  payment_request()->web_payment_request().options.request_payer_email = false;
  PaymentResponseHelper payment_response_helper(consumer_mock,
                                                payment_request());
  payment_response_helper.OnInstrumentDetailsReady(
      GetMethodName(), GetStringifiedDetails(), PayerData());
}

// Tests that the generated PaymentResponse has contact information only if it
// is requested.
TEST_F(PaymentRequestPaymentResponseHelperTest, PaymentResponseOneContact) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentResponseHelperConsumer)];
  id consumer_mock([[PaymentResponseHelperConsumerMock alloc]
      initWithRepresentedObject:consumer]);
  SEL selector =
      @selector(paymentResponseHelperDidCompleteWithPaymentResponse:);
  [consumer_mock onSelector:selector
       callBlockExpectation:^(const PaymentResponse& response) {
         EXPECT_EQ(base::ASCIIToUTF16("John H. Doe"), response.payer_name);
         EXPECT_EQ(base::string16(), response.payer_phone);
         EXPECT_EQ(base::string16(), response.payer_email);
       }];

  payment_request()->web_payment_request().options.request_payer_phone = false;
  payment_request()->web_payment_request().options.request_payer_email = false;
  PaymentResponseHelper payment_response_helper(consumer_mock,
                                                payment_request());
  payment_response_helper.OnInstrumentDetailsReady(
      GetMethodName(), GetStringifiedDetails(), PayerData());
}

// Tests that the generated PaymentResponse has contact information only if it
// is requested.
TEST_F(PaymentRequestPaymentResponseHelperTest, PaymentResponseSomeContact) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentResponseHelperConsumer)];
  id consumer_mock([[PaymentResponseHelperConsumerMock alloc]
      initWithRepresentedObject:consumer]);
  SEL selector =
      @selector(paymentResponseHelperDidCompleteWithPaymentResponse:);
  [consumer_mock onSelector:selector
       callBlockExpectation:^(const PaymentResponse& response) {
         EXPECT_EQ(base::ASCIIToUTF16("John H. Doe"), response.payer_name);
         EXPECT_EQ(base::ASCIIToUTF16("johndoe@hades.com"),
                   response.payer_email);
         EXPECT_EQ(base::string16(), response.payer_phone);
       }];

  payment_request()->web_payment_request().options.request_payer_phone = false;
  PaymentResponseHelper payment_response_helper(consumer_mock,
                                                payment_request());
  payment_response_helper.OnInstrumentDetailsReady(
      GetMethodName(), GetStringifiedDetails(), PayerData());
}

// Tests that the phone number in the contact information of the generated
// PaymentResponse is formatted into E.164 if the number is valid.
TEST_F(PaymentRequestPaymentResponseHelperTest,
       PaymentResponseContactPhoneIsFormatted_IfNumberIsValid) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentResponseHelperConsumer)];
  id consumer_mock([[PaymentResponseHelperConsumerMock alloc]
      initWithRepresentedObject:consumer]);
  SEL selector =
      @selector(paymentResponseHelperDidCompleteWithPaymentResponse:);
  [consumer_mock onSelector:selector
       callBlockExpectation:^(const PaymentResponse& response) {
         EXPECT_EQ(base::ASCIIToUTF16("+15152231234"), response.payer_phone);
       }];

  payment_request()->selected_contact_profile()->SetRawInfo(
      autofill::PHONE_HOME_WHOLE_NUMBER, base::UTF8ToUTF16("(515) 223-1234"));

  payment_request()->web_payment_request().options.request_payer_name = false;
  payment_request()->web_payment_request().options.request_payer_email = false;
  PaymentResponseHelper payment_response_helper(consumer_mock,
                                                payment_request());
  payment_response_helper.OnInstrumentDetailsReady(
      GetMethodName(), GetStringifiedDetails(), PayerData());
}

// Tests that the phone number in the contact information of the generated
// PaymentResponse is not minimumly formatted(removing non-digit letters) if
// the number is invalid.
TEST_F(PaymentRequestPaymentResponseHelperTest,
       PaymentResponseContactPhoneIsMinimumlyFormatted_IfNumberIsInValid) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentResponseHelperConsumer)];
  id consumer_mock([[PaymentResponseHelperConsumerMock alloc]
      initWithRepresentedObject:consumer]);
  SEL selector =
      @selector(paymentResponseHelperDidCompleteWithPaymentResponse:);
  [consumer_mock onSelector:selector
       callBlockExpectation:^(const PaymentResponse& response) {
         EXPECT_EQ(base::ASCIIToUTF16("5151231234"), response.payer_phone);
       }];

  payment_request()->selected_contact_profile()->SetRawInfo(
      autofill::PHONE_HOME_WHOLE_NUMBER, base::UTF8ToUTF16("(515) 123-1234"));

  payment_request()->web_payment_request().options.request_payer_name = false;
  payment_request()->web_payment_request().options.request_payer_email = false;
  PaymentResponseHelper payment_response_helper(consumer_mock,
                                                payment_request());
  payment_response_helper.OnInstrumentDetailsReady(
      GetMethodName(), GetStringifiedDetails(), PayerData());
}

}  // payments
