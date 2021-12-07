// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/types.h"
#include "media/formats/hls/parse_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace hls {

TEST(HlsFormatParserTest, ParseDecimalIntegerTest) {
  auto const error_test = [](base::StringPiece input) {
    auto result =
        types::ParseDecimalInteger(SourceString::CreateForTesting(1, 1, input));
    EXPECT_TRUE(result.has_error());
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(), ParseStatusCode::kFailedToParseDecimalInteger);
  };

  auto const ok_test = [](base::StringPiece input,
                          types::DecimalInteger expected) {
    auto result =
        types::ParseDecimalInteger(SourceString::CreateForTesting(1, 1, input));
    EXPECT_TRUE(result.has_value());
    auto value = std::move(result).value();
    EXPECT_EQ(value, expected);
  };

  // Empty string is not allowed
  error_test("");

  // Decimal-integers may not be quoted
  error_test("'90132409'");
  error_test("\"12309234\"");

  // Decimal-integers may not be negative
  error_test("-81234");

  // Decimal-integers may not contain junk or leading/trailing spaces
  error_test("12.352334");
  error_test("  12352334");
  error_test("2352334   ");
  error_test("235.2334");
  error_test("+2352334");
  error_test("235x2334");

  // Decimal-integers may not exceed 20 characters
  error_test("000000000000000000001");

  // Test some valid inputs
  ok_test("00000000000000000001", 1);
  ok_test("0", 0);
  ok_test("1", 1);
  ok_test("2334509345", 2334509345);

  // Test max supported value
  ok_test("18446744073709551615", 18446744073709551615u);
  error_test("18446744073709551616");
}

TEST(HlsFormatParserTest, ParseDecimalFloatingPointTest) {
  auto const error_test = [](base::StringPiece input) {
    auto result = types::ParseDecimalFloatingPoint(
        SourceString::CreateForTesting(1, 1, input));
    EXPECT_TRUE(result.has_error());
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(),
              ParseStatusCode::kFailedToParseDecimalFloatingPoint);
  };

  auto const ok_test = [](base::StringPiece input,
                          types::DecimalFloatingPoint expected) {
    auto result = types::ParseDecimalFloatingPoint(
        SourceString::CreateForTesting(1, 1, input));
    EXPECT_TRUE(result.has_value());
    auto value = std::move(result).value();
    EXPECT_DOUBLE_EQ(value, expected);
  };

  // Empty string is not allowed
  error_test("");

  // Decimal-floating-point may not be quoted
  error_test("'901.32409'");
  error_test("\"123092.34\"");

  // Decimal-floating-point may not be negative */
  error_test("-812.34");

  // Decimal-floating-point may not contain junk or leading/trailing spaces
  error_test("+12352334");
  error_test("  123.45");
  error_test("123.45   ");
  error_test("235x2334");
  error_test("+2352334");

  // Test some valid inputs
  ok_test("0", 0.0);
  ok_test("00.00", 0.0);
  ok_test("42", 42.0);
  ok_test("42.0", 42.0);
  ok_test("42.", 42.0);
  ok_test("0.75", 0.75);
  ok_test(".75", 0.75);
  ok_test("12312309123.908908234", 12312309123.908908234);
  ok_test("0000000.000001", 0.000001);
}

TEST(HlsFormatParserTest, ParseSignedDecimalFloatingPointTest) {
  auto const error_test = [](base::StringPiece input) {
    auto result = types::ParseSignedDecimalFloatingPoint(
        SourceString::CreateForTesting(1, 1, input));
    EXPECT_TRUE(result.has_error());
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(),
              ParseStatusCode::kFailedToParseSignedDecimalFloatingPoint);
  };

  auto const ok_test = [](base::StringPiece input,
                          types::SignedDecimalFloatingPoint expected) {
    auto result = types::ParseSignedDecimalFloatingPoint(
        SourceString::CreateForTesting(1, 1, input));
    EXPECT_TRUE(result.has_value());
    auto value = std::move(result).value();
    EXPECT_DOUBLE_EQ(value, expected);
  };

  // Empty string is not allowed
  error_test("");

  // Signed-decimal-floating-point may not be quoted
  error_test("'901.32409'");
  error_test("\"123092.34\"");

  // Signed-decimal-floating-point may not contain junk or leading/trailing
  // spaces
  error_test("+12352334");
  error_test("  123.45");
  error_test("123.45   ");
  error_test("235x2334");
  error_test("+2352334");

  // Test some valid inputs
  ok_test("0", 0.0);
  ok_test("00.00", 0.0);
  ok_test("42", 42.0);
  ok_test("-42", -42.0);
  ok_test("42.0", 42.0);
  ok_test("75.", 75.0);
  ok_test("0.75", 0.75);
  ok_test("-0.75", -0.75);
  ok_test("-.75", -0.75);
  ok_test(".75", 0.75);
  ok_test("-75.", -75.0);
  ok_test("12312309123.908908234", 12312309123.908908234);
  ok_test("0000000.000001", 0.000001);
}

}  // namespace hls
}  // namespace media
