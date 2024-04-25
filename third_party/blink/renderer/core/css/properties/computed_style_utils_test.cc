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
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"
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

TEST(ComputedStyleUtilsTest, ValueForTransformFunction_Translate) {
  TransformOperations operations;
  operations.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length(Length::Type::kFixed), Length(10, Length::Type::kFixed),
          TransformOperation::kTranslateY));
  const CSSValue* value =
      ComputedStyleUtils::ValueForTransformFunction(operations);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsFunctionValue());
  EXPECT_EQ(To<CSSFunctionValue>(value)->FunctionType(),
            CSSValueID::kTranslateY);
  EXPECT_EQ(value->CssText(), "translateY(10px)");

  operations.Operations()[0] =
      MakeGarbageCollected<TranslateTransformOperation>(
          Length(50, Length::Type::kPercent), Length(Length::Type::kFixed),
          TransformOperation::kTranslateX);
  value = ComputedStyleUtils::ValueForTransformFunction(operations);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsFunctionValue());
  EXPECT_EQ(To<CSSFunctionValue>(value)->FunctionType(),
            CSSValueID::kTranslateX);
  EXPECT_EQ(value->CssText(), "translateX(50%)");

  operations.Operations()[0] =
      MakeGarbageCollected<TranslateTransformOperation>(
          Length(Length::Type::kFixed), Length(Length::Type::kFixed), -100.0,
          TransformOperation::kTranslateZ);
  value = ComputedStyleUtils::ValueForTransformFunction(operations);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsFunctionValue());
  EXPECT_EQ(To<CSSFunctionValue>(value)->FunctionType(),
            CSSValueID::kTranslateZ);
  EXPECT_EQ(value->CssText(), "translateZ(-100px)");

  operations.Operations()[0] =
      MakeGarbageCollected<TranslateTransformOperation>(
          Length(-20, Length::Type::kFixed), Length(40, Length::Type::kPercent),
          0.0, TransformOperation::kTranslate);
  value = ComputedStyleUtils::ValueForTransformFunction(operations);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsFunctionValue());
  EXPECT_EQ(To<CSSFunctionValue>(value)->FunctionType(),
            CSSValueID::kTranslate);
  EXPECT_EQ(value->CssText(), "translate(-20px, 40%)");

  operations.Operations()[0] =
      MakeGarbageCollected<TranslateTransformOperation>(
          Length(-20, Length::Type::kFixed), Length(40, Length::Type::kPercent),
          0.0, TransformOperation::kTranslate3D);
  value = ComputedStyleUtils::ValueForTransformFunction(operations);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsFunctionValue());
  EXPECT_EQ(To<CSSFunctionValue>(value)->FunctionType(),
            CSSValueID::kTranslate3d);
  EXPECT_EQ(value->CssText(), "translate3d(-20px, 40%, 0px)");

  operations.Operations()[0] =
      MakeGarbageCollected<TranslateTransformOperation>(
          Length(-20, Length::Type::kFixed), Length(40, Length::Type::kPercent),
          0.0, TransformOperation::kTranslate);
  value = ComputedStyleUtils::ValueForTransformFunction(operations);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsFunctionValue());
  EXPECT_EQ(To<CSSFunctionValue>(value)->FunctionType(),
            CSSValueID::kTranslate);
  EXPECT_EQ(value->CssText(), "translate(-20px, 40%)");
}

TEST(ComputedStyleUtilsTest, ValueForTransformFunction_Matrix) {
  TransformOperations operations;
  gfx::Transform transform;
  transform.Translate(40.f, -20.f);
  operations.Operations().push_back(
      MakeGarbageCollected<MatrixTransformOperation>(transform));
  const CSSValue* value =
      ComputedStyleUtils::ValueForTransformFunction(operations);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsFunctionValue());
  EXPECT_EQ(To<CSSFunctionValue>(value)->FunctionType(), CSSValueID::kMatrix);
  EXPECT_EQ(value->CssText(), "matrix(1, 0, 0, 1, 40, -20)");
}

TEST(ComputedStyleUtilsTest, ValueForTransformFunction_Matrix3d) {
  TransformOperations operations;
  gfx::Transform transform;
  transform.Translate(40.f, -20.f);
  operations.Operations().push_back(
      MakeGarbageCollected<Matrix3DTransformOperation>(transform));
  const CSSValue* value =
      ComputedStyleUtils::ValueForTransformFunction(operations);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsFunctionValue());
  EXPECT_EQ(To<CSSFunctionValue>(value)->FunctionType(), CSSValueID::kMatrix3d);
  EXPECT_EQ(value->CssText(),
            "matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 40, -20, 0, 1)");
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

}  // namespace blink
