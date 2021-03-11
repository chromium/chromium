// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_string_util.h"

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(NetStringUtilTest, ToUpperEmpty) {
  base::string16 in;
  base::string16 out;
  base::string16 expected;
  ASSERT_TRUE(ToUpper(in, &out));
  ASSERT_EQ(expected, out);
}

TEST(NetStringUtilTest, ToUpperSingleChar) {
  base::string16 in(base::WideToUTF16(L"a"));
  base::string16 out;
  base::string16 expected(base::WideToUTF16(L"A"));
  ASSERT_TRUE(ToUpper(in, &out));
  ASSERT_EQ(expected, out);
}

TEST(NetStringUtilTest, ToUpperSimple) {
  base::string16 in(base::WideToUTF16(L"hello world"));
  base::string16 out;
  base::string16 expected(base::WideToUTF16(L"HELLO WORLD"));
  ASSERT_TRUE(ToUpper(in, &out));
  ASSERT_EQ(expected, out);
}

TEST(NetStringUtilTest, ToUpperAlreadyUpper) {
  base::string16 in(base::WideToUTF16(L"HELLO WORLD"));
  base::string16 out;
  base::string16 expected(base::WideToUTF16(L"HELLO WORLD"));
  ASSERT_TRUE(ToUpper(in, &out));
  ASSERT_EQ(expected, out);
}

}  // namespace net
