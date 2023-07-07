// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/style/style_name.h"
#include "third_party/blink/renderer/core/style/style_name_or_keyword.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

TEST(ComputedStyleUtilsTest, MatrixForce3D) {
  gfx::Transform identity;
  EXPECT_EQ(
      ComputedStyleUtils::ValueForTransform(identity, 1, false)->CssText(),
      "matrix(1, 0, 0, 1, 0, 0)");
  EXPECT_EQ(ComputedStyleUtils::ValueForTransform(identity, 1, true)->CssText(),
            "matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)");
}

TEST(ComputedStyleUtilsTest, MatrixZoom2D) {
  auto matrix = gfx::Transform::Affine(1, 2, 3, 4, 5, 6);
  EXPECT_EQ(ComputedStyleUtils::ValueForTransform(matrix, 1, false)->CssText(),
            "matrix(1, 2, 3, 4, 5, 6)");
  matrix.Zoom(2);
  EXPECT_EQ(ComputedStyleUtils::ValueForTransform(matrix, 2, false)->CssText(),
            "matrix(1, 2, 3, 4, 5, 6)");
}

TEST(ComputedStyleUtilsTest, MatrixZoom3D) {
  auto matrix = gfx::Transform::ColMajor(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                         13, 14, 15, 16);
  EXPECT_EQ(ComputedStyleUtils::ValueForTransform(matrix, 1, false)->CssText(),
            "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)");
  matrix.Zoom(2);
  EXPECT_EQ(ComputedStyleUtils::ValueForTransform(matrix, 2, false)->CssText(),
            "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)");
}

TEST(ComputedStyleUtilsTest, ValueForStyleName) {
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleName(
                StyleName(AtomicString("foo"), StyleName::Type::kCustomIdent)),
            *MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("foo")));
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleName(
                StyleName(AtomicString("foo"), StyleName::Type::kString)),
            *MakeGarbageCollected<CSSStringValue>("foo"));
}

TEST(ComputedStyleUtilsTest, ValueForStyleNameOrKeyword) {
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleNameOrKeyword(StyleNameOrKeyword(
                StyleName(AtomicString("foo"), StyleName::Type::kCustomIdent))),
            *MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("foo")));
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleNameOrKeyword(StyleNameOrKeyword(
                StyleName(AtomicString("foo"), StyleName::Type::kString))),
            *MakeGarbageCollected<CSSStringValue>("foo"));
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleNameOrKeyword(
                StyleNameOrKeyword(CSSValueID::kNone)),
            *MakeGarbageCollected<CSSIdentifierValue>(CSSValueID::kNone));
}

TEST(ComputedStyleUtilsTest, ValueForAnimationDelayWithNullptr) {
  // Verify that ValueForAnimationDelayStart/End produces a CSSValue with
  // canonical structure.
  auto* expected = CSSValueList::CreateCommaSeparated();
  expected->Append(*CSSNumericLiteralValue::Create(
      0, CSSPrimitiveValue::UnitType::kSeconds));

  auto* start = DynamicTo<CSSValueList>(
      ComputedStyleUtils::ValueForAnimationDelayStartList(
          /* CSSTimingData */ nullptr));
  ASSERT_TRUE(start);
  EXPECT_EQ(*expected, *start);

  auto* end =
      DynamicTo<CSSValueList>(ComputedStyleUtils::ValueForAnimationDelayEndList(
          /* CSSTimingData */ nullptr));
  ASSERT_TRUE(end);
  EXPECT_EQ(*expected, *end);
}

}  // namespace blink
