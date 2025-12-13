// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_util.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
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
                cardCvc:@""
               appLocal:"en"]);
  EXPECT_FALSE([AutofillCreditCardUtil
      isValidCreditCard:@"5105105105105100"
        expirationMonth:@"12"
         expirationYear:SysUTF8ToNSString(autofill::test::NextYear())
           cardNickname:@""
                cardCvc:@"12345"
               appLocal:"en"]);
  EXPECT_TRUE([AutofillCreditCardUtil
      isValidCreditCard:@"5105105105105100"
        expirationMonth:@"12"
         expirationYear:SysUTF8ToNSString(autofill::test::NextYear())
           cardNickname:@""
                cardCvc:@"1234"
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
                                               cardCvc:@""
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
                                   cardCvc:@""
                                  appLocal:"en"];
  EXPECT_EQ(card.expiration_month(), 3);
  EXPECT_EQ(card.expiration_year(), 2031);
}

// Test that `AutofillCreditCardUtilTest::CreateTextViewForLegalMessage` creates
// a text view for given SaveCardMessageWithLinks.
TEST_F(AutofillCreditCardUtilTest, CreateTextViewForLegalMessage) {
  autofill::LegalMessageLines legal_message_lines =
      autofill::LegalMessageLines({autofill::TestLegalMessageLine(
          /*ascii_text=*/"Save Card Legal Message Text",
          /*links=*/{
              autofill::LegalMessageLine::Link(
                  /*start=*/10, /*end=*/23,
                  /*url_spec=*/"https://savecard.test"),
          })});
  NSMutableArray<SaveCardMessageWithLinks*>* save_card_messages =
      [SaveCardMessageWithLinks convertFrom:legal_message_lines];

  UITextView* text_view = [AutofillCreditCardUtil
      createTextViewForLegalMessage:save_card_messages[0]];

  EXPECT_FALSE(text_view.editable);
  EXPECT_NSEQ(text_view.attributedText.string,
              base::SysUTF16ToNSString(legal_message_lines[0].text()));
  NSRange textRange;
  NSURL* nsurl = [text_view.attributedText attribute:NSLinkAttributeName
                                             atIndex:10
                                      effectiveRange:&textRange];
  autofill::LegalMessageLine::Link link = legal_message_lines[0].links()[0];
  EXPECT_TRUE(NSEqualRanges(link.range.ToNSRange(), textRange));
  EXPECT_NSEQ(([[CrURL alloc] initWithGURL:link.url]).nsurl, nsurl);
}
