// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/dtoa.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

TEST(DtoaTest, ToFixedPrecisionString) {
  DoubleToStringConverter converter;
  auto serialize = [&converter](double value,
                                unsigned precision) -> StringView {
    return StringView(converter.ToStringWithFixedPrecision(value, precision));
  };

  // There should be no trailing decimal or zeros.
  EXPECT_EQ("0", serialize(0.0, 6));

  // Up to 6 leading zeros.
  EXPECT_EQ("0.00000123123", serialize(0.00000123123123, 6));

  EXPECT_EQ("1.23123e-7", serialize(0.000000123123123, 6));

  // Up to 6 places before the decimal.
  EXPECT_EQ("123123", serialize(123123.123, 6));

  EXPECT_EQ("1.23123e+6", serialize(1231231.23, 6));

  // Don't strip trailing zeros in exponents.
  // http://crbug.com/545711
  EXPECT_EQ("1.23123e-10", serialize(0.000000000123123, 6));

  // FIXME: Trailing zeros before exponents should be stripped.
  EXPECT_EQ("1.00000e-10", serialize(0.0000000001, 6));
}

}  // namespace blink
