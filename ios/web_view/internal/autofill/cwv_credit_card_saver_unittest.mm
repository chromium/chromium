// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_saver_internal.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {
class CWVCreditCardSaverTest : public TestWithLocaleAndResources {
 protected:
  CWVCreditCardSaverTest() {}

  web::WebTaskEnvironment task_environment_;
};

// Tests CWVCreditCardSaver properly initializes.
TEST_F(CWVCreditCardSaverTest, Initialization) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions options;
  autofill::LegalMessageLines legal_message_lines = {
      autofill::TestLegalMessageLine("Test line 1",
                                     {autofill::LegalMessageLine::Link(
                                         5, 9, "http://www.chromium.org/")})};
  autofill::payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      callback;

  CWVCreditCardSaver* credit_card_saver =
      [[CWVCreditCardSaver alloc] initWithCreditCard:credit_card
                                         saveOptions:options
                                   legalMessageLines:legal_message_lines
                                  savePromptCallback:std::move(callback)];

  EXPECT_EQ(credit_card, *credit_card_saver.creditCard.internalCard);
  ASSERT_EQ(1U, credit_card_saver.legalMessages.count);
  NSAttributedString* legal_message =
      credit_card_saver.legalMessages.firstObject;
  EXPECT_NSEQ(@"Test line 1", legal_message.string);
  NSRange range;
  id link = [legal_message attribute:NSLinkAttributeName
                             atIndex:5
                      effectiveRange:&range];
  EXPECT_NSEQ([NSURL URLWithString:@"http://www.chromium.org/"], link);
  EXPECT_TRUE(NSEqualRanges(NSMakeRange(5, 4), range));
}

// Tests when user ignores credit card save.
TEST_F(CWVCreditCardSaverTest, Ignore) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions options;

  BOOL callback_called = NO;
  autofill::payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      callback = base::BindLambdaForTesting(
          [&](autofill::payments::PaymentsAutofillClient::
                  SaveCardOfferUserDecision decision,
              const autofill::payments::PaymentsAutofillClient::
                  UserProvidedCardDetails& user_provided_card_details) {
            callback_called = YES;
            EXPECT_EQ(autofill::payments::PaymentsAutofillClient::
                          SaveCardOfferUserDecision::kIgnored,
                      decision);
          });

  [[maybe_unused]] CWVCreditCardSaver* credit_card_saver =
      [[CWVCreditCardSaver alloc] initWithCreditCard:credit_card
                                         saveOptions:options
                                   legalMessageLines:{}
                                  savePromptCallback:std::move(callback)];
  // Force -[CWVCreditCardSaver dealloc].
  credit_card_saver = nil;

  EXPECT_TRUE(callback_called);
}

// Tests when user declines a save.
TEST_F(CWVCreditCardSaverTest, Decline) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions options;
  autofill::payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
      local_callback;

  BOOL callback_called = NO;
  autofill::payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      callback = base::BindLambdaForTesting(
          [&](autofill::payments::PaymentsAutofillClient::
                  SaveCardOfferUserDecision decision,
              const autofill::payments::PaymentsAutofillClient::
                  UserProvidedCardDetails& user_provided_card_details) {
            callback_called = YES;
            EXPECT_EQ(autofill::payments::PaymentsAutofillClient::
                          SaveCardOfferUserDecision::kDeclined,
                      decision);
          });

  CWVCreditCardSaver* credit_card_saver =
      [[CWVCreditCardSaver alloc] initWithCreditCard:credit_card
                                         saveOptions:options
                                   legalMessageLines:{}
                                  savePromptCallback:std::move(callback)];
  [credit_card_saver decline];

  EXPECT_TRUE(callback_called);
}

// Tests when user accepts a save.
TEST_F(CWVCreditCardSaverTest, Accept) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions options;

  BOOL callback_called = NO;
  autofill::payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      callback = base::BindLambdaForTesting(
          [&](autofill::payments::PaymentsAutofillClient::
                  SaveCardOfferUserDecision decision,
              const autofill::payments::PaymentsAutofillClient::
                  UserProvidedCardDetails& user_provided_card_details) {
            callback_called = YES;
            EXPECT_EQ(autofill::payments::PaymentsAutofillClient::
                          SaveCardOfferUserDecision::kAccepted,
                      decision);
            EXPECT_EQ(u"John Doe", user_provided_card_details.cardholder_name);
            EXPECT_EQ(u"08", user_provided_card_details.expiration_date_month);
            EXPECT_EQ(u"2021", user_provided_card_details.expiration_date_year);
          });

  CWVCreditCardSaver* credit_card_saver =
      [[CWVCreditCardSaver alloc] initWithCreditCard:credit_card
                                         saveOptions:options
                                   legalMessageLines:{}
                                  savePromptCallback:std::move(callback)];
  BOOL risk_data_used = NO;
  [credit_card_saver loadRiskData:base::BindLambdaForTesting(
                                      [&](const std::string& risk_data) {
                                        EXPECT_EQ(risk_data, "dummy-risk-data");
                                        risk_data_used = YES;
                                      })];
  __block BOOL completion_called = NO;
  [credit_card_saver acceptWithCardHolderFullName:@"John Doe"
                                  expirationMonth:@"08"
                                   expirationYear:@"2021"
                                         riskData:@"dummy-risk-data"
                                completionHandler:^(BOOL cardSaved) {
                                  EXPECT_TRUE(cardSaved);
                                  completion_called = YES;
                                }];
  [credit_card_saver handleCreditCardUploadCompleted:YES];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return completion_called;
  }));

  EXPECT_TRUE(risk_data_used);
  EXPECT_TRUE(callback_called);
}

}  // namespace ios_web_view
