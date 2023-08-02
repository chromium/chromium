// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credit_card+CreditCard.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using autofill::CreditCard;
using ManualFillCreditCardFormAutofilliOSTest = PlatformTest;

// Tests the creation of a unobfuscated credit card from an
// autofill::CreditCard.
TEST_F(ManualFillCreditCardFormAutofilliOSTest, CreationUnobfuscated) {
  NSString* GUID = @"1234-5678-abcd";
  NSString* bankName = @"Bank of 'Merica";
  NSString* cardHolder = @"Fred Itcard";
  // Visa -> starts with 4.
  NSString* number = @"4321 4321 4321 1234";
  NSString* expirationYear = @"19";
  NSString* expirationMonth = @"1";

  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_guid(
      base::UTF16ToASCII(base::SysNSStringToUTF16(GUID)));
  autofillCreditCard.SetNumber(base::SysNSStringToUTF16(number));
  autofillCreditCard.set_bank_name(base::SysNSStringToUTF8(bankName));
  autofillCreditCard.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                                base::SysNSStringToUTF16(cardHolder));
  autofillCreditCard.SetRawInfo(autofill::CREDIT_CARD_EXP_MONTH,
                                base::SysNSStringToUTF16(expirationMonth));
  autofillCreditCard.SetRawInfo(autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                base::SysNSStringToUTF16(expirationYear));

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard];

  EXPECT_TRUE(manualFillCard);
  EXPECT_TRUE([GUID isEqualToString:manualFillCard.GUID]);
  EXPECT_TRUE([@"Visa" isEqualToString:manualFillCard.network]);
  EXPECT_TRUE([bankName isEqualToString:manualFillCard.bankName]);
  EXPECT_TRUE([@"4321432143211234" isEqualToString:manualFillCard.number]);
  EXPECT_TRUE([manualFillCard.obfuscatedNumber containsString:@"1234"]);
  EXPECT_FALSE([manualFillCard.obfuscatedNumber containsString:@"4321"]);
  EXPECT_TRUE([cardHolder isEqualToString:manualFillCard.cardHolder]);
  // Test month and padding of months.
  EXPECT_TRUE([@"01" isEqualToString:manualFillCard.expirationMonth]);
  EXPECT_TRUE([expirationYear isEqualToString:manualFillCard.expirationYear]);
}

// Tests the creation of an obfuscated credit card from an
// autofill::CreditCard.
TEST_F(ManualFillCreditCardFormAutofilliOSTest, CreationObfuscated) {
  NSString* GUID = @"1234-5678-abcd";
  NSString* bankName = @"Bank of 'Merica";
  NSString* cardHolder = @"Fred Itcard";
  NSString* number = @"1234";
  NSString* expirationYear = @"19";
  NSString* expirationMonth = @"1";

  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_record_type(autofill::CreditCard::MASKED_SERVER_CARD);
  autofillCreditCard.set_guid(
      base::UTF16ToASCII(base::SysNSStringToUTF16(GUID)));
  autofillCreditCard.SetNumber(base::SysNSStringToUTF16(number));
  autofillCreditCard.SetNetworkForMaskedCard(autofill::kVisaCard);
  autofillCreditCard.set_bank_name(base::SysNSStringToUTF8(bankName));
  autofillCreditCard.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                                base::SysNSStringToUTF16(cardHolder));
  autofillCreditCard.SetRawInfo(autofill::CREDIT_CARD_EXP_MONTH,
                                base::SysNSStringToUTF16(expirationMonth));
  autofillCreditCard.SetRawInfo(autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                base::SysNSStringToUTF16(expirationYear));

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard];

  EXPECT_TRUE(manualFillCard);
  EXPECT_TRUE([GUID isEqualToString:manualFillCard.GUID]);
  EXPECT_TRUE([@"Visa" isEqualToString:manualFillCard.network]);
  EXPECT_TRUE([bankName isEqualToString:manualFillCard.bankName]);
  EXPECT_FALSE(manualFillCard.number);
  EXPECT_TRUE([manualFillCard.obfuscatedNumber containsString:@"1234"]);
  EXPECT_FALSE([manualFillCard.obfuscatedNumber containsString:@"4321"]);
  EXPECT_TRUE([cardHolder isEqualToString:manualFillCard.cardHolder]);
  // Test month and padding of months.
  EXPECT_TRUE([@"01" isEqualToString:manualFillCard.expirationMonth]);
  EXPECT_TRUE([expirationYear isEqualToString:manualFillCard.expirationYear]);
}
