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

  // Valid 3-byte sequence.
  legal_str = {0xE2, 0x82, 0xAC};  // €
  result = ConvertUtf8ToUtf16(legal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);

  // Valid 4-byte sequence.
  legal_str = {0xF0, 0x9F, 0x98, 0x81};  // 😁
  result = ConvertUtf8ToUtf16(legal_str, result_buffer);
  EXPECT_EQ(blink::unicode::kConversionOK, result.status);

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
}

}  // namespace blink::unicode
