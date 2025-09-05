// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_string_util.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using CreditCardScannerStringUtilTest = PlatformTest;

#pragma mark - Test ExtractExpirationDateFromText

// Tests extracting month and year from valid date text.
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractExpirationDateFromValidDateText) {
  NSDateComponents* components = ExtractExpirationDateFromText(@"10/25");

  EXPECT_EQ(components.month, 10);
  EXPECT_EQ(components.year, 2025);
}

// Tests extracting month and year from a valid date text with extra text.
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractExpirationDateFromValidDateTextWithExtraText) {
  NSDateComponents* components =
      ExtractExpirationDateFromText(@"Valid Thru: 10/25");

  EXPECT_EQ(components.month, 10);
  EXPECT_EQ(components.year, 2025);
}

// Tests extracting month and year from invalid date text.
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractExpirationDateFromInvalidDateText) {
  NSDateComponents* components = ExtractExpirationDateFromText(@"13/888");

  EXPECT_FALSE(components);
}

// Tests extracting month and year from invalid text.
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractExpirationDateFromInvalidText) {
  NSDateComponents* components = ExtractExpirationDateFromText(@"aaaaa");

  EXPECT_FALSE(components);
}

// Tests extracting month and year from invalid text with correct format.
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractExpirationDateFromInvalidFormattedText) {
  NSDateComponents* components = ExtractExpirationDateFromText(@"aa/aa");

  EXPECT_FALSE(components);
}

#pragma mark - Test ExtractCreditCardNumber

// Tests extracting card number from valid card number text (16 digits).
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractCardNumberFromValidCreditCardNumber16Digits) {
  NSString* cardNumber = ExtractCreditCardNumber(@"4111111111111111");

  EXPECT_NSEQ(cardNumber, @"4111111111111111");
}

// Tests extracting card number from valid card number text (14 digits).
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractCardNumberFromValidCreditCardNumber14Digits) {
  NSString* cardNumber = ExtractCreditCardNumber(@"36904001001529");

  EXPECT_NSEQ(cardNumber, @"36904001001529");
}

// Tests extracting card number from invalid card number text (14 digits).
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractCardNumberFromInvalidCreditCardNumber14Digits) {
  NSString* cardNumber = ExtractCreditCardNumber(@"4111111111111");

  EXPECT_FALSE(cardNumber);
}

// Tests extracting card number from valid card number text contains wrong
// characters.
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractCardNumberFromValidCreditCardNumberWithWrongCharacters) {
  NSString* cardNumber = ExtractCreditCardNumber(@"41/11-1111 1111.11:11");

  EXPECT_NSEQ(cardNumber, @"4111111111111111");
}

// Tests extracting card number from text after converting
// illegal characters.
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractCardNumberFromValidCreditCardNumberAfterConversion) {
  NSString* cardNumber = ExtractCreditCardNumber(@"4U24u0TLzu6636B5");

  EXPECT_NSEQ(cardNumber, @"4024007170663685");
}

// Tests extracting card number from invalid card number text (10 digits).
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractCardNumberFromInvalidCreditCardNumber10Digits) {
  NSString* cardNumber = ExtractCreditCardNumber(@"4111111111");

  EXPECT_FALSE(cardNumber);
}

// Tests extracting card number from invalid card number text.
TEST_F(CreditCardScannerStringUtilTest,
       TestExtractCardNumberFromInvalidCreditCardNumber) {
  NSString* cardNumber = ExtractCreditCardNumber(@"4111a11b11c11");

  EXPECT_FALSE(cardNumber);
}

#pragma mark - Test SubstituteSimilarCharactersInRecognizedText

// Tests substituting convertible characters with digits.
TEST_F(CreditCardScannerStringUtilTest,
       TestSubstitutingTrueCharactersWithDigits) {
  NSString* number =
      SubstituteSimilarCharactersInRecognizedText(@"bCdGiLoQsTuZ");

  EXPECT_NSEQ(number, @"800911005707");
}

// Tests substituting text without characters with digits.
TEST_F(CreditCardScannerStringUtilTest, TestSubstitutingTextWithoutCharacters) {
  NSString* number =
      SubstituteSimilarCharactersInRecognizedText(@"4111111111111111");

  EXPECT_NSEQ(number, @"4111111111111111");
}

// Tests substituting inconvertible characters with digits.
TEST_F(CreditCardScannerStringUtilTest,
       TestSubstitutingFalseCharactersWithDigits) {
  NSString* cardNumber =
      SubstituteSimilarCharactersInRecognizedText(@"abcdefghi");

  EXPECT_NSEQ(cardNumber, @"A800EF9H1");
}
