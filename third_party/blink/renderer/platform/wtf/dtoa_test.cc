// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/dtoa.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(DtoaTest, TestNumberToFixedPrecisionString) {
  NumberToStringBuffer buffer;

  // There should be no trailing decimal or zeros.
  NumberToFixedPrecisionString(0.0, 6, buffer);
  EXPECT_STREQ("0", buffer);

  // Up to 6 leading zeros.
  NumberToFixedPrecisionString(0.00000123123123, 6, buffer);
  EXPECT_STREQ("0.00000123123", buffer);

  NumberToFixedPrecisionString(0.000000123123123, 6, buffer);
  EXPECT_STREQ("1.23123e-7", buffer);

  // Up to 6 places before the decimal.
  NumberToFixedPrecisionString(123123.123, 6, buffer);
  EXPECT_STREQ("123123", buffer);

  NumberToFixedPrecisionString(1231231.23, 6, buffer);
  EXPECT_STREQ("1.23123e+6", buffer);

  // Don't strip trailing zeros in exponents.
  // http://crbug.com/545711
  NumberToFixedPrecisionString(0.000000000123123, 6, buffer);
  EXPECT_STREQ("1.23123e-10", buffer);

  // FIXME: Trailing zeros before exponents should be stripped.
  NumberToFixedPrecisionString(0.0000000001, 6, buffer);
  EXPECT_STREQ("1.00000e-10", buffer);
}

}  // namespace WTF
