// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/style/style_name.h"
#include "third_party/blink/renderer/core/style/style_name_or_keyword.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

TEST(ComputedStyleUtilsTest, MatrixForce3D) {
  TransformationMatrix identity;
  EXPECT_EQ(ComputedStyleUtils::ValueForTransformationMatrix(identity, 1, false)
                ->CssText(),
            "matrix(1, 0, 0, 1, 0, 0)");
  EXPECT_EQ(ComputedStyleUtils::ValueForTransformationMatrix(identity, 1, true)
                ->CssText(),
            "matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)");
}

TEST(ComputedStyleUtilsTest, MatrixZoom2D) {
  TransformationMatrix matrix(1, 2, 3, 4, 5, 6);
  EXPECT_EQ(ComputedStyleUtils::ValueForTransformationMatrix(matrix, 1, false)
                ->CssText(),
            "matrix(1, 2, 3, 4, 5, 6)");
  matrix.Zoom(2);
  EXPECT_EQ(ComputedStyleUtils::ValueForTransformationMatrix(matrix, 2, false)
                ->CssText(),
            "matrix(1, 2, 3, 4, 5, 6)");
}

TEST(ComputedStyleUtilsTest, MatrixZoom3D) {
  TransformationMatrix matrix(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                              16);
  EXPECT_EQ(ComputedStyleUtils::ValueForTransformationMatrix(matrix, 1, false)
                ->CssText(),
            "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)");
  matrix.Zoom(2);
  EXPECT_EQ(ComputedStyleUtils::ValueForTransformationMatrix(matrix, 2, false)
                ->CssText(),
            "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)");
}

TEST(ComputedStyleUtilsTest, ValueForStyleName) {
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleName(
                StyleName("foo", StyleName::Type::kCustomIdent)),
            *MakeGarbageCollected<CSSCustomIdentValue>("foo"));
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleName(
                StyleName("foo", StyleName::Type::kString)),
            *MakeGarbageCollected<CSSStringValue>("foo"));
}

TEST(ComputedStyleUtilsTest, ValueForStyleNameOrKeyword) {
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleNameOrKeyword(StyleNameOrKeyword(
                StyleName("foo", StyleName::Type::kCustomIdent))),
            *MakeGarbageCollected<CSSCustomIdentValue>("foo"));
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleNameOrKeyword(
                StyleNameOrKeyword(StyleName("foo", StyleName::Type::kString))),
            *MakeGarbageCollected<CSSStringValue>("foo"));
  EXPECT_EQ(*ComputedStyleUtils::ValueForStyleNameOrKeyword(
                StyleNameOrKeyword(CSSValueID::kNone)),
            *MakeGarbageCollected<CSSIdentifierValue>(CSSValueID::kNone));
}

}  // namespace blink
