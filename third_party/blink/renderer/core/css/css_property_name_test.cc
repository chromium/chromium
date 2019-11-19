// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class CSSPropertyNameTest : public testing::Test {
 public:
  CSSPropertyName Empty() const {
    return CSSPropertyName(CSSPropertyName::kEmptyValue);
  }

  CSSPropertyName Deleted() const {
    return CSSPropertyName(CSSPropertyName::kDeletedValue);
  }

  bool IsDeleted(const CSSPropertyName& name) const {
    return name.IsDeletedValue();
  }

  bool IsEmpty(const CSSPropertyName& name) const {
    return name.IsEmptyValue();
  }

  unsigned GetHash(const CSSPropertyName& name) const { return name.GetHash(); }
};

TEST_F(CSSPropertyNameTest, IdStandardProperty) {
  CSSPropertyName name(CSSPropertyID::kFontSize);
  EXPECT_EQ(CSSPropertyID::kFontSize, name.Id());
}

TEST_F(CSSPropertyNameTest, IdCustomProperty) {
  CSSPropertyName name(AtomicString("--x"));
  EXPECT_EQ(CSSPropertyID::kVariable, name.Id());
  EXPECT_TRUE(name.IsCustomProperty());
}

TEST_F(CSSPropertyNameTest, GetNameStandardProperty) {
  CSSPropertyName name(CSSPropertyID::kFontSize);
  EXPECT_EQ(AtomicString("font-size"), name.ToAtomicString());
}

TEST_F(CSSPropertyNameTest, GetNameCustomProperty) {
  CSSPropertyName name(AtomicString("--x"));
  EXPECT_EQ(AtomicString("--x"), name.ToAtomicString());
}

TEST_F(CSSPropertyNameTest, OperatorEquals) {
  EXPECT_EQ(CSSPropertyName("--x"), CSSPropertyName("--x"));
  EXPECT_EQ(CSSPropertyName(CSSPropertyID::kColor),
            CSSPropertyName(CSSPropertyID::kColor));
  EXPECT_NE(CSSPropertyName("--x"), CSSPropertyName("--y"));
  EXPECT_NE(CSSPropertyName(CSSPropertyID::kColor),
            CSSPropertyName(CSSPropertyID::kBackgroundColor));
}

TEST_F(CSSPropertyNameTest, From) {
  EXPECT_TRUE(CSSPropertyName::From("color"));
  EXPECT_TRUE(CSSPropertyName::From("--x"));
  EXPECT_FALSE(CSSPropertyName::From("notaproperty"));
  EXPECT_FALSE(CSSPropertyName::From("-not-a-property"));

  EXPECT_EQ(*CSSPropertyName::From("color"),
            CSSPropertyName(CSSPropertyID::kColor));
  EXPECT_EQ(*CSSPropertyName::From("--x"), CSSPropertyName("--x"));
}

TEST_F(CSSPropertyNameTest, FromNativeCSSProperty) {
  CSSPropertyName name = GetCSSPropertyFontSize().GetCSSPropertyName();
  EXPECT_EQ(CSSPropertyName(CSSPropertyID::kFontSize), name);
}

TEST_F(CSSPropertyNameTest, IsEmptyValue) {
  CSSPropertyName empty = Empty();
  CSSPropertyName deleted = Deleted();
  CSSPropertyName normal = GetCSSPropertyFontSize().GetCSSPropertyName();
  CSSPropertyName custom("--x");

  EXPECT_TRUE(IsEmpty(empty));
  EXPECT_FALSE(IsEmpty(deleted));
  EXPECT_FALSE(IsEmpty(normal));
  EXPECT_FALSE(IsEmpty(custom));
}

TEST_F(CSSPropertyNameTest, IsDeletedValue) {
  CSSPropertyName empty = Empty();
  CSSPropertyName deleted = Deleted();
  CSSPropertyName normal = GetCSSPropertyFontSize().GetCSSPropertyName();
  CSSPropertyName custom("--x");

  EXPECT_FALSE(IsDeleted(empty));
  EXPECT_TRUE(IsDeleted(deleted));
  EXPECT_FALSE(IsDeleted(normal));
  EXPECT_FALSE(IsDeleted(custom));
}

TEST_F(CSSPropertyNameTest, GetHash) {
  CSSPropertyName normal = GetCSSPropertyFontSize().GetCSSPropertyName();
  CSSPropertyName custom("--x");

  // Don't crash.
  GetHash(normal);
  GetHash(custom);
}

TEST_F(CSSPropertyNameTest, CompareEmptyDeleted) {
  CSSPropertyName normal = GetCSSPropertyFontSize().GetCSSPropertyName();
  CSSPropertyName custom("--x");

  EXPECT_EQ(Empty(), Empty());
  EXPECT_EQ(Deleted(), Deleted());

  EXPECT_NE(Empty(), Deleted());
  EXPECT_NE(Deleted(), Empty());

  EXPECT_NE(Empty(), normal);
  EXPECT_NE(Empty(), custom);
  EXPECT_NE(Deleted(), normal);
  EXPECT_NE(Deleted(), custom);

  EXPECT_NE(normal, Empty());
  EXPECT_NE(custom, Empty());
  EXPECT_NE(normal, Deleted());
  EXPECT_NE(custom, Deleted());
}

TEST_F(CSSPropertyNameTest, HashMapBasic) {
  HashMap<CSSPropertyName, AtomicString> map;

  map.Set(CSSPropertyName("--x"), "foo");
  map.Set(CSSPropertyName("--y"), "foo");
  map.Set(CSSPropertyName("--z"), "foo");

  map.Set(CSSPropertyName("--x"), "bar");
  map.erase(CSSPropertyName("--z"));

  EXPECT_EQ("bar", map.Take(CSSPropertyName("--x")));
  EXPECT_EQ("foo", map.Take(CSSPropertyName("--y")));
  EXPECT_EQ(map.end(), map.find(CSSPropertyName("--z")));

  map.Set(GetCSSPropertyFontSize().GetCSSPropertyName(), "foo");
  map.Set(GetCSSPropertyFontSize().GetCSSPropertyName(), "bar");
  EXPECT_EQ("bar", map.Take(GetCSSPropertyFontSize().GetCSSPropertyName()));
}

}  // namespace blink
