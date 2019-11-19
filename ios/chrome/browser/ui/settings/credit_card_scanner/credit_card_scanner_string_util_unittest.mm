// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_string_util.h"

#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using CreditCardScannerStringUtilUnitTest = PlatformTest;

#pragma mark - Test ExtractExpirationDateFromText

// Tests extracting month and year from valid date text.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractExpirationDateFromValidDateText) {
  NSDateComponents* components = ios::ExtractExpirationDateFromText(@"10/25");

  EXPECT_EQ(components.month, 10);
  EXPECT_EQ(components.year, 2025);
}

// Tests extracting month and year from invalid date text.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractExpirationDateFromInvalidDateText) {
  NSDateComponents* components = ios::ExtractExpirationDateFromText(@"13/888");

  EXPECT_FALSE(components);
}

// Tests extracting month and year from invalid text.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractExpirationDateFromInvalidText) {
  NSDateComponents* components = ios::ExtractExpirationDateFromText(@"aaaaa");

  EXPECT_FALSE(components);
}

// Tests extracting month and year from invalid text with correct format.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractExpirationDateFromInvalidFormattedText) {
  NSDateComponents* components = ios::ExtractExpirationDateFromText(@"aa/aa");

  EXPECT_FALSE(components);
}

#pragma mark - Test ExtractCreditCardNumber

// Tests extracting card number from valid card number text (16 digits).
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractCardNumberFromValidCreditCardNumber16Digits) {
  NSString* cardNumber = ios::ExtractCreditCardNumber(@"4111111111111111");

  EXPECT_NSEQ(cardNumber, @"4111111111111111");
}

// Tests extracting card number from valid card number text (14 digits).
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractCardNumberFromValidCreditCardNumber14Digits) {
  NSString* cardNumber = ios::ExtractCreditCardNumber(@"36904001001529");

  EXPECT_NSEQ(cardNumber, @"36904001001529");
}

// Tests extracting card number from invalid card number text (14 digits).
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractCardNumberFromInvalidCreditCardNumber14Digits) {
  NSString* cardNumber = ios::ExtractCreditCardNumber(@"4111111111111");

  EXPECT_FALSE(cardNumber);
}

// Tests extracting card number from valid card number text contains wrong
// characters.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractCardNumberFromValidCreditCardNumberWithWrongCharacters) {
  NSString* cardNumber = ios::ExtractCreditCardNumber(@"41/11-1111 1111.11:11");

  EXPECT_NSEQ(cardNumber, @"4111111111111111");
}

// Tests extracting card number from text after converting
// illegal characters.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractCardNumberFromValidCreditCardNumberAfterConversion) {
  NSString* cardNumber = ios::ExtractCreditCardNumber(@"4U24u0TLzu6636B5");

  EXPECT_NSEQ(cardNumber, @"4024007170663685");
}

// Tests extracting card number from invalid card number text (10 digits).
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractCardNumberFromInvalidCreditCardNumber10Digits) {
  NSString* cardNumber = ios::ExtractCreditCardNumber(@"4111111111");

  EXPECT_FALSE(cardNumber);
}

// Tests extracting card number from invalid card number text.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestExtractCardNumberFromInvalidCreditCardNumber) {
  NSString* cardNumber = ios::ExtractCreditCardNumber(@"4111a11b11c11");

  EXPECT_FALSE(cardNumber);
}

#pragma mark - Test SubstituteSimilarCharactersInRecognizedText

// Tests substituting convertible characters with digits.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestSubstitutingTrueCharactersWithDigits) {
  NSString* number =
      ios::SubstituteSimilarCharactersInRecognizedText(@"bCdGiLoQsTuZ");

  EXPECT_NSEQ(number, @"800911005707");
}

// Tests substituting text without characters with digits.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestSubstitutingTextWithoutCharacters) {
  NSString* number =
      ios::SubstituteSimilarCharactersInRecognizedText(@"4111111111111111");

  EXPECT_NSEQ(number, @"4111111111111111");
}

// Tests substituting inconvertible characters with digits.
TEST_F(CreditCardScannerStringUtilUnitTest,
       TestSubstitutingFalseCharactersWithDigits) {
  NSString* cardNumber =
      ios::SubstituteSimilarCharactersInRecognizedText(@"abcdefghi");

  EXPECT_NSEQ(cardNumber, @"A800EF9H1");
}
