// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/computed_style.h"

#include "base/memory/values_equivalent.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_value.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_initial_data.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"
#include "ui/base/ui_base_features.h"

namespace blink {

class ComputedStyleTest : public testing::Test {
 protected:
  void SetUp() override {
    initial_style_ = ComputedStyle::CreateInitialStyleSingleton();
  }

  scoped_refptr<ComputedStyle> CreateComputedStyle() {
    return ComputedStyle::Clone(*initial_style_);
  }

 private:
  scoped_refptr<const ComputedStyle> initial_style_;
};

TEST_F(ComputedStyleTest, ShapeOutsideBoxEqual) {
  auto* shape1 = MakeGarbageCollected<ShapeValue>(CSSBoxType::kContent);
  auto* shape2 = MakeGarbageCollected<ShapeValue>(CSSBoxType::kContent);
  scoped_refptr<ComputedStyle> style1 = CreateComputedStyle();
  scoped_refptr<ComputedStyle> style2 = CreateComputedStyle();
  style1->SetShapeOutside(shape1);
  style2->SetShapeOutside(shape2);
  EXPECT_EQ(*style1, *style2);
}

TEST_F(ComputedStyleTest, ShapeOutsideCircleEqual) {
  scoped_refptr<BasicShapeCircle> circle1 = BasicShapeCircle::Create();
  scoped_refptr<BasicShapeCircle> circle2 = BasicShapeCircle::Create();
  auto* shape1 = MakeGarbageCollected<ShapeValue>(std::move(circle1),
                                                  CSSBoxType::kContent);
  auto* shape2 = MakeGarbageCollected<ShapeValue>(std::move(circle2),
                                                  CSSBoxType::kContent);
  scoped_refptr<ComputedStyle> style1 = CreateComputedStyle();
  scoped_refptr<ComputedStyle> style2 = CreateComputedStyle();
  style1->SetShapeOutside(shape1);
  style2->SetShapeOutside(shape2);
  EXPECT_EQ(*style1, *style2);
}

TEST_F(ComputedStyleTest, ClipPathEqual) {
  scoped_refptr<BasicShapeCircle> shape = BasicShapeCircle::Create();
  scoped_refptr<ShapeClipPathOperation> path1 =
      ShapeClipPathOperation::Create(shape);
  scoped_refptr<ShapeClipPathOperation> path2 =
      ShapeClipPathOperation::Create(shape);
  scoped_refptr<ComputedStyle> style1 = CreateComputedStyle();
  scoped_refptr<ComputedStyle> style2 = CreateComputedStyle();
  style1->SetClipPath(path1);
  style2->SetClipPath(path2);
  EXPECT_EQ(*style1, *style2);
}

TEST_F(ComputedStyleTest, SVGStackingContext) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  style->UpdateIsStackingContextWithoutContainment(false, false, true);
  EXPECT_TRUE(style->IsStackingContextWithoutContainment());
}

TEST_F(ComputedStyleTest, Preserve3dForceStackingContext) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  style->SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  style->SetOverflowX(EOverflow::kHidden);
  style->SetOverflowY(EOverflow::kHidden);
  style->UpdateIsStackingContextWithoutContainment(false, false, false);
  EXPECT_EQ(ETransformStyle3D::kFlat, style->UsedTransformStyle3D());
  EXPECT_TRUE(style->IsStackingContextWithoutContainment());
}

TEST_F(ComputedStyleTest, LayoutContainmentStackingContext) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  EXPECT_FALSE(style->IsStackingContextWithoutContainment());
  style->SetContain(kContainsLayout);
  style->UpdateIsStackingContextWithoutContainment(false, false, false);
  // Containment doesn't change IsStackingContextWithoutContainment
  EXPECT_FALSE(style->IsStackingContextWithoutContainment());
}

TEST_F(ComputedStyleTest, TrackedPseudoStyle) {
  for (uint8_t pseudo_id_int = kFirstPublicPseudoId;
       pseudo_id_int <= kLastTrackedPublicPseudoId; pseudo_id_int++) {
    PseudoId pseudo_id = static_cast<PseudoId>(pseudo_id_int);
    scoped_refptr<ComputedStyle> style = CreateComputedStyle();
    style->SetHasPseudoElementStyle(pseudo_id);
    EXPECT_TRUE(style->HasPseudoElementStyle(pseudo_id));
    EXPECT_TRUE(style->HasAnyPseudoElementStyles());
  }
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesRespectsTransformAnimation) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);
  other->SetHasCurrentTransformAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsTransform) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  TransformOperations operations;
  // An operation is necessary since having either a non-empty transform list
  // or a transform animation will set HasTransform();
  operations.Operations().push_back(
      ScaleTransformOperation::Create(1, 1, TransformOperation::kScale));

  style->SetTransform(operations);
  other->SetTransform(operations);

  other->SetHasCurrentTransformAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_FALSE(diff.TransformChanged());
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesRespectsScaleAnimation) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);
  other->SetHasCurrentScaleAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesRespectsRotateAnimation) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);
  other->SetHasCurrentRotateAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesRespectsTranslateAnimation) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);
  other->SetHasCurrentTranslateAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsOpacity) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasCurrentOpacityAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsFilter) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasCurrentFilterAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsBackdropFilter) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasCurrentBackdropFilterAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsBackfaceVisibility) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetBackfaceVisibility(EBackfaceVisibility::kHidden);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsWillChange) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetBackfaceVisibility(EBackfaceVisibility::kHidden);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsUsedStylePreserve3D) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  style->SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  // This induces a flat used transform style.
  other->SetOpacity(0.5);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsOverflow) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetOverflowX(EOverflow::kHidden);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsContainsPaint) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  // This induces a flat used transform style.
  other->SetContain(kContainsPaint);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest, HasOutlineWithCurrentColor) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  EXPECT_FALSE(style->HasOutline());
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineColor(StyleColor::CurrentColor());
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineWidth(5);
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasOutlineWithCurrentColor());
}

TEST_F(ComputedStyleTest, BorderWidth) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  style->SetBorderBottomWidth(5);
  EXPECT_EQ(style->BorderBottomWidth(), 0);
  EXPECT_EQ(style->BorderBottom().Width(), 5);
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  EXPECT_EQ(style->BorderBottomWidth(), 5);
  EXPECT_EQ(style->BorderBottom().Width(), 5);
}

TEST_F(ComputedStyleTest, CursorList) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = CreateComputedStyle();

  auto* gradient = MakeGarbageCollected<cssvalue::CSSLinearGradientValue>(
      nullptr, nullptr, nullptr, nullptr, nullptr, cssvalue::kRepeating);

  auto* image_value = MakeGarbageCollected<StyleGeneratedImage>(
      *gradient, StyleGeneratedImage::ContainerSizes());
  auto* other_image_value = MakeGarbageCollected<StyleGeneratedImage>(
      *gradient, StyleGeneratedImage::ContainerSizes());

  EXPECT_TRUE(base::ValuesEquivalent(image_value, other_image_value));

  style->AddCursor(image_value, false);
  other->AddCursor(other_image_value, false);
  EXPECT_EQ(*style, *other);
}

TEST_F(ComputedStyleTest, BorderStyle) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> other = CreateComputedStyle();
  style->SetBorderLeftStyle(EBorderStyle::kSolid);
  style->SetBorderTopStyle(EBorderStyle::kSolid);
  style->SetBorderRightStyle(EBorderStyle::kSolid);
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  other->SetBorderLeftStyle(EBorderStyle::kSolid);
  other->SetBorderTopStyle(EBorderStyle::kSolid);
  other->SetBorderRightStyle(EBorderStyle::kSolid);
  other->SetBorderBottomStyle(EBorderStyle::kSolid);

  EXPECT_TRUE(style->BorderSizeEquals(*other));
  style->SetBorderLeftWidth(1.0);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  other->SetBorderLeftWidth(1.0);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  EXPECT_TRUE(style->BorderSizeEquals(*other));
  style->SetBorderTopWidth(1.0);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  other->SetBorderTopWidth(1.0);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  EXPECT_TRUE(style->BorderSizeEquals(*other));
  style->SetBorderRightWidth(1.0);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  other->SetBorderRightWidth(1.0);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  EXPECT_TRUE(style->BorderSizeEquals(*other));
  style->SetBorderBottomWidth(1.0);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  other->SetBorderBottomWidth(1.0);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  style->SetBorderLeftStyle(EBorderStyle::kHidden);
  EXPECT_EQ(LayoutUnit(), style->BorderLeftWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderLeftStyle(EBorderStyle::kNone);
  EXPECT_EQ(LayoutUnit(), style->BorderLeftWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderLeftStyle(EBorderStyle::kSolid);
  EXPECT_EQ(LayoutUnit(1), style->BorderLeftWidth());
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  style->SetBorderTopStyle(EBorderStyle::kHidden);
  EXPECT_EQ(LayoutUnit(), style->BorderTopWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderTopStyle(EBorderStyle::kNone);
  EXPECT_EQ(LayoutUnit(), style->BorderTopWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderTopStyle(EBorderStyle::kSolid);
  EXPECT_EQ(LayoutUnit(1), style->BorderTopWidth());
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  style->SetBorderRightStyle(EBorderStyle::kHidden);
  EXPECT_EQ(LayoutUnit(), style->BorderRightWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderRightStyle(EBorderStyle::kNone);
  EXPECT_EQ(LayoutUnit(), style->BorderRightWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderRightStyle(EBorderStyle::kSolid);
  EXPECT_EQ(LayoutUnit(1), style->BorderRightWidth());
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  style->SetBorderBottomStyle(EBorderStyle::kHidden);
  EXPECT_EQ(LayoutUnit(), style->BorderBottomWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderBottomStyle(EBorderStyle::kNone);
  EXPECT_EQ(LayoutUnit(), style->BorderBottomWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  EXPECT_EQ(LayoutUnit(1), style->BorderBottomWidth());
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  EXPECT_TRUE(style->HasBorder());
  style->SetBorderTopStyle(EBorderStyle::kHidden);
  EXPECT_TRUE(style->HasBorder());
  style->SetBorderRightStyle(EBorderStyle::kHidden);
  EXPECT_TRUE(style->HasBorder());
  style->SetBorderBottomStyle(EBorderStyle::kHidden);
  EXPECT_TRUE(style->HasBorder());
  style->SetBorderLeftStyle(EBorderStyle::kHidden);
  EXPECT_FALSE(style->HasBorder());

  style->SetBorderTopStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasBorder());
  style->SetBorderRightStyle(EBorderStyle::kSolid);
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  style->SetBorderLeftStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasBorder());

  style->SetBorderTopStyle(EBorderStyle::kNone);
  EXPECT_TRUE(style->HasBorder());
  style->SetBorderRightStyle(EBorderStyle::kNone);
  EXPECT_TRUE(style->HasBorder());
  style->SetBorderBottomStyle(EBorderStyle::kNone);
  EXPECT_TRUE(style->HasBorder());
  style->SetBorderLeftStyle(EBorderStyle::kNone);
  EXPECT_FALSE(style->HasBorder());
}

#define TEST_ANIMATION_FLAG(flag, inherited)                               \
  do {                                                                     \
    auto style = CreateComputedStyle();                                    \
    auto other = CreateComputedStyle();                                    \
    EXPECT_FALSE(style->flag());                                           \
    EXPECT_FALSE(other->flag());                                           \
    style->Set##flag(true);                                                \
    EXPECT_TRUE(style->flag());                                            \
    EXPECT_EQ(ComputedStyle::Difference::inherited,                        \
              ComputedStyle::ComputeDifference(style.get(), other.get())); \
    auto diff = style->VisualInvalidationDiff(*document, *other);          \
    EXPECT_TRUE(diff.HasDifference());                                     \
    EXPECT_TRUE(diff.CompositingReasonsChanged());                         \
  } while (false)

#define TEST_ANIMATION_FLAG_NO_DIFF(flag)                                  \
  do {                                                                     \
    auto style = CreateComputedStyle();                                    \
    auto other = CreateComputedStyle();                                    \
    EXPECT_FALSE(style->flag());                                           \
    EXPECT_FALSE(other->flag());                                           \
    style->Set##flag(true);                                                \
    EXPECT_TRUE(style->flag());                                            \
    EXPECT_EQ(ComputedStyle::Difference::kEqual,                           \
              ComputedStyle::ComputeDifference(style.get(), other.get())); \
    auto diff = style->VisualInvalidationDiff(*document, *other);          \
    EXPECT_FALSE(diff.HasDifference());                                    \
    EXPECT_FALSE(diff.CompositingReasonsChanged());                        \
  } while (false)

TEST_F(ComputedStyleTest, AnimationFlags) {
  Persistent<Document> document = Document::CreateForTest();
  TEST_ANIMATION_FLAG(HasCurrentTransformAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentScaleAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentRotateAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentTranslateAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentOpacityAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentFilterAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentBackdropFilterAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(SubtreeWillChangeContents, kInherited);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningTransformAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningScaleAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningRotateAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningTranslateAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningOpacityAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningFilterAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningBackdropFilterAnimationOnCompositor);
}

TEST_F(ComputedStyleTest, CustomPropertiesEqual_Values) {
  auto dummy = std::make_unique<DummyPageHolder>(gfx::Size(0, 0));
  css_test_helpers::RegisterProperty(dummy->GetDocument(), "--x", "<length>",
                                     "0px", false);

  scoped_refptr<ComputedStyle> style1 = CreateComputedStyle();
  scoped_refptr<ComputedStyle> style2 = CreateComputedStyle();

  using UnitType = CSSPrimitiveValue::UnitType;

  const auto* value1 = CSSNumericLiteralValue::Create(1.0, UnitType::kPixels);
  const auto* value2 = CSSNumericLiteralValue::Create(2.0, UnitType::kPixels);
  const auto* value3 = CSSNumericLiteralValue::Create(1.0, UnitType::kPixels);

  Vector<AtomicString> properties;
  properties.push_back("--x");

  style1->SetVariableValue("--x", value1, false);
  style2->SetVariableValue("--x", value1, false);
  EXPECT_TRUE(style1->CustomPropertiesEqual(properties, *style2));

  style1->SetVariableValue("--x", value1, false);
  style2->SetVariableValue("--x", value3, false);
  EXPECT_TRUE(style1->CustomPropertiesEqual(properties, *style2));

  style1->SetVariableValue("--x", value1, false);
  style2->SetVariableValue("--x", value2, false);
  EXPECT_FALSE(style1->CustomPropertiesEqual(properties, *style2));
}

TEST_F(ComputedStyleTest, CustomPropertiesEqual_Data) {
  auto dummy = std::make_unique<DummyPageHolder>(gfx::Size(0, 0));
  css_test_helpers::RegisterProperty(dummy->GetDocument(), "--x", "<length>",
                                     "0px", false);

  scoped_refptr<ComputedStyle> style1 = CreateComputedStyle();
  scoped_refptr<ComputedStyle> style2 = CreateComputedStyle();

  auto value1 = css_test_helpers::CreateVariableData("foo");
  auto value2 = css_test_helpers::CreateVariableData("bar");
  auto value3 = css_test_helpers::CreateVariableData("foo");

  Vector<AtomicString> properties;
  properties.push_back("--x");

  style1->SetVariableData("--x", value1, false);
  style2->SetVariableData("--x", value1, false);
  EXPECT_TRUE(style1->CustomPropertiesEqual(properties, *style2));

  style1->SetVariableData("--x", value1, false);
  style2->SetVariableData("--x", value3, false);
  EXPECT_TRUE(style1->CustomPropertiesEqual(properties, *style2));

  style1->SetVariableData("--x", value1, false);
  style2->SetVariableData("--x", value2, false);
  EXPECT_FALSE(style1->CustomPropertiesEqual(properties, *style2));
}

TEST_F(ComputedStyleTest, CustomPropertiesInheritance_FastPath) {
  auto dummy = std::make_unique<DummyPageHolder>(gfx::Size(0, 0));
  css_test_helpers::RegisterProperty(dummy->GetDocument(), "--x", "<length>",
                                     "0px", true);

  scoped_refptr<ComputedStyle> old_style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> new_style = CreateComputedStyle();

  using UnitType = CSSPrimitiveValue::UnitType;

  const auto* value1 = CSSNumericLiteralValue::Create(1.0, UnitType::kPixels);
  const auto* value2 = CSSNumericLiteralValue::Create(2.0, UnitType::kPixels);

  EXPECT_FALSE(old_style->HasVariableDeclaration());
  EXPECT_FALSE(old_style->HasVariableReference());
  EXPECT_FALSE(new_style->HasVariableReference());
  EXPECT_FALSE(new_style->HasVariableDeclaration());

  // Removed variable
  old_style->SetVariableValue("--x", value1, true);
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  old_style = CreateComputedStyle();
  new_style = CreateComputedStyle();

  // Added a new variable
  new_style->SetVariableValue("--x", value2, true);
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  // Change value of variable
  old_style->SetVariableValue("--x", value1, true);
  new_style->SetVariableValue("--x", value2, true);
  new_style->SetHasVariableReference();
  EXPECT_FALSE(new_style->HasVariableDeclaration());
  EXPECT_TRUE(new_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  old_style = CreateComputedStyle();
  new_style = CreateComputedStyle();

  // New styles with variable declaration don't force style recalc
  old_style->SetVariableValue("--x", value1, true);
  new_style->SetVariableValue("--x", value2, true);
  new_style->SetHasVariableDeclaration();
  EXPECT_TRUE(new_style->HasVariableDeclaration());
  EXPECT_FALSE(new_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  old_style = CreateComputedStyle();
  new_style = CreateComputedStyle();

  // New styles with variable reference don't force style recalc
  old_style->SetVariableValue("--x", value1, true);
  new_style->SetVariableValue("--x", value2, true);
  new_style->SetHasVariableDeclaration();
  new_style->SetHasVariableReference();
  EXPECT_TRUE(new_style->HasVariableDeclaration());
  EXPECT_TRUE(new_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));
}

TEST_F(ComputedStyleTest, CustomPropertiesInheritance_StyleRecalc) {
  auto dummy = std::make_unique<DummyPageHolder>(gfx::Size(0, 0));
  css_test_helpers::RegisterProperty(dummy->GetDocument(), "--x", "<length>",
                                     "0px", true);

  scoped_refptr<ComputedStyle> old_style = CreateComputedStyle();
  scoped_refptr<ComputedStyle> new_style = CreateComputedStyle();

  using UnitType = CSSPrimitiveValue::UnitType;

  const auto* value1 = CSSNumericLiteralValue::Create(1.0, UnitType::kPixels);
  const auto* value2 = CSSNumericLiteralValue::Create(2.0, UnitType::kPixels);

  EXPECT_FALSE(old_style->HasVariableDeclaration());
  EXPECT_FALSE(old_style->HasVariableReference());
  EXPECT_FALSE(new_style->HasVariableReference());
  EXPECT_FALSE(new_style->HasVariableDeclaration());

  // Removed variable value
  // Old styles with variable reference force style recalc
  old_style->SetHasVariableReference();
  old_style->SetVariableValue("--x", value2, true);
  EXPECT_TRUE(old_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  old_style = CreateComputedStyle();
  new_style = CreateComputedStyle();

  // New variable value
  // Old styles with variable declaration force style recalc
  old_style->SetHasVariableDeclaration();
  new_style->SetVariableValue("--x", value2, true);
  EXPECT_TRUE(old_style->HasVariableDeclaration());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  old_style = CreateComputedStyle();
  new_style = CreateComputedStyle();

  // Change variable value
  // Old styles with variable declaration force style recalc
  old_style->SetVariableValue("--x", value1, true);
  new_style->SetVariableValue("--x", value2, true);
  old_style->SetHasVariableDeclaration();
  EXPECT_TRUE(old_style->HasVariableDeclaration());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  old_style = CreateComputedStyle();
  new_style = CreateComputedStyle();

  // Change variable value
  // Old styles with variable reference force style recalc
  old_style->SetVariableValue("--x", value1, true);
  new_style->SetVariableValue("--x", value2, true);
  old_style->SetHasVariableReference();
  EXPECT_TRUE(old_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));
}

TEST_F(ComputedStyleTest, ApplyColorSchemeLightOnDark) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  Document& document = dummy_page_holder->GetDocument();
  scoped_refptr<const ComputedStyle> initial =
      document.GetStyleResolver().InitialStyleForElement();

  ColorSchemeHelper color_scheme_helper(document);
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial.get()));

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  state.SetStyle(style);

  CSSPropertyRef ref("color-scheme", state.GetDocument());

  CSSValueList* dark_value = CSSValueList::CreateSpaceSeparated();
  dark_value->Append(*CSSIdentifierValue::Create(CSSValueID::kDark));

  CSSValueList* light_value = CSSValueList::CreateSpaceSeparated();
  light_value->Append(*CSSIdentifierValue::Create(CSSValueID::kLight));

  To<Longhand>(ref.GetProperty()).ApplyValue(state, *dark_value);
  EXPECT_EQ(mojom::blink::ColorScheme::kDark, style->UsedColorScheme());

  To<Longhand>(ref.GetProperty()).ApplyValue(state, *light_value);
  EXPECT_EQ(mojom::blink::ColorScheme::kLight, style->UsedColorScheme());
}

TEST_F(ComputedStyleTest, ApplyInternalLightDarkColor) {
  using css_test_helpers::ParseDeclarationBlock;

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  Document& document = dummy_page_holder->GetDocument();
  scoped_refptr<const ComputedStyle> initial =
      document.GetStyleResolver().InitialStyleForElement();

  ColorSchemeHelper color_scheme_helper(document);
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial.get()));

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  state.SetStyle(style);

  CSSValueList* dark_value = CSSValueList::CreateSpaceSeparated();
  dark_value->Append(*CSSIdentifierValue::Create(CSSValueID::kDark));

  CSSValueList* light_value = CSSValueList::CreateSpaceSeparated();
  light_value->Append(*CSSIdentifierValue::Create(CSSValueID::kLight));

  auto* color_declaration = ParseDeclarationBlock(
      "color:-internal-light-dark(black, white)", CSSParserMode::kUASheetMode);
  auto* dark_declaration = ParseDeclarationBlock("color-scheme:dark");
  auto* light_declaration = ParseDeclarationBlock("color-scheme:light");

  StyleCascade cascade1(state);
  cascade1.MutableMatchResult().AddMatchedProperties(color_declaration);
  cascade1.MutableMatchResult().AddMatchedProperties(dark_declaration);
  cascade1.Apply();
  EXPECT_EQ(Color::kWhite, style->VisitedDependentColor(GetCSSPropertyColor()));

  StyleCascade cascade2(state);
  cascade2.MutableMatchResult().AddMatchedProperties(color_declaration);
  cascade2.MutableMatchResult().AddMatchedProperties(light_declaration);
  cascade2.Apply();
  EXPECT_EQ(Color::kBlack, style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(ComputedStyleTest, ApplyInternalLightDarkBackgroundImage) {
  using css_test_helpers::ParseDeclarationBlock;

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  Document& document = dummy_page_holder->GetDocument();
  scoped_refptr<const ComputedStyle> initial =
      document.GetStyleResolver().InitialStyleForElement();

  ColorSchemeHelper color_scheme_helper(document);
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial.get()));

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  state.SetStyle(style);

  auto* bgimage_declaration = ParseDeclarationBlock(
      "background-image:-internal-light-dark(none, url(dummy.png))",
      kUASheetMode);
  auto* dark_declaration = ParseDeclarationBlock("color-scheme:dark");
  auto* light_declaration = ParseDeclarationBlock("color-scheme:light");

  StyleCascade cascade1(state);
  cascade1.MutableMatchResult().AddMatchedProperties(bgimage_declaration);
  cascade1.MutableMatchResult().AddMatchedProperties(dark_declaration);
  cascade1.Apply();
  EXPECT_TRUE(style->HasBackgroundImage());

  style = CreateComputedStyle();
  state.SetStyle(style);

  StyleCascade cascade2(state);
  cascade2.MutableMatchResult().AddMatchedProperties(bgimage_declaration);
  cascade2.MutableMatchResult().AddMatchedProperties(light_declaration);
  cascade2.Apply();
  EXPECT_FALSE(style->HasBackgroundImage());
}

TEST_F(ComputedStyleTest, StrokeWidthZoomAndCalc) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  Document& document = dummy_page_holder->GetDocument();
  scoped_refptr<const ComputedStyle> initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial.get()));

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  style->SetEffectiveZoom(1.5);
  state.SetStyle(style);

  auto* calc_value = CSSMathFunctionValue::Create(
      CSSMathExpressionNumericLiteral::Create(CSSNumericLiteralValue::Create(
          10, CSSPrimitiveValue::UnitType::kNumber)));

  To<Longhand>(GetCSSPropertyStrokeWidth()).ApplyValue(state, *calc_value);
  auto* computed_value = To<Longhand>(GetCSSPropertyStrokeWidth())
                             .CSSValueFromComputedStyleInternal(
                                 *style, nullptr /* layout_object */,
                                 false /* allow_visited_style */);
  ASSERT_TRUE(computed_value);
  auto* numeric_value = DynamicTo<CSSNumericLiteralValue>(computed_value);
  ASSERT_TRUE(numeric_value);
  EXPECT_TRUE(numeric_value->IsPx());
  EXPECT_EQ(10, numeric_value->DoubleValue());
}

TEST_F(ComputedStyleTest, InitialVariableNamesEmpty) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  EXPECT_TRUE(style->GetVariableNames().IsEmpty());
}

TEST_F(ComputedStyleTest, InitialVariableNames) {
  using css_test_helpers::CreateLengthRegistration;

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
  registry->RegisterProperty("--x", *CreateLengthRegistration("--x", 1));
  registry->RegisterProperty("--y", *CreateLengthRegistration("--y", 2));
  style->SetInitialData(
      StyleInitialData::Create(*Document::CreateForTest(), *registry));

  EXPECT_EQ(2u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--y"));
}

TEST_F(ComputedStyleTest, InheritedVariableNames) {
  using css_test_helpers::CreateVariableData;

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  const bool inherited = true;
  style->SetVariableData("--a", CreateVariableData("foo"), inherited);
  style->SetVariableData("--b", CreateVariableData("bar"), inherited);

  EXPECT_EQ(2u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
}

TEST_F(ComputedStyleTest, NonInheritedVariableNames) {
  using css_test_helpers::CreateVariableData;

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  const bool inherited = true;
  style->SetVariableData("--a", CreateVariableData("foo"), !inherited);
  style->SetVariableData("--b", CreateVariableData("bar"), !inherited);

  EXPECT_EQ(2u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
}

TEST_F(ComputedStyleTest, InheritedAndNonInheritedVariableNames) {
  using css_test_helpers::CreateVariableData;

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  const bool inherited = true;
  style->SetVariableData("--a", CreateVariableData("foo"), inherited);
  style->SetVariableData("--b", CreateVariableData("bar"), inherited);
  style->SetVariableData("--d", CreateVariableData("foz"), !inherited);
  style->SetVariableData("--c", CreateVariableData("baz"), !inherited);

  EXPECT_EQ(4u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--c"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--d"));
}

TEST_F(ComputedStyleTest, InitialAndInheritedAndNonInheritedVariableNames) {
  using css_test_helpers::CreateLengthRegistration;
  using css_test_helpers::CreateVariableData;

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
  registry->RegisterProperty("--b", *CreateLengthRegistration("--b", 1));
  registry->RegisterProperty("--e", *CreateLengthRegistration("--e", 2));
  style->SetInitialData(
      StyleInitialData::Create(*Document::CreateForTest(), *registry));

  const bool inherited = true;
  style->SetVariableData("--a", CreateVariableData("foo"), inherited);
  style->SetVariableData("--b", CreateVariableData("bar"), inherited);
  style->SetVariableData("--d", CreateVariableData("foz"), !inherited);
  style->SetVariableData("--c", CreateVariableData("baz"), !inherited);

  EXPECT_EQ(5u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--c"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--d"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--e"));
}

TEST_F(ComputedStyleTest, GetVariableNamesCount_Invalidation) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  EXPECT_EQ(style->GetVariableNamesCount(), 0u);

  auto data = css_test_helpers::CreateVariableData("foo");
  style->SetVariableData("--x", data, false);
  EXPECT_EQ(style->GetVariableNamesCount(), 1u);

  style->SetVariableData("--y", data, false);
  EXPECT_EQ(style->GetVariableNamesCount(), 2u);

  style->SetVariableData("--z", data, true);
  EXPECT_EQ(style->GetVariableNamesCount(), 3u);
}

TEST_F(ComputedStyleTest, GetVariableNames_Invalidation) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  auto data = css_test_helpers::CreateVariableData("foo");
  style->SetVariableData("--x", data, false);
  EXPECT_EQ(style->GetVariableNames().size(), 1u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));

  style->SetVariableData("--y", data, false);
  EXPECT_EQ(style->GetVariableNames().size(), 2u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--y"));

  style->SetVariableData("--z", data, true);
  EXPECT_EQ(style->GetVariableNames().size(), 3u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--y"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--z"));
}

TEST_F(ComputedStyleTest, GetVariableNamesWithInitialData_Invalidation) {
  using css_test_helpers::CreateLengthRegistration;

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  {
    PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
    registry->RegisterProperty("--x", *CreateLengthRegistration("--x", 1));
    style->SetInitialData(
        StyleInitialData::Create(*Document::CreateForTest(), *registry));
  }
  EXPECT_EQ(style->GetVariableNames().size(), 1u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));

  // Not set StyleInitialData to something else.
  {
    PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
    registry->RegisterProperty("--y", *CreateLengthRegistration("--y", 2));
    registry->RegisterProperty("--z", *CreateLengthRegistration("--z", 3));
    style->SetInitialData(
        StyleInitialData::Create(*Document::CreateForTest(), *registry));
  }
  EXPECT_EQ(style->GetVariableNames().size(), 2u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--y"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--z"));
}

TEST_F(ComputedStyleTest, BorderWidthZoom) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  Document& document = dummy_page_holder->GetDocument();
  scoped_refptr<const ComputedStyle> initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial.get()));

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  style->SetEffectiveZoom(2);
  style->SetBorderLeftStyle(EBorderStyle::kSolid);
  style->SetOutlineStyle(EBorderStyle::kSolid);
  style->SetColumnRuleStyle(EBorderStyle::kSolid);
  state.SetStyle(style);

  const struct {
    CSSIdentifierValue* css_value;
    double expected_px;
    STACK_ALLOCATED();
  } tests[] = {
      {CSSIdentifierValue::Create(CSSValueID::kThin), 1.0},
      {CSSIdentifierValue::Create(CSSValueID::kMedium), 3.0},
      {CSSIdentifierValue::Create(CSSValueID::kThick), 5.0},
  };

  for (const auto& test : tests) {
    for (const auto* property :
         {&GetCSSPropertyBorderLeftWidth(), &GetCSSPropertyOutlineWidth(),
          &GetCSSPropertyColumnRuleWidth()}) {
      const Longhand& longhand = To<Longhand>(*property);
      longhand.ApplyValue(state, *test.css_value);
      auto* computed_value = longhand.CSSValueFromComputedStyleInternal(
          *style, nullptr /* layout_object */, false /* allow_visited_style */);
      AtomicString prop_name = longhand.GetCSSPropertyName().ToAtomicString();
      ASSERT_TRUE(computed_value) << prop_name;
      auto* numeric_value = DynamicTo<CSSNumericLiteralValue>(computed_value);
      ASSERT_TRUE(numeric_value) << prop_name;
      EXPECT_TRUE(numeric_value->IsPx()) << prop_name;
      EXPECT_EQ(test.expected_px, numeric_value->DoubleValue()) << prop_name;
    }
  }
}

TEST_F(ComputedStyleTest,
       TextDecorationEqualDoesNotRequireRecomputeInkOverflow) {
  using css_test_helpers::ParseDeclarationBlock;

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  Document& document = dummy_page_holder->GetDocument();
  scoped_refptr<const ComputedStyle> initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial.get()));

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  // Set up the initial text decoration properties
  style->SetTextDecorationStyle(ETextDecorationStyle::kSolid);
  style->SetTextDecorationColor(StyleColor(CSSValueID::kGreen));
  style->SetTextDecorationLine(TextDecorationLine::kUnderline);
  style->SetTextDecorationThickness(
      TextDecorationThickness(Length(5, Length::Type::kFixed)));
  style->SetTextUnderlineOffset(Length(2, Length::Type::kFixed));
  style->SetTextUnderlinePosition(kTextUnderlinePositionUnder);
  state.SetStyle(style);
  StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
  EXPECT_EQ(TextDecorationLine::kUnderline, style->TextDecorationsInEffect());

  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);
  StyleDifference diff1;
  style->UpdatePropertySpecificDifferences(*other, diff1);
  EXPECT_FALSE(diff1.NeedsRecomputeVisualOverflow());

  // Change the color, and it should not invalidate
  other->SetTextDecorationColor(StyleColor(CSSValueID::kBlue));
  state.SetStyle(other);
  StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
  StyleDifference diff2;
  style->UpdatePropertySpecificDifferences(*other, diff2);
  EXPECT_FALSE(diff2.NeedsRecomputeVisualOverflow());
}

TEST_F(ComputedStyleTest, TextDecorationNotEqualRequiresRecomputeInkOverflow) {
  using css_test_helpers::ParseDeclarationBlock;

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  Document& document = dummy_page_holder->GetDocument();
  scoped_refptr<const ComputedStyle> initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial.get()));

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  // Set up the initial text decoration properties
  style->SetTextDecorationStyle(ETextDecorationStyle::kSolid);
  style->SetTextDecorationColor(StyleColor(CSSValueID::kGreen));
  style->SetTextDecorationLine(TextDecorationLine::kUnderline);
  style->SetTextDecorationThickness(
      TextDecorationThickness(Length(5, Length::Type::kFixed)));
  style->SetTextUnderlineOffset(Length(2, Length::Type::kFixed));
  style->SetTextUnderlinePosition(kTextUnderlinePositionUnder);
  state.SetStyle(style);
  StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);

  // Change decoration style
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);
  other->SetTextDecorationStyle(ETextDecorationStyle::kWavy);
  state.SetStyle(other);
  StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
  StyleDifference diff_decoration_style;
  style->UpdatePropertySpecificDifferences(*other, diff_decoration_style);
  EXPECT_TRUE(diff_decoration_style.NeedsRecomputeVisualOverflow());

  // Change decoration line
  other = ComputedStyle::Clone(*style);
  other->SetTextDecorationLine(TextDecorationLine::kOverline);
  state.SetStyle(other);
  StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
  StyleDifference diff_decoration_line;
  style->UpdatePropertySpecificDifferences(*other, diff_decoration_line);
  EXPECT_TRUE(diff_decoration_line.NeedsRecomputeVisualOverflow());

  // Change decoration thickness
  other = ComputedStyle::Clone(*style);
  other->SetTextDecorationThickness(
      TextDecorationThickness(Length(3, Length::Type::kFixed)));
  state.SetStyle(other);
  StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
  StyleDifference diff_decoration_thickness;
  style->UpdatePropertySpecificDifferences(*other, diff_decoration_thickness);
  EXPECT_TRUE(diff_decoration_thickness.NeedsRecomputeVisualOverflow());

  // Change underline offset
  other = ComputedStyle::Clone(*style);
  other->SetTextUnderlineOffset(Length(4, Length::Type::kFixed));
  state.SetStyle(other);
  StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
  StyleDifference diff_underline_offset;
  style->UpdatePropertySpecificDifferences(*other, diff_underline_offset);
  EXPECT_TRUE(diff_underline_offset.NeedsRecomputeVisualOverflow());

  // Change underline position
  other = ComputedStyle::Clone(*style);
  other->SetTextUnderlinePosition(kTextUnderlinePositionLeft);
  state.SetStyle(other);
  StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
  StyleDifference diff_underline_position;
  style->UpdatePropertySpecificDifferences(*other, diff_underline_position);
  EXPECT_TRUE(diff_underline_position.NeedsRecomputeVisualOverflow());
}

// Verify that cloned ComputedStyle is independent from source, i.e.
// copy-on-write works as expected.
TEST_F(ComputedStyleTest, ClonedStyleAnimationsAreIndependent) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  auto& animations = style->AccessAnimations();
  animations.DelayList().clear();
  animations.DelayList().push_back(CSSAnimationData::InitialDelay());
  EXPECT_EQ(1u, style->Animations()->DelayList().size());

  scoped_refptr<ComputedStyle> cloned_style = ComputedStyle::Clone(*style);
  auto& cloned_style_animations = cloned_style->AccessAnimations();
  EXPECT_EQ(1u, cloned_style_animations.DelayList().size());
  cloned_style_animations.DelayList().push_back(
      CSSAnimationData::InitialDelay());

  EXPECT_EQ(2u, cloned_style->Animations()->DelayList().size());
  EXPECT_EQ(1u, style->Animations()->DelayList().size());
}

TEST_F(ComputedStyleTest, ClonedStyleTransitionsAreIndependent) {
  scoped_refptr<ComputedStyle> style = CreateComputedStyle();

  auto& transitions = style->AccessTransitions();
  transitions.PropertyList().clear();
  transitions.PropertyList().push_back(CSSTransitionData::InitialProperty());
  EXPECT_EQ(1u, style->Transitions()->PropertyList().size());

  scoped_refptr<ComputedStyle> cloned_style = ComputedStyle::Clone(*style);
  auto& cloned_style_transitions = cloned_style->AccessTransitions();
  EXPECT_EQ(1u, cloned_style_transitions.PropertyList().size());
  cloned_style_transitions.PropertyList().push_back(
      CSSTransitionData::InitialProperty());

  EXPECT_EQ(2u, cloned_style->Transitions()->PropertyList().size());
  EXPECT_EQ(1u, style->Transitions()->PropertyList().size());
}

TEST_F(ComputedStyleTest, ApplyInitialAnimationNameAndTransitionProperty) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  Document& document = dummy_page_holder->GetDocument();
  scoped_refptr<const ComputedStyle> initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial.get()));

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
  state.SetStyle(style);
  EXPECT_FALSE(style->Animations());
  EXPECT_FALSE(style->Transitions());

  To<Longhand>(GetCSSPropertyAnimationName()).ApplyInitial(state);
  To<Longhand>(GetCSSPropertyTransitionProperty()).ApplyInitial(state);
  EXPECT_FALSE(style->Animations());
  EXPECT_FALSE(style->Transitions());
}

#define TEST_STYLE_VALUE_NO_DIFF(field_name)                        \
  {                                                                 \
    scoped_refptr<ComputedStyle> style1 = CreateComputedStyle();    \
    scoped_refptr<ComputedStyle> style2 = CreateComputedStyle();    \
    style1->Set##field_name(                                        \
        ComputedStyleInitialValues::Initial##field_name());         \
    style2->Set##field_name(                                        \
        ComputedStyleInitialValues::Initial##field_name());         \
    auto diff = style1->VisualInvalidationDiff(*document, *style2); \
    EXPECT_FALSE(diff.HasDifference());                             \
  }

// Ensures ref-counted values are compared by their values, not by pointers.
#define TEST_STYLE_REFCOUNTED_VALUE_NO_DIFF(type, field_name)              \
  {                                                                        \
    scoped_refptr<ComputedStyle> style1 = CreateComputedStyle();           \
    scoped_refptr<ComputedStyle> style2 = CreateComputedStyle();           \
    scoped_refptr<type> value1 = base::MakeRefCounted<type>();             \
    scoped_refptr<type> value2 = base::MakeRefCounted<type>(value1->data); \
    style1->Set##field_name(value1);                                       \
    style2->Set##field_name(value2);                                       \
    auto diff = style1->VisualInvalidationDiff(*document, *style2);        \
    EXPECT_FALSE(diff.HasDifference());                                    \
  }

TEST_F(ComputedStyleTest, SvgStrokeStyleShouldCompareValue) {
  Persistent<Document> document = Document::CreateForTest();
  TEST_STYLE_VALUE_NO_DIFF(StrokeOpacity);
  TEST_STYLE_VALUE_NO_DIFF(StrokeMiterLimit);
  TEST_STYLE_VALUE_NO_DIFF(StrokeWidth);
  TEST_STYLE_VALUE_NO_DIFF(StrokeDashOffset);
  TEST_STYLE_REFCOUNTED_VALUE_NO_DIFF(SVGDashArray, StrokeDashArray);

  TEST_STYLE_VALUE_NO_DIFF(StrokePaint);
  TEST_STYLE_VALUE_NO_DIFF(InternalVisitedStrokePaint);
}

TEST_F(ComputedStyleTest, SvgMiscStyleShouldCompareValue) {
  Persistent<Document> document = Document::CreateForTest();
  TEST_STYLE_VALUE_NO_DIFF(FloodColor);
  TEST_STYLE_VALUE_NO_DIFF(FloodOpacity);
  TEST_STYLE_VALUE_NO_DIFF(LightingColor);
  TEST_STYLE_VALUE_NO_DIFF(BaselineShift);
}

TEST_F(ComputedStyleTest, ShouldApplyAnyContainment) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  Document& document = dummy_page_holder->GetDocument();

  auto* html = document.documentElement();
  auto* body = document.body();
  ASSERT_TRUE(html);
  ASSERT_TRUE(body);

  std::vector display_types = {CSSValueID::kInline,
                               CSSValueID::kBlock,
                               CSSValueID::kListItem,
                               CSSValueID::kInlineBlock,
                               CSSValueID::kTable,
                               CSSValueID::kInlineTable,
                               CSSValueID::kTableRowGroup,
                               CSSValueID::kTableHeaderGroup,
                               CSSValueID::kTableFooterGroup,
                               CSSValueID::kTableRow,
                               CSSValueID::kTableColumnGroup,
                               CSSValueID::kTableColumn,
                               CSSValueID::kTableCell,
                               CSSValueID::kTableCaption,
                               CSSValueID::kWebkitBox,
                               CSSValueID::kWebkitInlineBox,
                               CSSValueID::kFlex,
                               CSSValueID::kInlineFlex,
                               CSSValueID::kGrid,
                               CSSValueID::kInlineGrid,
                               CSSValueID::kContents,
                               CSSValueID::kFlowRoot,
                               CSSValueID::kNone};
  if (RuntimeEnabledFeatures::MathMLCoreEnabled())
    display_types.push_back(CSSValueID::kMath);

  for (auto contain :
       {CSSValueID::kNone, CSSValueID::kLayout, CSSValueID::kPaint,
        CSSValueID::kSize, CSSValueID::kStyle}) {
    html->SetInlineStyleProperty(CSSPropertyID::kContain,
                                 getValueName(contain));
    body->SetInlineStyleProperty(CSSPropertyID::kContain,
                                 getValueName(contain));
    for (auto html_display : display_types) {
      html->SetInlineStyleProperty(CSSPropertyID::kDisplay, html_display);
      for (auto body_display : display_types) {
        body->SetInlineStyleProperty(CSSPropertyID::kDisplay, body_display);
        document.View()->UpdateAllLifecyclePhasesForTest();

        if (!html->GetLayoutObject()) {
          EXPECT_TRUE(!html->GetComputedStyle());
          continue;
        }
        EXPECT_EQ(html->GetLayoutObject()->ShouldApplyAnyContainment(),
                  html->GetLayoutObject()->StyleRef().ShouldApplyAnyContainment(
                      *html))
            << "html contain:" << getValueName(contain)
            << " display:" << getValueName(html_display);
        if (!body->GetLayoutObject()) {
          if (const auto* body_style = body->GetComputedStyle()) {
            EXPECT_EQ(body_style->Display(), EDisplay::kContents);
            EXPECT_EQ(body_style->ShouldApplyAnyContainment(*body),
                      contain == CSSValueID::kStyle);
          }
          continue;
        }
        EXPECT_EQ(body->GetLayoutObject()->ShouldApplyAnyContainment(),
                  body->GetLayoutObject()->StyleRef().ShouldApplyAnyContainment(
                      *body))
            << "body contain:" << getValueName(contain)
            << " display:" << getValueName(body_display);
      }
    }
  }
}

#if DCHECK_IS_ON()

TEST_F(ComputedStyleTest, DebugDiffFields) {
  using DebugField = ComputedStyleBase::DebugField;

  scoped_refptr<ComputedStyle> style1 = CreateComputedStyle();
  scoped_refptr<ComputedStyle> style2 = CreateComputedStyle();

  style1->SetWidth(Length(100.0, Length::kFixed));
  style2->SetWidth(Length(200.0, Length::kFixed));

  EXPECT_EQ(0u, style1->DebugDiffFields(*style1).size());
  EXPECT_EQ(0u, style2->DebugDiffFields(*style2).size());

  EXPECT_EQ(1u, style1->DebugDiffFields(*style2).size());
  EXPECT_EQ(DebugField::width_, style1->DebugDiffFields(*style2)[0]);
  EXPECT_EQ("width_",
            ComputedStyleBase::DebugFieldToString(DebugField::width_));
}

#endif  // #if DCHECK_IS_ON()

}  // namespace blink
