// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_saver_internal.h"

#import <UIKit/UIKit.h>
#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  autofill::AutofillClient::SaveCreditCardOptions options;
  autofill::LegalMessageLines legal_message_lines = {
      autofill::TestLegalMessageLine("Test line 1",
                                     {autofill::LegalMessageLine::Link(
                                         5, 9, "http://www.chromium.org/")})};
  autofill::AutofillClient::UploadSaveCardPromptCallback callback;

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
  autofill::AutofillClient::SaveCreditCardOptions options;

  BOOL callback_called = NO;
  autofill::AutofillClient::UploadSaveCardPromptCallback callback =
      base::BindLambdaForTesting(
          [&](autofill::AutofillClient::SaveCardOfferUserDecision decision,
              const autofill::AutofillClient::UserProvidedCardDetails&
                  user_provided_card_details) {
            callback_called = YES;
            EXPECT_EQ(autofill::AutofillClient::IGNORED, decision);
          });

  CWVCreditCardSaver* credit_card_saver =
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
  autofill::AutofillClient::SaveCreditCardOptions options;
  autofill::AutofillClient::LocalSaveCardPromptCallback local_callback;

  BOOL callback_called = NO;
  autofill::AutofillClient::UploadSaveCardPromptCallback callback =
      base::BindLambdaForTesting(
          [&](autofill::AutofillClient::SaveCardOfferUserDecision decision,
              const autofill::AutofillClient::UserProvidedCardDetails&
                  user_provided_card_details) {
            callback_called = YES;
            EXPECT_EQ(autofill::AutofillClient::DECLINED, decision);
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
  autofill::AutofillClient::SaveCreditCardOptions options;

  BOOL callback_called = NO;
  autofill::AutofillClient::UploadSaveCardPromptCallback callback =
      base::BindLambdaForTesting(
          [&](autofill::AutofillClient::SaveCardOfferUserDecision decision,
              const autofill::AutofillClient::UserProvidedCardDetails&
                  user_provided_card_details) {
            callback_called = YES;
            EXPECT_EQ(autofill::AutofillClient::ACCEPTED, decision);
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
  [credit_card_saver acceptWithRiskData:@"dummy-risk-data"
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
