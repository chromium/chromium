// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using AutofillCreditCardUtilTest = PlatformTest;
using base::SysUTF8ToNSString;

// Tests that the `AutofillCreditCardUtil::isValidCreditCardExpirationYear`
// returns false when the expiration year is invalid.
TEST_F(AutofillCreditCardUtilTest, TestExpiryYear) {
  EXPECT_FALSE([AutofillCreditCardUtil
      isValidCreditCardExpirationYear:SysUTF8ToNSString(
                                          autofill::test::LastYear())
                             appLocal:"en"]);
  EXPECT_TRUE([AutofillCreditCardUtil
      isValidCreditCardExpirationYear:SysUTF8ToNSString(
                                          autofill::test::NextYear())
                             appLocal:"en"]);
}

// Tests that the `AutofillCreditCardUtil::isValidCreditCardExpirationMonth`
// returns false when the expiration month is invalid.
TEST_F(AutofillCreditCardUtilTest, TestExpiryMonth) {
  EXPECT_FALSE([AutofillCreditCardUtil isValidCreditCardExpirationMonth:@"13"]);
  EXPECT_TRUE([AutofillCreditCardUtil isValidCreditCardExpirationMonth:@"1"]);
}

// Tests that the `AutofillCreditCardUtil::isValidCreditCardNumber` returns
// false when the credit card number is invalid.
TEST_F(AutofillCreditCardUtilTest, TestCreditCardNumber) {
  EXPECT_FALSE([AutofillCreditCardUtil isValidCreditCardNumber:@"13"
                                                      appLocal:"en"]);
  EXPECT_TRUE([AutofillCreditCardUtil
      isValidCreditCardNumber:@"5105105105105100"
                     appLocal:"en"]);
}

// Tests that the `AutofillCreditCardUtil::isValidCreditCard` returns false when
// the credit card details are invalid.
TEST_F(AutofillCreditCardUtilTest, TestCreditCardValidity) {
  EXPECT_FALSE([AutofillCreditCardUtil
      isValidCreditCard:@"13"
        expirationMonth:@"13"
         expirationYear:SysUTF8ToNSString(autofill::test::LastYear())
           cardNickname:@""
               appLocal:"en"]);
  EXPECT_TRUE([AutofillCreditCardUtil
      isValidCreditCard:@"5105105105105100"
        expirationMonth:@"12"
         expirationYear:SysUTF8ToNSString(autofill::test::NextYear())
           cardNickname:@""
               appLocal:"en"]);
}

// Tests that the `AutofillCreditCardUtil::updateCreditCard` updates the credit
// card data.
TEST_F(AutofillCreditCardUtilTest, TestCreditCardData) {
  autofill::CreditCard card =
      [AutofillCreditCardUtil creditCardWithHolderName:@"Test"
                                            cardNumber:@"5105105105105100"
                                       expirationMonth:@"12"
                                        expirationYear:@"2030"
                                          cardNickname:@""
                                              appLocal:"en"];
  EXPECT_EQ(card.expiration_month(), 12);
  EXPECT_EQ(card.expiration_year(), 2030);
  EXPECT_EQ(card.number(), u"5105105105105100");

  [AutofillCreditCardUtil updateCreditCard:&card
                            cardHolderName:@"Test"
                                cardNumber:@"5105105105105100"
                           expirationMonth:@"03"
                            expirationYear:@"2031"
                              cardNickname:@""
                                  appLocal:"en"];
  EXPECT_EQ(card.expiration_month(), 3);
  EXPECT_EQ(card.expiration_year(), 2031);
}
