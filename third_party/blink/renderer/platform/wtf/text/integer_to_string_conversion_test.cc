// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

TEST(IntegerToStringConversionTest, SimpleIntConversion) {
  const IntegerToStringConverter<int> conv(100500);
  EXPECT_EQ(StringView(conv.Span()), StringView("100500"));
}

template <typename T>
class IntegerToStringConversionBoundsTest : public ::testing::Test {};

using IntegerToStringConversionBoundsTestTypes = ::testing::Types<uint8_t,
                                                                  int8_t,
                                                                  uint16_t,
                                                                  int16_t,
                                                                  uint32_t,
                                                                  int32_t,
                                                                  uint64_t,
                                                                  int64_t>;
TYPED_TEST_SUITE(IntegerToStringConversionBoundsTest,
                 IntegerToStringConversionBoundsTestTypes);

// Test that the minimum value for a given integer type is converted accurately.
TYPED_TEST(IntegerToStringConversionBoundsTest, LowerBound) {
  constexpr auto value = std::numeric_limits<TypeParam>::min();
  const IntegerToStringConverter<TypeParam> conv(value);
  std::string expected = base::NumberToString(value);
  EXPECT_EQ(StringView(expected.c_str()), StringView(conv.Span()));
}

// Test that the maximum value for a given integer type is converted accurately.
TYPED_TEST(IntegerToStringConversionBoundsTest, UpperBound) {
  constexpr auto value = std::numeric_limits<TypeParam>::max();
  const IntegerToStringConverter<TypeParam> conv(value);
  std::string expected = base::NumberToString(value);
  EXPECT_EQ(StringView(expected.c_str()), StringView(conv.Span()));
}

TEST(IntegerToStringConversionTest, HexConversion) {
  const IntegerToStringConverter<int, 16> conv1(255);
  EXPECT_EQ(StringView(conv1.Span()), StringView("ff"));

  const IntegerToStringConverter<int, 16, true> conv2(255);
  EXPECT_EQ(StringView(conv2.Span()), StringView("FF"));

  const IntegerToStringConverter<int, 16, true> conv3(267);
  EXPECT_EQ(StringView(conv3.Span()), StringView("10B"));

  const IntegerToStringConverter<int, 16> conv4(0);
  EXPECT_EQ(StringView(conv4.Span()), StringView("0"));

  const IntegerToStringConverter<uint64_t, 16> conv5(
      UINT64_C(0x123456789ABCDEF0));
  EXPECT_EQ(StringView(conv5.Span()), StringView("123456789abcdef0"));
}

TEST(IntegerToStringConversionTest, HexConversionNegative) {
  const IntegerToStringConverter<int8_t, 16> conv1(-1);
  EXPECT_EQ(StringView(conv1.Span()), StringView("ff"));

  const IntegerToStringConverter<int32_t, 16> conv2(-1);
  EXPECT_EQ(StringView(conv2.Span()), StringView("ffffffff"));

  const IntegerToStringConverter<int64_t, 16> conv3(-1);
  EXPECT_EQ(StringView(conv3.Span()), StringView("ffffffffffffffff"));
}

}  // namespace blink
