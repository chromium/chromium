// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_name_or_keyword.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(StyleNameOrKeywordTest, StyleName) {
  StyleName name_custom_ident(AtomicString("foo"),
                              StyleName::Type::kCustomIdent);
  StyleName name_string(AtomicString("foo"), StyleName::Type::kString);

  EXPECT_FALSE(StyleNameOrKeyword(name_custom_ident).IsKeyword());
  EXPECT_FALSE(StyleNameOrKeyword(name_string).IsKeyword());

  EXPECT_EQ(name_custom_ident, StyleNameOrKeyword(name_custom_ident).GetName());
  EXPECT_EQ(name_string, StyleNameOrKeyword(name_string).GetName());
}

TEST(StyleNameOrKeywordTest, Keyword) {
  EXPECT_TRUE(StyleNameOrKeyword(CSSValueID::kAuto).IsKeyword());
  EXPECT_TRUE(StyleNameOrKeyword(CSSValueID::kNone).IsKeyword());

  EXPECT_EQ(CSSValueID::kAuto,
            StyleNameOrKeyword(CSSValueID::kAuto).GetKeyword());
  EXPECT_EQ(CSSValueID::kNone,
            StyleNameOrKeyword(CSSValueID::kNone).GetKeyword());
}

TEST(StyleNameOrKeywordTest, Equality) {
  StyleName name_custom_ident(AtomicString("foo"),
                              StyleName::Type::kCustomIdent);
  StyleName name_string(AtomicString("foo"), StyleName::Type::kString);

  EXPECT_EQ(StyleNameOrKeyword(CSSValueID::kAuto),
            StyleNameOrKeyword(CSSValueID::kAuto));
  EXPECT_EQ(StyleNameOrKeyword(name_string), StyleNameOrKeyword(name_string));
  EXPECT_EQ(StyleNameOrKeyword(name_custom_ident),
            StyleNameOrKeyword(name_custom_ident));
  EXPECT_NE(StyleNameOrKeyword(name_custom_ident),
            StyleNameOrKeyword(name_string));
  EXPECT_NE(StyleNameOrKeyword(CSSValueID::kAuto),
            StyleNameOrKeyword(CSSValueID::kNone));
}

}  // namespace blink
