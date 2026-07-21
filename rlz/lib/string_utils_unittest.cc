// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test for string manipulation functions used in the RLZ library.

#include "rlz/lib/string_utils.h"

#include <stddef.h>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "rlz/lib/assert.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(StringUtilsUnittest, IsAscii) {
  rlz_lib::SetExpectedAssertion("");

  char bad_letters[] = {'\x80', '\xA0', '\xFF'};
  for (char letter : bad_letters) {
    EXPECT_FALSE(rlz_lib::IsAscii(letter));
  }

  char good_letters[] = {'A', '~', '\n', 0x7F, 0x00};
  for (char letter : good_letters) {
    EXPECT_TRUE(rlz_lib::IsAscii(letter));
  }
}



TEST(StringUtilsUnittest, TestBytesToString) {
  unsigned char data[] = {0x1E, 0x00, 0x21, 0x67, 0xFF};
  std::string result;

  EXPECT_FALSE(rlz_lib::BytesToString(base::span<const uint8_t>(), &result));
  EXPECT_FALSE(rlz_lib::BytesToString(data, NULL));
  EXPECT_FALSE(rlz_lib::BytesToString(base::span<const uint8_t>(), NULL));

  EXPECT_TRUE(rlz_lib::BytesToString(data, &result));
  EXPECT_EQ(std::string("1E002167FF"), result);
  EXPECT_TRUE(rlz_lib::BytesToString(base::span(data).first<4>(), &result));
  EXPECT_EQ(std::string("1E002167"), result);
  EXPECT_TRUE(rlz_lib::BytesToString(base::span(data).first(4u), &result));
  EXPECT_EQ(std::string("1E002167"), result);
}
