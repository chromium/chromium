// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/computed_style.h"

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
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/style/clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_value.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_initial_data.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"
#include "ui/base/ui_base_features.h"

namespace blink {

TEST(ComputedStyleTest, ShapeOutsideBoxEqual) {
  auto* shape1 = MakeGarbageCollected<ShapeValue>(CSSBoxType::kContent);
  auto* shape2 = MakeGarbageCollected<ShapeValue>(CSSBoxType::kContent);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetShapeOutside(shape1);
  style2->SetShapeOutside(shape2);
  EXPECT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ShapeOutsideCircleEqual) {
  scoped_refptr<BasicShapeCircle> circle1 = BasicShapeCircle::Create();
  scoped_refptr<BasicShapeCircle> circle2 = BasicShapeCircle::Create();
  auto* shape1 = MakeGarbageCollected<ShapeValue>(std::move(circle1),
                                                  CSSBoxType::kContent);
  auto* shape2 = MakeGarbageCollected<ShapeValue>(std::move(circle2),
                                                  CSSBoxType::kContent);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetShapeOutside(shape1);
  style2->SetShapeOutside(shape2);
  EXPECT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ClipPathEqual) {
  scoped_refptr<BasicShapeCircle> shape = BasicShapeCircle::Create();
  scoped_refptr<ShapeClipPathOperation> path1 =
      ShapeClipPathOperation::Create(shape);
  scoped_refptr<ShapeClipPathOperation> path2 =
      ShapeClipPathOperation::Create(shape);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetClipPath(path1);
  style2->SetClipPath(path2);
  EXPECT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, FocusRingWidth) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  if (::features::IsFormControlsRefreshEnabled()) {
    style->SetOutlineStyleIsAuto(static_cast<bool>(OutlineIsAuto::kOn));
    EXPECT_EQ(3, style->GetOutlineStrokeWidthForFocusRing());
    style->SetEffectiveZoom(3.5);
    style->SetOutlineWidth(4);
    EXPECT_EQ(3.5, style->GetOutlineStrokeWidthForFocusRing());
  } else {
    style->SetEffectiveZoom(3.5);
    style->SetOutlineStyle(EBorderStyle::kSolid);
#if defined(OS_MAC)
    EXPECT_EQ(3, style->GetOutlineStrokeWidthForFocusRing());
#else
    style->SetOutlineStyleIsAuto(static_cast<bool>(OutlineIsAuto::kOn));
    static uint16_t outline_width = 4;
    style->SetOutlineWidth(outline_width);

    double expected_width = 3.5;
    EXPECT_EQ(expected_width, style->GetOutlineStrokeWidthForFocusRing());

    expected_width = 1.0;
    style->SetEffectiveZoom(0.5);
    EXPECT_EQ(expected_width, style->GetOutlineStrokeWidthForFocusRing());
#endif
  }
}

TEST(ComputedStyleTest, FocusRingOutset) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetOutlineStyle(EBorderStyle::kSolid);
  style->SetOutlineStyleIsAuto(static_cast<bool>(OutlineIsAuto::kOn));
  style->SetEffectiveZoom(4.75);
  if (::features::IsFormControlsRefreshEnabled()) {
    EXPECT_EQ(4, style->OutlineOutsetExtent());
  } else {
#if defined(OS_MAC)
    EXPECT_EQ(4, style->OutlineOutsetExtent());
#else
    EXPECT_EQ(3, style->OutlineOutsetExtent());
#endif
  }
}

TEST(ComputedStyleTest, SVGStackingContext) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->UpdateIsStackingContextWithoutContainment(false, false, true);
  EXPECT_TRUE(style->IsStackingContextWithoutContainment());
}

TEST(ComputedStyleTest, Preserve3dForceStackingContext) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  style->SetOverflowX(EOverflow::kHidden);
  style->SetOverflowY(EOverflow::kHidden);
  style->UpdateIsStackingContextWithoutContainment(false, false, false);
  EXPECT_EQ(ETransformStyle3D::kFlat, style->UsedTransformStyle3D());
  EXPECT_TRUE(style->IsStackingContextWithoutContainment());
}

TEST(ComputedStyleTest, LayoutContainmentStackingContext) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  EXPECT_FALSE(style->IsStackingContextWithoutContainment());
  style->SetContain(kContainsLayout);
  style->UpdateIsStackingContextWithoutContainment(false, false, false);
  // Containment doesn't change IsStackingContextWithoutContainment
  EXPECT_FALSE(style->IsStackingContextWithoutContainment());
}

TEST(ComputedStyleTest, FirstPublicPseudoStyle) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetHasPseudoElementStyle(kPseudoIdFirstLine);
  EXPECT_TRUE(style->HasPseudoElementStyle(kPseudoIdFirstLine));
  EXPECT_TRUE(style->HasAnyPseudoElementStyles());
}

TEST(ComputedStyleTest, LastPublicPseudoElementStyle) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetHasPseudoElementStyle(kPseudoIdScrollbar);
  EXPECT_TRUE(style->HasPseudoElementStyle(kPseudoIdScrollbar));
  EXPECT_TRUE(style->HasAnyPseudoElementStyles());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesRespectsTransformAnimation) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);
  other->SetHasCurrentTransformAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsTransforom) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
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

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsOpacity) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasCurrentOpacityAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsFilter) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasCurrentFilterAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsBackdropFilter) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasCurrentBackdropFilterAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsBackfaceVisibility) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetBackfaceVisibility(EBackfaceVisibility::kHidden);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsWillChange) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetBackfaceVisibility(EBackfaceVisibility::kHidden);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsUsedStylePreserve3D) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  // This induces a flat used transform style.
  other->SetOpacity(0.5);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsOverflow) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetOverflowX(EOverflow::kHidden);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsContainsPaint) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  // This induces a flat used transform style.
  other->SetContain(kContainsPaint);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest, UpdateBackgroundColorDifferencesHasAlpha) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  StyleDifference diff;
  style->AdjustDiffForBackgroundVisuallyEqual(*other, diff);
  EXPECT_FALSE(diff.HasAlphaChanged());

  style->SetBackgroundColor(StyleColor(Color(255, 255, 255, 255)));
  other->SetBackgroundColor(StyleColor(Color(255, 255, 255, 128)));

  EXPECT_FALSE(
      style->VisitedDependentColor(GetCSSPropertyBackgroundColor()).HasAlpha());
  EXPECT_TRUE(
      other->VisitedDependentColor(GetCSSPropertyBackgroundColor()).HasAlpha());

  style->AdjustDiffForBackgroundVisuallyEqual(*other, diff);
  EXPECT_TRUE(diff.HasAlphaChanged());
}

TEST(ComputedStyleTest, UpdateBackgroundLayerDifferencesHasAlpha) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  StyleDifference diff;
  style->AdjustDiffForBackgroundVisuallyEqual(*other, diff);
  EXPECT_FALSE(diff.HasAlphaChanged());

  other->AccessBackgroundLayers().EnsureNext();
  style->AdjustDiffForBackgroundVisuallyEqual(*other, diff);
  EXPECT_TRUE(diff.HasAlphaChanged());
}

TEST(ComputedStyleTest, HasOutlineWithCurrentColor) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  EXPECT_FALSE(style->HasOutline());
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineColor(StyleColor::CurrentColor());
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineWidth(5);
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasOutlineWithCurrentColor());
}

TEST(ComputedStyleTest, HasBorderColorReferencingCurrentColor) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  EXPECT_FALSE(style->HasBorderColorReferencingCurrentColor());
  style->SetBorderBottomColor(StyleColor::CurrentColor());
  EXPECT_FALSE(style->HasBorderColorReferencingCurrentColor());
  style->SetBorderBottomWidth(5);
  EXPECT_FALSE(style->HasBorderColorReferencingCurrentColor());
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasBorderColorReferencingCurrentColor());
}

TEST(ComputedStyleTest, BorderWidth) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetBorderBottomWidth(5);
  EXPECT_EQ(style->BorderBottomWidth(), 0);
  EXPECT_EQ(style->BorderBottom().Width(), 5);
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  EXPECT_EQ(style->BorderBottomWidth(), 5);
  EXPECT_EQ(style->BorderBottom().Width(), 5);
}

TEST(ComputedStyleTest, CursorList) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Create();

  auto* gradient = MakeGarbageCollected<cssvalue::CSSLinearGradientValue>(
      nullptr, nullptr, nullptr, nullptr, nullptr, cssvalue::kRepeating);

  auto* image_value = MakeGarbageCollected<StyleGeneratedImage>(*gradient);
  auto* other_image_value =
      MakeGarbageCollected<StyleGeneratedImage>(*gradient);

  EXPECT_TRUE(DataEquivalent(image_value, other_image_value));

  style->AddCursor(image_value, false);
  other->AddCursor(other_image_value, false);
  EXPECT_EQ(*style, *other);
}

TEST(ComputedStyleTest, BorderStyle) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Create();
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
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderLeftStyle(EBorderStyle::kNone);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderLeftStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  style->SetBorderTopStyle(EBorderStyle::kHidden);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderTopStyle(EBorderStyle::kNone);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderTopStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  style->SetBorderRightStyle(EBorderStyle::kHidden);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderRightStyle(EBorderStyle::kNone);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderRightStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  style->SetBorderBottomStyle(EBorderStyle::kHidden);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderBottomStyle(EBorderStyle::kNone);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->BorderSizeEquals(*other));
}

#define TEST_ANIMATION_FLAG(flag, inherited)                               \
  do {                                                                     \
    auto style = ComputedStyle::Create();                                  \
    auto other = ComputedStyle::Create();                                  \
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
    auto style = ComputedStyle::Create();                                  \
    auto other = ComputedStyle::Create();                                  \
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

TEST(ComputedStyleTest, AnimationFlags) {
  Persistent<Document> document = Document::CreateForTest();
  TEST_ANIMATION_FLAG(HasCurrentTransformAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentOpacityAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentFilterAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentBackdropFilterAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(SubtreeWillChangeContents, kInherited);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningTransformAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningOpacityAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningFilterAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningBackdropFilterAnimationOnCompositor);
}

TEST(ComputedStyleTest, CustomPropertiesEqual_Values) {
  auto dummy = std::make_unique<DummyPageHolder>(IntSize(0, 0));
  css_test_helpers::RegisterProperty(dummy->GetDocument(), "--x", "<length>",
                                     "0px", false);

  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();

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

TEST(ComputedStyleTest, CustomPropertiesEqual_Data) {
  auto dummy = std::make_unique<DummyPageHolder>(IntSize(0, 0));
  css_test_helpers::RegisterProperty(dummy->GetDocument(), "--x", "<length>",
                                     "0px", false);

  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();

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

TEST(ComputedStyleTest, CustomPropertiesInheritance_FastPath) {
  auto dummy = std::make_unique<DummyPageHolder>(IntSize(0, 0));
  css_test_helpers::RegisterProperty(dummy->GetDocument(), "--x", "<length>",
                                     "0px", true);

  scoped_refptr<ComputedStyle> old_style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> new_style = ComputedStyle::Create();

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

  old_style = ComputedStyle::Create();
  new_style = ComputedStyle::Create();

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

  old_style = ComputedStyle::Create();
  new_style = ComputedStyle::Create();

  // New styles with variable declaration don't force style recalc
  old_style->SetVariableValue("--x", value1, true);
  new_style->SetVariableValue("--x", value2, true);
  new_style->SetHasVariableDeclaration();
  EXPECT_TRUE(new_style->HasVariableDeclaration());
  EXPECT_FALSE(new_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  old_style = ComputedStyle::Create();
  new_style = ComputedStyle::Create();

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

TEST(ComputedStyleTest, CustomPropertiesInheritance_StyleRecalc) {
  auto dummy = std::make_unique<DummyPageHolder>(IntSize(0, 0));
  css_test_helpers::RegisterProperty(dummy->GetDocument(), "--x", "<length>",
                                     "0px", true);

  scoped_refptr<ComputedStyle> old_style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> new_style = ComputedStyle::Create();

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

  old_style = ComputedStyle::Create();
  new_style = ComputedStyle::Create();

  // New variable value
  // Old styles with variable declaration force style recalc
  old_style->SetHasVariableDeclaration();
  new_style->SetVariableValue("--x", value2, true);
  EXPECT_TRUE(old_style->HasVariableDeclaration());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  old_style = ComputedStyle::Create();
  new_style = ComputedStyle::Create();

  // Change variable value
  // Old styles with variable declaration force style recalc
  old_style->SetVariableValue("--x", value1, true);
  new_style->SetVariableValue("--x", value2, true);
  old_style->SetHasVariableDeclaration();
  EXPECT_TRUE(old_style->HasVariableDeclaration());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));

  old_style = ComputedStyle::Create();
  new_style = ComputedStyle::Create();

  // Change variable value
  // Old styles with variable reference force style recalc
  old_style->SetVariableValue("--x", value1, true);
  new_style->SetVariableValue("--x", value2, true);
  old_style->SetHasVariableReference();
  EXPECT_TRUE(old_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style.get(), new_style.get()));
}

TEST(ComputedStyleTest, ApplyColorSchemeLightOnDark) {
  ScopedCSSColorSchemeForTest scoped_property_enabled(true);
  ScopedCSSColorSchemeUARenderingForTest scoped_ua_enabled(true);

  std::unique_ptr<DummyPageHolder> dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(0, 0), nullptr);
  const ComputedStyle* initial = &ComputedStyle::InitialStyle();

  ColorSchemeHelper color_scheme_helper(dummy_page_holder_->GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);
  StyleResolverState state(dummy_page_holder_->GetDocument(),
                           *dummy_page_holder_->GetDocument().documentElement(),
                           initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  state.SetStyle(style);

  CSSPropertyRef ref("color-scheme", state.GetDocument());

  CSSValueList* dark_value = CSSValueList::CreateSpaceSeparated();
  dark_value->Append(*CSSIdentifierValue::Create(CSSValueID::kDark));

  CSSValueList* light_value = CSSValueList::CreateSpaceSeparated();
  light_value->Append(*CSSIdentifierValue::Create(CSSValueID::kLight));

  To<Longhand>(ref.GetProperty()).ApplyValue(state, *dark_value);
  EXPECT_EQ(ColorScheme::kDark, style->UsedColorScheme());

  To<Longhand>(ref.GetProperty()).ApplyValue(state, *light_value);
  EXPECT_EQ(ColorScheme::kLight, style->UsedColorScheme());
}

TEST(ComputedStyleTest, ApplyInternalLightDarkColor) {
  using css_test_helpers::ParseDeclarationBlock;

  ScopedCSSColorSchemeForTest scoped_property_enabled(true);
  ScopedCSSColorSchemeUARenderingForTest scoped_ua_enabled(true);

  std::unique_ptr<DummyPageHolder> dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(0, 0), nullptr);
  const ComputedStyle* initial = &ComputedStyle::InitialStyle();

  ColorSchemeHelper color_scheme_helper(dummy_page_holder_->GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);
  StyleResolverState state(dummy_page_holder_->GetDocument(),
                           *dummy_page_holder_->GetDocument().documentElement(),
                           initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
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

TEST(ComputedStyleTest, ApplyInternalLightDarkBackgroundImage) {
  using css_test_helpers::ParseDeclarationBlock;

  ScopedCSSColorSchemeForTest scoped_property_enabled(true);
  ScopedCSSColorSchemeUARenderingForTest scoped_ua_enabled(true);

  std::unique_ptr<DummyPageHolder> dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(0, 0), nullptr);
  const ComputedStyle* initial = &ComputedStyle::InitialStyle();

  ColorSchemeHelper color_scheme_helper(dummy_page_holder_->GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);
  StyleResolverState state(dummy_page_holder_->GetDocument(),
                           *dummy_page_holder_->GetDocument().documentElement(),
                           initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  state.SetStyle(style);

  auto* bgimage_declaration = ParseDeclarationBlock(
      "background-image:-internal-light-dark(none, url(dummy.png))",
      kUASheetMode);
  auto* dark_declaration = ParseDeclarationBlock("color-scheme:dark");
  auto* light_declaration = ParseDeclarationBlock("color-scheme:light");

  EXPECT_FALSE(style->HasNonInheritedLightDarkValue());

  StyleCascade cascade1(state);
  cascade1.MutableMatchResult().AddMatchedProperties(bgimage_declaration);
  cascade1.MutableMatchResult().AddMatchedProperties(dark_declaration);
  cascade1.Apply();
  EXPECT_TRUE(style->HasBackgroundImage());
  EXPECT_TRUE(style->HasNonInheritedLightDarkValue());

  style = ComputedStyle::Create();
  state.SetStyle(style);

  StyleCascade cascade2(state);
  cascade2.MutableMatchResult().AddMatchedProperties(bgimage_declaration);
  cascade2.MutableMatchResult().AddMatchedProperties(light_declaration);
  cascade2.Apply();
  EXPECT_FALSE(style->HasBackgroundImage());
  EXPECT_TRUE(style->HasNonInheritedLightDarkValue());
}

TEST(ComputedStyleTest, StrokeWidthZoomAndCalc) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(0, 0), nullptr);

  const ComputedStyle* initial = &ComputedStyle::InitialStyle();

  StyleResolverState state(dummy_page_holder_->GetDocument(),
                           *dummy_page_holder_->GetDocument().documentElement(),
                           initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetEffectiveZoom(1.5);
  state.SetStyle(style);

  auto* calc_value =
      CSSMathFunctionValue::Create(CSSMathExpressionNumericLiteral::Create(
          CSSNumericLiteralValue::Create(10,
                                         CSSPrimitiveValue::UnitType::kNumber),
          true));

  To<Longhand>(GetCSSPropertyStrokeWidth()).ApplyValue(state, *calc_value);
  auto* computed_value =
      To<Longhand>(GetCSSPropertyStrokeWidth())
          .CSSValueFromComputedStyleInternal(*style, style->SvgStyle(),
                                             nullptr /* layout_object */,
                                             false /* allow_visited_style */);
  ASSERT_TRUE(computed_value);
  auto* numeric_value = DynamicTo<CSSNumericLiteralValue>(computed_value);
  ASSERT_TRUE(numeric_value);
  EXPECT_TRUE(numeric_value->IsPx());
  EXPECT_EQ(10, numeric_value->DoubleValue());
}

TEST(ComputedStyleTest, InitialVariableNamesEmpty) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  EXPECT_TRUE(style->GetVariableNames().IsEmpty());
}

TEST(ComputedStyleTest, InitialVariableNames) {
  using css_test_helpers::CreateLengthRegistration;

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
  registry->RegisterProperty("--x", *CreateLengthRegistration("--x", 1));
  registry->RegisterProperty("--y", *CreateLengthRegistration("--y", 2));
  style->SetInitialData(StyleInitialData::Create(*registry));

  EXPECT_EQ(2u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--y"));
}

TEST(ComputedStyleTest, InheritedVariableNames) {
  using css_test_helpers::CreateVariableData;

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  const bool inherited = true;
  style->SetVariableData("--a", CreateVariableData("foo"), inherited);
  style->SetVariableData("--b", CreateVariableData("bar"), inherited);

  EXPECT_EQ(2u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
}

TEST(ComputedStyleTest, NonInheritedVariableNames) {
  using css_test_helpers::CreateVariableData;

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  const bool inherited = true;
  style->SetVariableData("--a", CreateVariableData("foo"), !inherited);
  style->SetVariableData("--b", CreateVariableData("bar"), !inherited);

  EXPECT_EQ(2u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
}

TEST(ComputedStyleTest, InheritedAndNonInheritedVariableNames) {
  using css_test_helpers::CreateVariableData;

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

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

TEST(ComputedStyleTest, InitialAndInheritedAndNonInheritedVariableNames) {
  using css_test_helpers::CreateLengthRegistration;
  using css_test_helpers::CreateVariableData;

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
  registry->RegisterProperty("--b", *CreateLengthRegistration("--b", 1));
  registry->RegisterProperty("--e", *CreateLengthRegistration("--e", 2));
  style->SetInitialData(StyleInitialData::Create(*registry));

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

TEST(ComputedStyleTest, BorderWidthZoom) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(0, 0), nullptr);

  const ComputedStyle* initial = &ComputedStyle::InitialStyle();

  StyleResolverState state(dummy_page_holder_->GetDocument(),
                           *dummy_page_holder_->GetDocument().documentElement(),
                           initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
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
          *style, style->SvgStyle(), nullptr /* layout_object */,
          false /* allow_visited_style */);
      AtomicString prop_name = longhand.GetCSSPropertyName().ToAtomicString();
      ASSERT_TRUE(computed_value) << prop_name;
      auto* numeric_value = DynamicTo<CSSNumericLiteralValue>(computed_value);
      ASSERT_TRUE(numeric_value) << prop_name;
      EXPECT_TRUE(numeric_value->IsPx()) << prop_name;
      EXPECT_EQ(test.expected_px, numeric_value->DoubleValue()) << prop_name;
    }
  }
}

TEST(ComputedStyleTest, TextDecorationEqualDoesNotRequireRecomputeInkOverflow) {
  using css_test_helpers::ParseDeclarationBlock;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(0, 0), nullptr);
  const ComputedStyle* initial = &ComputedStyle::InitialStyle();

  StyleResolverState state(dummy_page_holder_->GetDocument(),
                           *dummy_page_holder_->GetDocument().documentElement(),
                           initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  // Set up the initial text decoration properties
  style->SetTextDecorationStyle(ETextDecorationStyle::kSolid);
  style->SetTextDecorationColor(StyleColor(CSSValueID::kGreen));
  style->SetTextDecoration(TextDecoration::kUnderline);
  style->SetTextDecorationThickness(
      TextDecorationThickness(Length(5, Length::Type::kFixed)));
  style->SetTextUnderlineOffset(Length(2, Length::Type::kFixed));
  style->SetTextUnderlinePosition(kTextUnderlinePositionUnder);
  state.SetStyle(style);
  StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
  EXPECT_EQ(TextDecoration::kUnderline, style->TextDecorationsInEffect());

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

TEST(ComputedStyleTest, TextDecorationNotEqualRequiresRecomputeInkOverflow) {
  using css_test_helpers::ParseDeclarationBlock;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(0, 0), nullptr);
  const ComputedStyle* initial = &ComputedStyle::InitialStyle();

  StyleResolverState state(dummy_page_holder_->GetDocument(),
                           *dummy_page_holder_->GetDocument().documentElement(),
                           initial, initial);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  // Set up the initial text decoration properties
  style->SetTextDecorationStyle(ETextDecorationStyle::kSolid);
  style->SetTextDecorationColor(StyleColor(CSSValueID::kGreen));
  style->SetTextDecoration(TextDecoration::kUnderline);
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
  other->SetTextDecoration(TextDecoration::kOverline);
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

}  // namespace blink
