// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/utf8.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink::unicode {

TEST(Utf8Test, ConvertUtf8ToUtf16) {
  // A newly created buffer is legal.
  std::vector<uint8_t> legal_str = {'a', 'b', 'c'};
  UChar result_buffer[3];
  auto result = ConvertUtf8ToUtf16(legal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(3u, result.converted.size());
  EXPECT_EQ(std::vector<UChar>({'a', 'b', 'c'}), result.converted);

  // All subsequent chars in a sequence must be >= 0x80.
  std::vector<uint8_t> illegal_str = {0xF4, 0x66, 0x88, 0x8C};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // A subsequent char after F4 must be <= 0x8F.
  illegal_str = {0xF4, 0x90, 0x88, 0x8C};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // All subsequent chars in a sequence must be >= 0x80.
  illegal_str = {0xE1, 0x11, 0x88};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // A subsequent char after 0xED must be <= 0x9F.
  illegal_str = {0xED, 0xA0, 0x88};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // A length 1 unicode character >= 128 that looks like
  // a multibyte sequence should return not enough source.
  illegal_str = {0b11110001};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);

  // A length 1 unicode character >= 128 but less than the first multibyte value
  // should be illegal.
  illegal_str = {0b10111111};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Valid 2-byte sequence.
  legal_str = {0xC2, 0xA2};  // ¢
  result = ConvertUtf8ToUtf16(legal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(std::vector<UChar>({0x00A2}), result.converted);

  // Valid 3-byte sequence.
  legal_str = {0xE2, 0x82, 0xAC};  // €
  result = ConvertUtf8ToUtf16(legal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(std::vector<UChar>({0x20AC}), result.converted);

  // Valid 4-byte sequence.
  legal_str = {0xF0, 0x9F, 0x98, 0x81};  // 😁
  result = ConvertUtf8ToUtf16(legal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(std::vector<UChar>({0xD83D, 0xDE01}), result.converted);

  // Invalid 2-byte sequence (second byte missing).
  illegal_str = {0xC2};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);

  // Invalid 2-byte sequence (second byte has wrong format).
  illegal_str = {0xC2, 0x20};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Invalid 3-byte sequence (second byte missing).
  illegal_str = {0xE2, 0x82};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);

  // Invalid 3-byte sequence (third byte missing).
  illegal_str = {0xE2};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);

  // Invalid 3-byte sequence (second byte has wrong format).
  illegal_str = {0xE2, 0x20, 0xAC};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Invalid 3-byte sequence (third byte has wrong format).
  illegal_str = {0xE2, 0x82, 0x20};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Invalid 4-byte sequence (second byte missing).
  illegal_str = {0xF0, 0x9F, 0x98};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);

  // Invalid 4-byte sequence (third byte missing).
  illegal_str = {0xF0, 0x9F};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);

  // Invalid 4-byte sequence (fourth byte missing).
  illegal_str = {0xF0};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);

  // Invalid 4-byte sequence (second byte has wrong format).
  illegal_str = {0xF0, 0x20, 0x98, 0x81};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Invalid 4-byte sequence (third byte has wrong format).
  illegal_str = {0xF0, 0x9F, 0x20, 0x81};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Invalid 4-byte sequence (fourth byte has wrong format).
  illegal_str = {0xF0, 0x9F, 0x98, 0x20};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Overlong encoding of a character (2 bytes for a 1-byte char).
  illegal_str = {0xC0, 0x81};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Overlong encoding of a character (3 bytes for a 1-byte char).
  illegal_str = {0xE0, 0x80, 0x81};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Overlong encoding of a character (4 bytes for a 1-byte char).
  illegal_str = {0xF0, 0x80, 0x80, 0x81};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Overlong encoding of a character (3 bytes for a 2-byte char).
  illegal_str = {0xE0, 0x82, 0xA2};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Overlong encoding of a character (4 bytes for a 2-byte char).
  illegal_str = {0xF0, 0x80, 0x82, 0xA2};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Overlong encoding of a character (4 bytes for a 3-byte char).
  illegal_str = {0xF0, 0x82, 0x82, 0xAC};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Invalid byte 0xC0.
  illegal_str = {0xC0};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);

  // Invalid byte 0xC1.
  illegal_str = {0xC1};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);

  // Invalid bytes 0xF5 and above.
  illegal_str = {0xF5, 0x81, 0x81, 0x81};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);
  illegal_str = {0xFF, 0x81, 0x81, 0x81};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // An unpaired high surrogate should fail.
  illegal_str = {0xED, 0xA0, 0x80, 0x01};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // An unpaired low surrogate should fail.
  illegal_str = {0xED, 0xB0, 0x80, 0x01};
  result = ConvertUtf8ToUtf16(illegal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Target buffer too small.
  std::vector<uint8_t> long_input('a', 20);
  result = ConvertUtf8ToUtf16(long_input, result_buffer);
  EXPECT_EQ(blink::unicode::kTargetExhausted, result.status);
  EXPECT_EQ(3u, result.converted.size());
}

TEST(Utf8Test, ConvertLatin1ToUtf8) {
  // Valid Latin-1 string.
  std::vector<LChar> legal_str = {'a', 'b', 'c', 0xA2, 0xAC};
  std::vector<uint8_t> result_buffer(10);
  auto result = ConvertLatin1ToUtf8(legal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(7u, result.converted.size());
  const std::vector<uint8_t> expected1 = {'a',  'b',  'c', 0xC2,
                                          0xA2, 0xC2, 0xAC};
  EXPECT_EQ(expected1, result.converted);

  // Target buffer too small.
  std::vector<uint8_t> small_buffer(6);
  result = ConvertLatin1ToUtf8(legal_str, small_buffer);
  EXPECT_EQ(blink::unicode::kTargetExhausted, result.status);
  EXPECT_EQ(5u, result.converted.size());

  // High-bit characters.
  std::vector<LChar> high_bit_str = {0xFF, 0xFE, 0xFD};
  std::vector<uint8_t> high_bit_buffer(6);
  result = ConvertLatin1ToUtf8(high_bit_str, high_bit_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(6u, result.converted.size());
  const std::vector<uint8_t> expected3 = {0xC3, 0xBF, 0xC3, 0xBE, 0xC3, 0xBD};
  EXPECT_EQ(expected3, result.converted);

  // Empty string.
  result = ConvertLatin1ToUtf8(std::vector<LChar>(), result_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(std::vector<uint8_t>(), result.converted);
}

TEST(Utf8Test, CalculateStringLengthFromUtf8) {
  bool seen_non_ascii;
  bool seen_non_latin1;

  // Empty string
  EXPECT_EQ(0u,
            CalculateStringLengthFromUtf8({}, seen_non_ascii, seen_non_latin1));
  EXPECT_FALSE(seen_non_ascii);
  EXPECT_FALSE(seen_non_latin1);

  // All ASCII
  std::vector<uint8_t> ascii_str = {'a', 'b', 'c'};
  EXPECT_EQ(3u, CalculateStringLengthFromUtf8(ascii_str, seen_non_ascii,
                                              seen_non_latin1));
  EXPECT_FALSE(seen_non_ascii);
  EXPECT_FALSE(seen_non_latin1);

  // Latin-1
  std::vector<uint8_t> latin1_str = {'a', 'b', 0xC2, 0xA2};
  EXPECT_EQ(3u, CalculateStringLengthFromUtf8(latin1_str, seen_non_ascii,
                                              seen_non_latin1));
  EXPECT_TRUE(seen_non_ascii);
  EXPECT_FALSE(seen_non_latin1);

  // Non-Latin-1 UTF-8
  std::vector<uint8_t> utf8_str = {0xE2, 0x82, 0xAC};  // €
  EXPECT_EQ(1u, CalculateStringLengthFromUtf8(utf8_str, seen_non_ascii,
                                              seen_non_latin1));
  EXPECT_TRUE(seen_non_ascii);
  EXPECT_TRUE(seen_non_latin1);

  // Mixed
  std::vector<uint8_t> mixed_str = {'a', 0xC2, 0xA2, 0xE2, 0x82, 0xAC};
  EXPECT_EQ(3u, CalculateStringLengthFromUtf8(mixed_str, seen_non_ascii,
                                              seen_non_latin1));
  EXPECT_TRUE(seen_non_ascii);
  EXPECT_TRUE(seen_non_latin1);

  // Surrogate pair
  std::vector<uint8_t> surrogate_pair = {0xF0, 0x9F, 0x98, 0x81};
  EXPECT_EQ(2u, CalculateStringLengthFromUtf8(surrogate_pair, seen_non_ascii,
                                              seen_non_latin1));
  EXPECT_TRUE(seen_non_ascii);
  EXPECT_TRUE(seen_non_latin1);

  // Invalid UTF-8
  std::vector<uint8_t> invalid_str = {0xC2};
  EXPECT_EQ(0u, CalculateStringLengthFromUtf8(invalid_str, seen_non_ascii,
                                              seen_non_latin1));

  // Truncated UTF-8
  std::vector<uint8_t> truncated_str = {0xE2, 0x82};
  EXPECT_EQ(0u, CalculateStringLengthFromUtf8(truncated_str, seen_non_ascii,
                                              seen_non_latin1));
}

TEST(Utf8Test, ConvertUtf16ToUtf8) {
  // Empty string
  std::vector<UChar> empty_str = {};
  std::vector<uint8_t> empty_buffer(1);
  auto result = ConvertUtf16ToUtf8(empty_str, empty_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(0u, result.converted.size());

  // All ASCII
  std::vector<UChar> ascii_str = {'a', 'b', 'c'};
  std::vector<uint8_t> ascii_buffer(3);
  result = ConvertUtf16ToUtf8(ascii_str, ascii_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(std::vector<uint8_t>({'a', 'b', 'c'}), result.converted);

  // BMP characters
  std::vector<UChar> bmp_str = {0x00A2, 0x20AC};  // ¢€
  std::vector<uint8_t> bmp_buffer(5);
  result = ConvertUtf16ToUtf8(bmp_str, bmp_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(std::vector<uint8_t>({0xC2, 0xA2, 0xE2, 0x82, 0xAC}),
            result.converted);

  // Supplementary plane characters (surrogate pairs)
  std::vector<UChar> supplementary_str = {0xD83D, 0xDE01};  // 😁
  std::vector<uint8_t> supplementary_buffer(4);
  result = ConvertUtf16ToUtf8(supplementary_str, supplementary_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(std::vector<uint8_t>({0xF0, 0x9F, 0x98, 0x81}), result.converted);

  // Mixed
  std::vector<UChar> mixed_str = {'a', 0x00A2, 0xD83D, 0xDE01};
  std::vector<uint8_t> mixed_buffer(7);
  result = ConvertUtf16ToUtf8(mixed_str, mixed_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(std::vector<uint8_t>({'a', 0xC2, 0xA2, 0xF0, 0x9F, 0x98, 0x81}),
            result.converted);

  // Target buffer too small
  std::vector<uint8_t> small_buffer(6);
  result = ConvertUtf16ToUtf8(mixed_str, small_buffer);
  EXPECT_EQ(blink::unicode::kTargetExhausted, result.status);
  EXPECT_EQ(std::vector<uint8_t>({'a', 0xC2, 0xA2}), result.converted);

  // Invalid surrogate pair (strict)
  std::vector<UChar> invalid_str = {0xD83D, 'a'};
  std::vector<uint8_t> invalid_buffer(4);
  result = ConvertUtf16ToUtf8(invalid_str, invalid_buffer, true);
  EXPECT_EQ(blink::unicode::kSourceIllegal, result.status);

  // Invalid surrogate pair (non-strict)
  result = ConvertUtf16ToUtf8(invalid_str, invalid_buffer, false);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);
  EXPECT_EQ(4u, result.converted.size());
  EXPECT_EQ(std::vector<uint8_t>({0xED, 0xA0, 0xBD, 'a'}), result.converted);

  // Lone high surrogate at the end of the input.
  std::vector<UChar> lone_surrogate_str = {0xD83D};
  result = ConvertUtf16ToUtf8(lone_surrogate_str, invalid_buffer, true);
  EXPECT_EQ(blink::unicode::kSourceExhausted, result.status);
}

}  // namespace blink::unicode
