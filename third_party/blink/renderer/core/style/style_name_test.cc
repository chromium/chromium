// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_name.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(StyleNameTest, DefaultConstructor) {
  StyleName name;
  EXPECT_FALSE(name.IsCustomIdent());
  EXPECT_TRUE(name.GetValue().IsNull());
}

TEST(StyleNameTest, Copy) {
  StyleName name_string(AtomicString("foo"), StyleName::Type::kString);
  StyleName name_custom_ident(AtomicString("foo"),
                              StyleName::Type::kCustomIdent);

  StyleName name_string_copy1(name_string);
  StyleName name_custom_ident_copy1(name_custom_ident);

  StyleName name_string_copy2 = name_string;
  StyleName name_custom_ident_copy2 = name_custom_ident;

  EXPECT_EQ(name_string, name_string_copy1);
  EXPECT_EQ(name_string, name_string_copy2);

  EXPECT_EQ(name_custom_ident, name_custom_ident_copy1);
  EXPECT_EQ(name_custom_ident, name_custom_ident_copy2);
}

TEST(StyleNameTest, CustomIdent) {
  StyleName name(AtomicString("foo"), StyleName::Type::kCustomIdent);
  EXPECT_TRUE(name.IsCustomIdent());
  EXPECT_EQ("foo", name.GetValue());
}

TEST(StyleNameTest, String) {
  StyleName name(AtomicString("foo"), StyleName::Type::kString);
  EXPECT_FALSE(name.IsCustomIdent());
  EXPECT_EQ("foo", name.GetValue());
}

TEST(StyleNameTest, Equals) {
  EXPECT_EQ(StyleName(AtomicString("foo"), StyleName::Type::kString),
            StyleName(AtomicString("foo"), StyleName::Type::kString));
  EXPECT_NE(StyleName(AtomicString("foo"), StyleName::Type::kString),
            StyleName(AtomicString("bar"), StyleName::Type::kString));
  EXPECT_NE(StyleName(AtomicString("foo"), StyleName::Type::kString),
            StyleName(AtomicString("foo"), StyleName::Type::kCustomIdent));
}

}  // namespace blink
