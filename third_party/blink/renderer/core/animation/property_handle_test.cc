// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/property_handle.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

using svg_names::kAmplitudeAttr;
using svg_names::kExponentAttr;

class PropertyHandleTest : public testing::Test {};

TEST_F(PropertyHandleTest, Equality) {
  AtomicString name_a("--a");
  AtomicString name_b("--b");

  EXPECT_TRUE(PropertyHandle(GetCSSPropertyOpacity()) ==
              PropertyHandle(GetCSSPropertyOpacity()));
  EXPECT_FALSE(PropertyHandle(GetCSSPropertyOpacity()) !=
               PropertyHandle(GetCSSPropertyOpacity()));
  EXPECT_FALSE(PropertyHandle(GetCSSPropertyOpacity()) ==
               PropertyHandle(GetCSSPropertyTransform()));
  EXPECT_TRUE(PropertyHandle(GetCSSPropertyOpacity()) !=
              PropertyHandle(GetCSSPropertyTransform()));
  EXPECT_FALSE(PropertyHandle(GetCSSPropertyOpacity()) ==
               PropertyHandle(name_a));
  EXPECT_TRUE(PropertyHandle(GetCSSPropertyOpacity()) !=
              PropertyHandle(name_a));
  EXPECT_FALSE(PropertyHandle(GetCSSPropertyOpacity()) ==
               PropertyHandle(kAmplitudeAttr));
  EXPECT_TRUE(PropertyHandle(GetCSSPropertyOpacity()) !=
              PropertyHandle(kAmplitudeAttr));

  EXPECT_FALSE(PropertyHandle(name_a) ==
               PropertyHandle(GetCSSPropertyOpacity()));
  EXPECT_TRUE(PropertyHandle(name_a) !=
              PropertyHandle(GetCSSPropertyOpacity()));
  EXPECT_FALSE(PropertyHandle(name_a) ==
               PropertyHandle(GetCSSPropertyTransform()));
  EXPECT_TRUE(PropertyHandle(name_a) !=
              PropertyHandle(GetCSSPropertyTransform()));
  EXPECT_TRUE(PropertyHandle(name_a) == PropertyHandle(name_a));
  EXPECT_FALSE(PropertyHandle(name_a) != PropertyHandle(name_a));
  EXPECT_FALSE(PropertyHandle(name_a) == PropertyHandle(name_b));
  EXPECT_TRUE(PropertyHandle(name_a) != PropertyHandle(name_b));
  EXPECT_FALSE(PropertyHandle(name_a) == PropertyHandle(kAmplitudeAttr));
  EXPECT_TRUE(PropertyHandle(name_a) != PropertyHandle(kAmplitudeAttr));

  EXPECT_FALSE(PropertyHandle(kAmplitudeAttr) ==
               PropertyHandle(GetCSSPropertyOpacity()));
  EXPECT_TRUE(PropertyHandle(kAmplitudeAttr) !=
              PropertyHandle(GetCSSPropertyOpacity()));
  EXPECT_FALSE(PropertyHandle(kAmplitudeAttr) == PropertyHandle(name_a));
  EXPECT_TRUE(PropertyHandle(kAmplitudeAttr) != PropertyHandle(name_a));
  EXPECT_TRUE(PropertyHandle(kAmplitudeAttr) == PropertyHandle(kAmplitudeAttr));
  EXPECT_FALSE(PropertyHandle(kAmplitudeAttr) !=
               PropertyHandle(kAmplitudeAttr));
  EXPECT_FALSE(PropertyHandle(kAmplitudeAttr) == PropertyHandle(kExponentAttr));
  EXPECT_TRUE(PropertyHandle(kAmplitudeAttr) != PropertyHandle(kExponentAttr));
}

TEST_F(PropertyHandleTest, Hash) {
  AtomicString name_a("--a");
  AtomicString name_b("--b");

  EXPECT_TRUE(PropertyHandle(GetCSSPropertyOpacity()).GetHash() ==
              PropertyHandle(GetCSSPropertyOpacity()).GetHash());
  EXPECT_FALSE(PropertyHandle(GetCSSPropertyOpacity()).GetHash() ==
               PropertyHandle(name_a).GetHash());
  EXPECT_FALSE(PropertyHandle(GetCSSPropertyOpacity()).GetHash() ==
               PropertyHandle(GetCSSPropertyTransform()).GetHash());
  EXPECT_FALSE(PropertyHandle(GetCSSPropertyOpacity()).GetHash() ==
               PropertyHandle(kAmplitudeAttr).GetHash());

  EXPECT_FALSE(PropertyHandle(name_a).GetHash() ==
               PropertyHandle(GetCSSPropertyOpacity()).GetHash());
  EXPECT_TRUE(PropertyHandle(name_a).GetHash() ==
              PropertyHandle(name_a).GetHash());
  EXPECT_FALSE(PropertyHandle(name_a).GetHash() ==
               PropertyHandle(name_b).GetHash());
  EXPECT_FALSE(PropertyHandle(name_a).GetHash() ==
               PropertyHandle(kExponentAttr).GetHash());

  EXPECT_FALSE(PropertyHandle(kAmplitudeAttr).GetHash() ==
               PropertyHandle(GetCSSPropertyOpacity()).GetHash());
  EXPECT_FALSE(PropertyHandle(kAmplitudeAttr).GetHash() ==
               PropertyHandle(name_a).GetHash());
  EXPECT_TRUE(PropertyHandle(kAmplitudeAttr).GetHash() ==
              PropertyHandle(kAmplitudeAttr).GetHash());
  EXPECT_FALSE(PropertyHandle(kAmplitudeAttr).GetHash() ==
               PropertyHandle(kExponentAttr).GetHash());
}

TEST_F(PropertyHandleTest, Accessors) {
  AtomicString name("--x");

  EXPECT_TRUE(PropertyHandle(GetCSSPropertyOpacity()).IsCSSProperty());
  EXPECT_TRUE(PropertyHandle(name).IsCSSProperty());
  EXPECT_FALSE(PropertyHandle(kAmplitudeAttr).IsCSSProperty());

  EXPECT_FALSE(PropertyHandle(GetCSSPropertyOpacity()).IsSVGAttribute());
  EXPECT_FALSE(PropertyHandle(name).IsSVGAttribute());
  EXPECT_TRUE(PropertyHandle(kAmplitudeAttr).IsSVGAttribute());

  EXPECT_FALSE(PropertyHandle(GetCSSPropertyOpacity()).IsCSSCustomProperty());
  EXPECT_TRUE(PropertyHandle(name).IsCSSCustomProperty());
  EXPECT_FALSE(PropertyHandle(kAmplitudeAttr).IsCSSCustomProperty());

  EXPECT_EQ(
      CSSPropertyID::kOpacity,
      PropertyHandle(GetCSSPropertyOpacity()).GetCSSProperty().PropertyID());
  EXPECT_EQ(CSSPropertyID::kVariable,
            PropertyHandle(name).GetCSSProperty().PropertyID());
  EXPECT_EQ(name, PropertyHandle(name).CustomPropertyName());
  EXPECT_EQ(kAmplitudeAttr, PropertyHandle(kAmplitudeAttr).SvgAttribute());

  EXPECT_EQ(name, PropertyHandle(name).GetCSSPropertyName().ToAtomicString());
  EXPECT_EQ(CSSPropertyID::kOpacity,
            PropertyHandle(GetCSSPropertyOpacity()).GetCSSPropertyName().Id());
  EXPECT_EQ(
      CSSPropertyID::kColor,
      PropertyHandle(GetCSSPropertyColor(), true).GetCSSPropertyName().Id());
}

}  // namespace blink
