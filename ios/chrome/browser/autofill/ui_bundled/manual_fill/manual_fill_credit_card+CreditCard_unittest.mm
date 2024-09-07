// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credit_card+CreditCard.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/common/credit_card_network_identifiers.h"
#import "testing/gtest_mac.h"
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
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  EXPECT_TRUE(manualFillCard);
  EXPECT_NSEQ(GUID, manualFillCard.GUID);
  EXPECT_NSEQ(@"Visa", manualFillCard.network);
  EXPECT_NSEQ(bankName, manualFillCard.bankName);
  EXPECT_NSEQ(@"4321432143211234", manualFillCard.number);
  EXPECT_TRUE([manualFillCard.obfuscatedNumber containsString:@"1234"]);
  EXPECT_FALSE([manualFillCard.obfuscatedNumber containsString:@"4321"]);
  EXPECT_NSEQ(cardHolder, manualFillCard.cardHolder);
  // Test month and padding of months.
  EXPECT_NSEQ(@"01", manualFillCard.expirationMonth);
  EXPECT_NSEQ(expirationYear, manualFillCard.expirationYear);
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
  autofillCreditCard.set_record_type(
      autofill::CreditCard::RecordType::kMaskedServerCard);
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
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  EXPECT_TRUE(manualFillCard);
  EXPECT_NSEQ(GUID, manualFillCard.GUID);
  EXPECT_NSEQ(@"Visa", manualFillCard.network);
  EXPECT_NSEQ(bankName, manualFillCard.bankName);
  EXPECT_FALSE(manualFillCard.number);
  EXPECT_TRUE([manualFillCard.obfuscatedNumber containsString:@"1234"]);
  EXPECT_FALSE([manualFillCard.obfuscatedNumber containsString:@"4321"]);
  EXPECT_NSEQ(cardHolder, manualFillCard.cardHolder);
  // Test month and padding of months.
  EXPECT_NSEQ(@"01", manualFillCard.expirationMonth);
  EXPECT_NSEQ(expirationYear, manualFillCard.expirationYear);
}

// Tests that a local card's canFillDirectly is true
TEST_F(ManualFillCreditCardFormAutofilliOSTest, CanFillDirectly) {
  NSString* GUID = @"1234-5678-abcd";
  NSString* number = @"1234";

  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_record_type(
      autofill::CreditCard::RecordType::kLocalCard);
  autofillCreditCard.set_guid(
      base::UTF16ToASCII(base::SysNSStringToUTF16(GUID)));
  autofillCreditCard.SetNumber(base::SysNSStringToUTF16(number));

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  EXPECT_TRUE(manualFillCard);
  EXPECT_TRUE(manualFillCard.canFillDirectly);
}

// Tests that a virtual card's canFillDirectly is false
TEST_F(ManualFillCreditCardFormAutofilliOSTest, VirtualCardCanNotFillDirectly) {
  NSString* GUID = @"1234-5678-abcd";
  NSString* number = @"1234";

  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_record_type(
      autofill::CreditCard::RecordType::kVirtualCard);
  autofillCreditCard.set_guid(
      base::UTF16ToASCII(base::SysNSStringToUTF16(GUID)));
  autofillCreditCard.SetNumber(base::SysNSStringToUTF16(number));

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  EXPECT_TRUE(manualFillCard);
  EXPECT_FALSE(manualFillCard.canFillDirectly);
}

// Tests that a masked card's canFillDirectly is false
TEST_F(ManualFillCreditCardFormAutofilliOSTest, MaskedCardCanNotFillDirectly) {
  NSString* GUID = @"1234-5678-abcd";
  NSString* number = @"1234";

  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_record_type(
      autofill::CreditCard::RecordType::kMaskedServerCard);
  autofillCreditCard.set_guid(
      base::UTF16ToASCII(base::SysNSStringToUTF16(GUID)));
  autofillCreditCard.SetNumber(base::SysNSStringToUTF16(number));

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  EXPECT_TRUE(manualFillCard);
  EXPECT_FALSE(manualFillCard.canFillDirectly);
}
