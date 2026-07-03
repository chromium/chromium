// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {

class CSSPropertyRefTest : public PageTestBase {};

}  // namespace

TEST_F(CSSPropertyRefTest, LookupUnregistred) {
  AtomicString x_name("--x");
  CSSPropertyRef ref(&x_name, GetDocument());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(CSSPropertyID::kVariable, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, LookupRegistered) {
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "42px",
                                     false);
  AtomicString x_name("--x");
  CSSPropertyRef ref(&x_name, GetDocument());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(CSSPropertyID::kVariable, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, LookupStandard) {
  AtomicString property_name("font-size");
  CSSPropertyRef ref(&property_name, GetDocument());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(CSSPropertyID::kFontSize, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, IsValid) {
  AtomicString property_name("nosuchproperty");
  CSSPropertyRef ref(&property_name, GetDocument());
  EXPECT_FALSE(ref.IsValid());
}

TEST_F(CSSPropertyRefTest, FromCustomProperty) {
  AtomicString x_name("--x");
  CustomProperty custom(&x_name, GetDocument());
  CSSPropertyRef ref(custom);
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(CSSPropertyID::kVariable, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, FromStandardProperty) {
  CSSPropertyRef ref(GetCSSPropertyFontSize());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(CSSPropertyID::kFontSize, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, FromStaticVariableInstance) {
  CSSPropertyRef ref(GetCSSPropertyVariable());
  EXPECT_FALSE(ref.IsValid());
}

TEST_F(CSSPropertyRefTest, GetUnresolvedPropertyStandard) {
  AtomicString property_name("font-size");
  CSSPropertyRef ref(&property_name, GetDocument());
  EXPECT_TRUE(ref.GetUnresolvedProperty().IsResolvedProperty());
}

TEST_F(CSSPropertyRefTest, GetUnresolvedPropertyCustom) {
  AtomicString x_name("--x");
  CSSPropertyRef ref(&x_name, GetDocument());
  EXPECT_TRUE(ref.GetUnresolvedProperty().IsResolvedProperty());
}

TEST_F(CSSPropertyRefTest, GetUnresolvedPropertyAlias) {
  // -webkit-transform is an arbitrarily chosen alias.
  AtomicString property_name("-webkit-transform");
  CSSPropertyRef ref(&property_name, GetDocument());
  const auto& unresolved = ref.GetUnresolvedProperty();
  EXPECT_FALSE(unresolved.IsResolvedProperty());
  EXPECT_EQ("-webkit-transform", unresolved.GetPropertyNameString());
}

TEST_F(CSSPropertyRefTest, GetResolvedPropertyAlias) {
  // -webkit-transform is an arbitrarily chosen alias.
  AtomicString property_name("-webkit-transform");
  CSSPropertyRef ref(&property_name, GetDocument());
  EXPECT_TRUE(ref.GetProperty().IsResolvedProperty());
  EXPECT_EQ("transform", ref.GetProperty().GetPropertyNameString());
}

TEST_F(CSSPropertyRefTest, FromCSSPropertyNameCustom) {
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "42px",
                                     false);
  CSSPropertyName name(AtomicString("--x"));
  CSSPropertyRef ref(&name, GetDocument());
  EXPECT_EQ(CSSPropertyID::kVariable, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, FromCSSPropertyNameStandard) {
  CSSPropertyName name(CSSPropertyID::kFontSize);
  CSSPropertyRef ref(&name, GetDocument());
  EXPECT_EQ(CSSPropertyID::kFontSize, ref.GetProperty().PropertyID());
}

}  // namespace blink
