// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_string_util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(NetStringUtilTest, ToUpperEmpty) {
  std::u16string in;
  std::u16string out;
  std::u16string expected;
  ASSERT_TRUE(ToUpperUsingLocale(in, &out));
  ASSERT_EQ(expected, out);
}

TEST(NetStringUtilTest, ToUpperSingleChar) {
  std::u16string in(u"a");
  std::u16string out;
  std::u16string expected(u"A");
  ASSERT_TRUE(ToUpperUsingLocale(in, &out));
  ASSERT_EQ(expected, out);
}

TEST(NetStringUtilTest, ToUpperSimple) {
  std::u16string in(u"hello world");
  std::u16string out;
  std::u16string expected(u"HELLO WORLD");
  ASSERT_TRUE(ToUpperUsingLocale(in, &out));
  ASSERT_EQ(expected, out);
}

TEST(NetStringUtilTest, ToUpperAlreadyUpper) {
  std::u16string in(u"HELLO WORLD");
  std::u16string out;
  std::u16string expected(u"HELLO WORLD");
  ASSERT_TRUE(ToUpperUsingLocale(in, &out));
  ASSERT_EQ(expected, out);
}

}  // namespace net
