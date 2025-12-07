// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

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

TEST(StrCatTest, MixedBitsResulting8it) {
  const String src16(u"a");
  ASSERT_FALSE(src16.Is8Bit());
  String result = StrCat({"foo \"", src16, "\" bar."});
  EXPECT_EQ("foo \"a\" bar.", result);
  EXPECT_TRUE(result.Is8Bit());
}

TEST(StrCatTest, StringSelfSubstitution) {
  String foo("abc");
  StringView view(" after");

  // We had an issue that the following code caused a DCHECK failure in
  // ~StringView() because a StringView created for `foo` outlives the
  // initial StringImpl of `foo`.
  foo = StrCat({"before ", foo, view});

  EXPECT_EQ("before abc after", foo);
  EXPECT_EQ(" after", view);
}

}  // namespace blink
