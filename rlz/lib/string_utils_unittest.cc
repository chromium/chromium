// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test for string manipulation functions used in the RLZ library.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "rlz/lib/string_utils.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "rlz/lib/assert.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(StringUtilsUnittest, IsAscii) {
  rlz_lib::SetExpectedAssertion("");

  char bad_letters[] = {'\x80', '\xA0', '\xFF'};
  for (size_t i = 0; i < std::size(bad_letters); ++i)
    EXPECT_FALSE(rlz_lib::IsAscii(bad_letters[i]));

  char good_letters[] = {'A', '~', '\n', 0x7F, 0x00};
  for (size_t i = 0; i < std::size(good_letters); ++i)
    EXPECT_TRUE(rlz_lib::IsAscii(good_letters[i]));
}

TEST(StringUtilsUnittest, HexStringToInteger) {
  rlz_lib::SetExpectedAssertion("HexStringToInteger: text is NULL.");
  EXPECT_EQ(0, rlz_lib::HexStringToInteger(NULL));

  rlz_lib::SetExpectedAssertion("");
  EXPECT_EQ(0, rlz_lib::HexStringToInteger(""));
  EXPECT_EQ(0, rlz_lib::HexStringToInteger("   "));
  EXPECT_EQ(0, rlz_lib::HexStringToInteger("  0x  "));
  EXPECT_EQ(0, rlz_lib::HexStringToInteger("  0x0  "));
  EXPECT_EQ(0x12345, rlz_lib::HexStringToInteger("12345"));
  EXPECT_EQ(0xa34Ed0, rlz_lib::HexStringToInteger("a34Ed0"));
  EXPECT_EQ(0xa34Ed0, rlz_lib::HexStringToInteger("0xa34Ed0"));
  EXPECT_EQ(0xa34Ed0, rlz_lib::HexStringToInteger("   0xa34Ed0"));
  EXPECT_EQ(0xa34Ed0, rlz_lib::HexStringToInteger("0xa34Ed0   "));
  EXPECT_EQ(0xa34Ed0, rlz_lib::HexStringToInteger("   0xa34Ed0   "));
  EXPECT_EQ(0xa34Ed0, rlz_lib::HexStringToInteger("   0x000a34Ed0   "));
  EXPECT_EQ(0xa34Ed0, rlz_lib::HexStringToInteger("   000a34Ed0   "));

  rlz_lib::SetExpectedAssertion(
      "HexStringToInteger: text contains non-hex characters.");
  EXPECT_EQ(0x12ff, rlz_lib::HexStringToInteger("12ffg"));
  EXPECT_EQ(0x12f, rlz_lib::HexStringToInteger("12f 121"));
  EXPECT_EQ(0x12f, rlz_lib::HexStringToInteger("12f 121"));
  EXPECT_EQ(0, rlz_lib::HexStringToInteger("g12f"));
  EXPECT_EQ(0, rlz_lib::HexStringToInteger("  0x0  \n"));

  rlz_lib::SetExpectedAssertion("");
}

TEST(StringUtilsUnittest, TestBytesToString) {
  unsigned char data[] = {0x1E, 0x00, 0x21, 0x67, 0xFF};
  std::string result;

  EXPECT_FALSE(rlz_lib::BytesToString(NULL, 5, &result));
  EXPECT_FALSE(rlz_lib::BytesToString(data, 5, NULL));
  EXPECT_FALSE(rlz_lib::BytesToString(NULL, 5, NULL));

  EXPECT_TRUE(rlz_lib::BytesToString(data, 5, &result));
  EXPECT_EQ(std::string("1E002167FF"), result);
  EXPECT_TRUE(rlz_lib::BytesToString(data, 4, &result));
  EXPECT_EQ(std::string("1E002167"), result);
}
