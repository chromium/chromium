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
  CSSPropertyRef ref("--x", GetDocument());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(CSSPropertyID::kVariable, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, LookupRegistered) {
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "42px",
                                     false);
  CSSPropertyRef ref("--x", GetDocument());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(CSSPropertyID::kVariable, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, LookupStandard) {
  CSSPropertyRef ref("font-size", GetDocument());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(CSSPropertyID::kFontSize, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, IsValid) {
  CSSPropertyRef ref("nosuchproperty", GetDocument());
  EXPECT_FALSE(ref.IsValid());
}

TEST_F(CSSPropertyRefTest, FromCustomProperty) {
  CustomProperty custom(AtomicString("--x"), GetDocument());
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
  CSSPropertyRef ref("font-size", GetDocument());
  EXPECT_TRUE(ref.GetUnresolvedProperty().IsResolvedProperty());
}

TEST_F(CSSPropertyRefTest, GetUnresolvedPropertyCustom) {
  CSSPropertyRef ref("--x", GetDocument());
  EXPECT_TRUE(ref.GetUnresolvedProperty().IsResolvedProperty());
}

TEST_F(CSSPropertyRefTest, GetUnresolvedPropertyAlias) {
  // -webkit-transform is an arbitrarily chosen alias.
  CSSPropertyRef ref("-webkit-transform", GetDocument());
  const auto& unresolved = ref.GetUnresolvedProperty();
  EXPECT_FALSE(unresolved.IsResolvedProperty());
  EXPECT_EQ("-webkit-transform", unresolved.GetPropertyNameString());
}

TEST_F(CSSPropertyRefTest, GetResolvedPropertyAlias) {
  // -webkit-transform is an arbitrarily chosen alias.
  CSSPropertyRef ref("-webkit-transform", GetDocument());
  EXPECT_TRUE(ref.GetProperty().IsResolvedProperty());
  EXPECT_EQ("transform", ref.GetProperty().GetPropertyNameString());
}

TEST_F(CSSPropertyRefTest, FromCSSPropertyNameCustom) {
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "42px",
                                     false);
  CSSPropertyRef ref(CSSPropertyName(AtomicString("--x")), GetDocument());
  EXPECT_EQ(CSSPropertyID::kVariable, ref.GetProperty().PropertyID());
}

TEST_F(CSSPropertyRefTest, FromCSSPropertyNameStandard) {
  CSSPropertyRef ref(CSSPropertyName(CSSPropertyID::kFontSize), GetDocument());
  EXPECT_EQ(CSSPropertyID::kFontSize, ref.GetProperty().PropertyID());
}

}  // namespace blink
