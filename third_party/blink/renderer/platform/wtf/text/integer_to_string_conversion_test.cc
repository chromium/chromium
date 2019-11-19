// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace WTF {

TEST(IntegerToStringConversionTest, SimpleIntConversion) {
  const IntegerToStringConverter<int> conv(100500);
  EXPECT_EQ(StringView(conv.Characters8(), conv.length()),
            StringView("100500"));
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
  EXPECT_EQ(StringView(expected.c_str()),
            StringView(conv.Characters8(), conv.length()));
}

// Test that the maximum value for a given integer type is converted accurately.
TYPED_TEST(IntegerToStringConversionBoundsTest, UpperBound) {
  constexpr auto value = std::numeric_limits<TypeParam>::max();
  const IntegerToStringConverter<TypeParam> conv(value);
  std::string expected = base::NumberToString(value);
  EXPECT_EQ(StringView(expected.c_str()),
            StringView(conv.Characters8(), conv.length()));
}

}  // namespace WTF
