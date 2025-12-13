// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/data_quality/validation.h"
#import "components/autofill/core/common/credit_card_network_identifiers.h"
#import "components/autofill/core/common/credit_card_number_validation.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credit_card+CreditCard.h"
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

// Tests that the manual fill credit card build from virtual card has CVC.
TEST_F(ManualFillCreditCardFormAutofilliOSTest,
       ManualFillFromVirtualCardHasCVC) {
  NSString* GUID = @"1234-5678-abcd";
  NSString* number = @"1234";
  NSString* CVC = @"123";

  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_record_type(
      autofill::CreditCard::RecordType::kVirtualCard);
  autofillCreditCard.set_guid(
      base::UTF16ToASCII(base::SysNSStringToUTF16(GUID)));
  autofillCreditCard.SetNumber(base::SysNSStringToUTF16(number));
  autofillCreditCard.set_cvc(base::SysNSStringToUTF16(CVC));

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  EXPECT_NSEQ(manualFillCard.CVC, CVC);
}

// Tests that the manual fill credit card build from CardInfoRetrieval enrolled
// card has CVC and the enrollment state set.
TEST_F(ManualFillCreditCardFormAutofilliOSTest,
       ManualFillFromCardInfoRetrieval) {
  NSString* GUID = @"1234-5678-abcd";
  NSString* number = @"1234";
  NSString* CVC = @"123";

  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_card_info_retrieval_enrollment_state(
      autofill::CreditCard::CardInfoRetrievalEnrollmentState::
          kRetrievalEnrolled);
  autofillCreditCard.set_guid(
      base::UTF16ToASCII(base::SysNSStringToUTF16(GUID)));
  autofillCreditCard.SetNumber(base::SysNSStringToUTF16(number));
  autofillCreditCard.set_cvc(base::SysNSStringToUTF16(CVC));

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  EXPECT_NSEQ(manualFillCard.CVC, CVC);
  EXPECT_EQ(manualFillCard.cardInfoRetrievalEnrollmentState,
            autofill::CreditCard::CardInfoRetrievalEnrollmentState::
                kRetrievalEnrolled);
}

// Tests that a manual fill card created from a local card with a CVC has the
// CVC data.
TEST_F(ManualFillCreditCardFormAutofilliOSTest, ManualFillFromLocalCardHasCVC) {
  NSString* CVC = @"123";

  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_record_type(
      autofill::CreditCard::RecordType::kLocalCard);
  autofillCreditCard.set_cvc(base::SysNSStringToUTF16(CVC));

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  // For a local card, the CVC should be the actual value.
  EXPECT_NSEQ(manualFillCard.CVC, CVC);
}

// Tests that a manual fill card created from a server card has a masked CVC.
TEST_F(ManualFillCreditCardFormAutofilliOSTest,
       ManualFillFromServerCardHasMaskedCVC) {
  NSString* CVC = @"123";

  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_record_type(
      autofill::CreditCard::RecordType::kMaskedServerCard);

  autofillCreditCard.SetNetworkForMaskedCard(autofill::kMasterCard);
  autofillCreditCard.set_cvc(base::SysNSStringToUTF16(CVC));

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  // For a server card, the CVC should be masked.
  EXPECT_NSNE(manualFillCard.CVC, CVC);

  std::u16string expected_masked_cvc =
      autofill::CreditCard::GetMidlineEllipsisDots(
          autofill::GetCvcLengthForCardNetwork(autofillCreditCard.network()));

  EXPECT_NSEQ(manualFillCard.CVC,
              base::SysUTF16ToNSString(expected_masked_cvc));
}

// Tests that a manual fill card created from a server card without a CVC has
// no CVC data.
TEST_F(ManualFillCreditCardFormAutofilliOSTest,
       ManualFillFromServerCardWithoutCVCHasNoCVC) {
  CreditCard autofillCreditCard = CreditCard();
  autofillCreditCard.set_record_type(
      autofill::CreditCard::RecordType::kMaskedServerCard);
  // The CVC is intentionally not set on the autofillCreditCard object.

  ManualFillCreditCard* manualFillCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:autofillCreditCard
                                                  icon:nil];

  // For a server card without a CVC, the CVC property should be nil.
  EXPECT_EQ(manualFillCard.CVC, nil);
}
