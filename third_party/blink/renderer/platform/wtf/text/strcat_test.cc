// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(StrCatTest, Only8Bit) {
  const String src8("8bit 8bit");
  String result = StrCat({"foo \"", src8, "\" bar."});
  EXPECT_EQ("foo \"8bit 8bit\" bar.", result);
  EXPECT_TRUE(result.Is8Bit());
}

TEST(StrCatTest, Only16Bit) {
  const String src16_1(u"foo \"");
  const String src16_2(u"16bit 16bit");
  const String src16_3(u"\" bar.");
  String result = StrCat({src16_1, src16_2, src16_3});
  EXPECT_EQ("foo \"16bit 16bit\" bar.", result);
  EXPECT_FALSE(result.Is8Bit());
}

TEST(StrCatTest, MixedBits) {
  const String src16(u"16bit 16bit");
  String result = StrCat({"foo \"", src16, "\" bar."});
  EXPECT_EQ("foo \"16bit 16bit\" bar.", result);
  EXPECT_FALSE(result.Is8Bit());
}

}  // namespace WTF
