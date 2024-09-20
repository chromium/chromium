// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/computed_style.h"

#include "base/memory/values_equivalent.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_dynamic_range_limit_mix_value.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_repeat_style_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
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
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_value.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_initial_data.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"
#include "ui/base/ui_base_features.h"

namespace blink {

class ComputedStyleTest : public testing::Test {
 protected:
  void SetUp() override {
    dummy_page_holder_ =
        std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
    initial_style_ = ComputedStyle::GetInitialStyleSingleton();
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  const ComputedStyle* InitialComputedStyle() { return initial_style_; }

  ComputedStyleBuilder CreateComputedStyleBuilder() {
    return ComputedStyleBuilder(*initial_style_);
  }

  ComputedStyleBuilder CreateComputedStyleBuilderFrom(
      const ComputedStyle& style) {
    return ComputedStyleBuilder(style);
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<const ComputedStyle> initial_style_;
};

TEST_F(ComputedStyleTest, ShapeOutsideBoxEqual) {
  auto* shape1 = MakeGarbageCollected<ShapeValue>(CSSBoxType::kContent);
  auto* shape2 = MakeGarbageCollected<ShapeValue>(CSSBoxType::kContent);
  ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
  ComputedStyleBuilder builder2 = CreateComputedStyleBuilder();
  builder1.SetShapeOutside(shape1);
  builder2.SetShapeOutside(shape2);
  EXPECT_EQ(*builder1.TakeStyle(), *builder2.TakeStyle());
}

TEST_F(ComputedStyleTest, ShapeOutsideCircleEqual) {
  scoped_refptr<BasicShapeCircle> circle1 = BasicShapeCircle::Create();
  scoped_refptr<BasicShapeCircle> circle2 = BasicShapeCircle::Create();
  auto* shape1 = MakeGarbageCollected<ShapeValue>(std::move(circle1),
                                                  CSSBoxType::kContent);
  auto* shape2 = MakeGarbageCollected<ShapeValue>(std::move(circle2),
                                                  CSSBoxType::kContent);
  ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
  ComputedStyleBuilder builder2 = CreateComputedStyleBuilder();
  builder1.SetShapeOutside(shape1);
  builder2.SetShapeOutside(shape2);
  EXPECT_EQ(*builder1.TakeStyle(), *builder2.TakeStyle());
}

TEST_F(ComputedStyleTest, ClipPathEqual) {
  scoped_refptr<BasicShapeCircle> shape = BasicShapeCircle::Create();
  ShapeClipPathOperation* path1 = MakeGarbageCollected<ShapeClipPathOperation>(
      shape, GeometryBox::kBorderBox);
  ShapeClipPathOperation* path2 = MakeGarbageCollected<ShapeClipPathOperation>(
      shape, GeometryBox::kBorderBox);
  ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
  ComputedStyleBuilder builder2 = CreateComputedStyleBuilder();
  builder1.SetClipPath(path1);
  builder2.SetClipPath(path2);
  EXPECT_EQ(*builder1.TakeStyle(), *builder2.TakeStyle());
}

TEST_F(ComputedStyleTest, ForcesStackingContext) {
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetForcesStackingContext(true);
  const ComputedStyle* style = builder.TakeStyle();
  EXPECT_TRUE(style->IsStackingContextWithoutContainment());
}

TEST_F(ComputedStyleTest, Preserve3dForceStackingContext) {
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  builder.SetOverflowX(EOverflow::kHidden);
  builder.SetOverflowY(EOverflow::kHidden);
  const ComputedStyle* style = builder.TakeStyle();
  EXPECT_EQ(ETransformStyle3D::kFlat, style->UsedTransformStyle3D());
  EXPECT_TRUE(style->IsStackingContextWithoutContainment());
}

TEST_F(ComputedStyleTest, LayoutContainmentStackingContext) {
  const ComputedStyle* style = InitialComputedStyle();
  EXPECT_FALSE(style->IsStackingContextWithoutContainment());

  ComputedStyleBuilder builder(*style);
  builder.SetContain(kContainsLayout);
  style = builder.TakeStyle();
  // Containment doesn't change IsStackingContextWithoutContainment
  EXPECT_FALSE(style->IsStackingContextWithoutContainment());
}

TEST_F(ComputedStyleTest, IsStackingContextWithoutContainmentAfterClone) {
  ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
  builder1.SetForcesStackingContext(true);
  const ComputedStyle* style1 = builder1.TakeStyle();
  EXPECT_TRUE(style1->IsStackingContextWithoutContainment());

  ComputedStyleBuilder builder2(*style1);
  const ComputedStyle* style2 = builder2.TakeStyle();
  EXPECT_TRUE(style2->IsStackingContextWithoutContainment());

  // Verify that the cached value for IsStackingContextWithoutContainment
  // isn't copied from `style1`.
  ComputedStyleBuilder builder3(*style1);
  builder3.SetForcesStackingContext(false);
  const ComputedStyle* style3 = builder3.TakeStyle();
  EXPECT_FALSE(style3->IsStackingContextWithoutContainment());
}

TEST_F(ComputedStyleTest, DerivedFlagCopyNonInherited) {
  {
    ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
    builder1.SetForcesStackingContext(true);
    const ComputedStyle* style1 = builder1.TakeStyle();
    EXPECT_TRUE(style1->IsStackingContextWithoutContainment());

    // Whether the style is a stacking context or not should not be copied
    // from the style we're cloning.
    ComputedStyleBuilder builder2 = CreateComputedStyleBuilderFrom(*style1);
    const ComputedStyle* style2 = builder2.TakeStyle();
    EXPECT_TRUE(style2->IsStackingContextWithoutContainment());
  }

  // The same as above, except that IsStackingContextWithoutContainment is
  // expected to be false.
  {
    ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
    const ComputedStyle* style1 = builder1.TakeStyle();
    EXPECT_FALSE(style1->IsStackingContextWithoutContainment());

    ComputedStyleBuilder builder2 = CreateComputedStyleBuilderFrom(*style1);
    const ComputedStyle* style2 = builder2.TakeStyle();
    EXPECT_FALSE(style2->IsStackingContextWithoutContainment());
  }

  // The same as the first case, except builder2 sets
  // SetForcesStackingContext(false) after cloning.
  {
    ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
    builder1.SetForcesStackingContext(true);
    const ComputedStyle* style1 = builder1.TakeStyle();
    EXPECT_TRUE(style1->IsStackingContextWithoutContainment());

    ComputedStyleBuilder builder2 = CreateComputedStyleBuilderFrom(*style1);
    builder2.SetForcesStackingContext(false);
    const ComputedStyle* style2 = builder2.TakeStyle();
    // Value copied from 'style1' must not persist.
    EXPECT_FALSE(style2->IsStackingContextWithoutContainment());
  }
}

TEST_F(ComputedStyleTest, TrackedPseudoStyle) {
  for (uint8_t pseudo_id_int = kFirstPublicPseudoId;
       pseudo_id_int <= kLastTrackedPublicPseudoId; pseudo_id_int++) {
    PseudoId pseudo_id = static_cast<PseudoId>(pseudo_id_int);
    MatchResult match_result;
    match_result.SetHasPseudoElementStyle(pseudo_id);

    ComputedStyleBuilder builder = CreateComputedStyleBuilder();
    builder.SetPseudoElementStyles(match_result.PseudoElementStyles());
    const ComputedStyle* style = builder.TakeStyle();

    EXPECT_TRUE(style->HasPseudoElementStyle(pseudo_id));
    EXPECT_TRUE(style->HasAnyPseudoElementStyles());
  }
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesRespectsTransformAnimation) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetHasCurrentTransformAnimation(true);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsTransform) {
  TransformOperations operations;
  // An operation is necessary since having either a non-empty transform list
  // or a transform animation will set HasTransform();
  operations.Operations().push_back(
      MakeGarbageCollected<ScaleTransformOperation>(
          1, 1, TransformOperation::kScale));

  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetTransform(operations);
  const ComputedStyle* style = builder.TakeStyle();

  builder = ComputedStyleBuilder(*style);
  builder.SetHasCurrentTransformAnimation(true);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_FALSE(diff.TransformChanged());
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesRespectsScaleAnimation) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetHasCurrentScaleAnimation(true);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesRespectsRotateAnimation) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetHasCurrentRotateAnimation(true);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesRespectsTranslateAnimation) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetHasCurrentTranslateAnimation(true);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsOpacity) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetHasCurrentOpacityAnimation(true);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsFilter) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetHasCurrentFilterAnimation(true);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsBackdropFilter) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetHasCurrentBackdropFilterAnimation(true);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsBackfaceVisibility) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetBackfaceVisibility(EBackfaceVisibility::kHidden);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsWillChange) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetBackfaceVisibility(EBackfaceVisibility::kHidden);
  builder.SetWillChangeProperties({CSSPropertyID::kOpacity});
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsUsedStylePreserve3D) {
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  const ComputedStyle* style = builder.TakeStyle();

  builder = ComputedStyleBuilder(*style);
  // This induces a flat used transform style.
  builder.SetOpacity(0.5);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsOverflow) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  builder.SetOverflowX(EOverflow::kHidden);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest,
       UpdatePropertySpecificDifferencesCompositingReasonsContainsPaint) {
  const ComputedStyle* style = InitialComputedStyle();
  ComputedStyleBuilder builder(*style);
  // This induces a flat used transform style.
  builder.SetContain(kContainsPaint);
  const ComputedStyle* other = builder.TakeStyle();

  StyleDifference diff = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST_F(ComputedStyleTest, HasOutlineWithCurrentColor) {
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  const ComputedStyle* style = builder.TakeStyle();
  EXPECT_FALSE(style->HasOutline());
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());

  builder = CreateComputedStyleBuilder();
  builder.SetOutlineColor(StyleColor::CurrentColor());
  builder.SetOutlineWidth(5);
  style = builder.TakeStyle();
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());

  builder = CreateComputedStyleBuilder();
  builder.SetOutlineColor(StyleColor::CurrentColor());
  builder.SetOutlineWidth(5);
  builder.SetOutlineStyle(EBorderStyle::kSolid);
  style = builder.TakeStyle();
  EXPECT_TRUE(style->HasOutlineWithCurrentColor());
}

TEST_F(ComputedStyleTest, BorderWidth) {
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetBorderBottomWidth(5);
  const ComputedStyle* style = builder.TakeStyle();
  EXPECT_EQ(style->BorderBottomWidth(), 0);

  builder = ComputedStyleBuilder(*style);
  builder.SetBorderBottomStyle(EBorderStyle::kSolid);
  style = builder.TakeStyle();
  EXPECT_EQ(style->BorderBottomWidth(), 5);
}

TEST_F(ComputedStyleTest, CursorList) {
  auto* gradient = MakeGarbageCollected<cssvalue::CSSLinearGradientValue>(
      nullptr, nullptr, nullptr, nullptr, nullptr, cssvalue::kRepeating);

  auto* image_value = MakeGarbageCollected<StyleGeneratedImage>(
      *gradient, StyleGeneratedImage::ContainerSizes());
  auto* other_image_value = MakeGarbageCollected<StyleGeneratedImage>(
      *gradient, StyleGeneratedImage::ContainerSizes());

  EXPECT_TRUE(base::ValuesEquivalent(image_value, other_image_value));

  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.AddCursor(image_value, false);
  const ComputedStyle* style = builder.TakeStyle();

  builder = CreateComputedStyleBuilder();
  builder.AddCursor(other_image_value, false);
  const ComputedStyle* other = builder.TakeStyle();
  EXPECT_EQ(*style, *other);
}

#define UPDATE_STYLE(style_object, setter, value)      \
  {                                                    \
    ComputedStyleBuilder style_builder(*style_object); \
    style_builder.setter(value);                       \
    style_object = style_builder.TakeStyle();          \
  }

TEST_F(ComputedStyleTest, BorderStyle) {
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetBorderLeftStyle(EBorderStyle::kSolid);
  builder.SetBorderTopStyle(EBorderStyle::kSolid);
  builder.SetBorderRightStyle(EBorderStyle::kSolid);
  builder.SetBorderBottomStyle(EBorderStyle::kSolid);
  const ComputedStyle* style = builder.CloneStyle();
  const ComputedStyle* other = builder.TakeStyle();
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  UPDATE_STYLE(style, SetBorderLeftWidth, 1);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(other, SetBorderLeftWidth, 1);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  UPDATE_STYLE(style, SetBorderTopWidth, 1);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(other, SetBorderTopWidth, 1);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  UPDATE_STYLE(style, SetBorderRightWidth, 1);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(other, SetBorderRightWidth, 1);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  UPDATE_STYLE(style, SetBorderBottomWidth, 1);
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(other, SetBorderBottomWidth, 1);
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  UPDATE_STYLE(style, SetBorderLeftStyle, EBorderStyle::kHidden);
  EXPECT_EQ(LayoutUnit(), style->BorderLeftWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(style, SetBorderLeftStyle, EBorderStyle::kNone);
  EXPECT_EQ(LayoutUnit(), style->BorderLeftWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(style, SetBorderLeftStyle, EBorderStyle::kSolid);
  EXPECT_EQ(LayoutUnit(1), style->BorderLeftWidth());
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  UPDATE_STYLE(style, SetBorderTopStyle, EBorderStyle::kHidden);
  EXPECT_EQ(LayoutUnit(), style->BorderTopWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(style, SetBorderTopStyle, EBorderStyle::kNone);
  EXPECT_EQ(LayoutUnit(), style->BorderTopWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(style, SetBorderTopStyle, EBorderStyle::kSolid);
  EXPECT_EQ(LayoutUnit(1), style->BorderTopWidth());
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  UPDATE_STYLE(style, SetBorderRightStyle, EBorderStyle::kHidden);
  EXPECT_EQ(LayoutUnit(), style->BorderRightWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(style, SetBorderRightStyle, EBorderStyle::kNone);
  EXPECT_EQ(LayoutUnit(), style->BorderRightWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(style, SetBorderRightStyle, EBorderStyle::kSolid);
  EXPECT_EQ(LayoutUnit(1), style->BorderRightWidth());
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  UPDATE_STYLE(style, SetBorderBottomStyle, EBorderStyle::kHidden);
  EXPECT_EQ(LayoutUnit(), style->BorderBottomWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(style, SetBorderBottomStyle, EBorderStyle::kNone);
  EXPECT_EQ(LayoutUnit(), style->BorderBottomWidth());
  EXPECT_FALSE(style->BorderSizeEquals(*other));
  UPDATE_STYLE(style, SetBorderBottomStyle, EBorderStyle::kSolid);
  EXPECT_EQ(LayoutUnit(1), style->BorderBottomWidth());
  EXPECT_TRUE(style->BorderSizeEquals(*other));

  EXPECT_TRUE(style->HasBorder());
  UPDATE_STYLE(style, SetBorderTopStyle, EBorderStyle::kHidden);
  EXPECT_TRUE(style->HasBorder());
  UPDATE_STYLE(style, SetBorderRightStyle, EBorderStyle::kHidden);
  EXPECT_TRUE(style->HasBorder());
  UPDATE_STYLE(style, SetBorderBottomStyle, EBorderStyle::kHidden);
  EXPECT_TRUE(style->HasBorder());
  UPDATE_STYLE(style, SetBorderLeftStyle, EBorderStyle::kHidden);
  EXPECT_FALSE(style->HasBorder());

  UPDATE_STYLE(style, SetBorderTopStyle, EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasBorder());
  UPDATE_STYLE(style, SetBorderRightStyle, EBorderStyle::kSolid);
  UPDATE_STYLE(style, SetBorderBottomStyle, EBorderStyle::kSolid);
  UPDATE_STYLE(style, SetBorderLeftStyle, EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasBorder());

  UPDATE_STYLE(style, SetBorderTopStyle, EBorderStyle::kNone);
  EXPECT_TRUE(style->HasBorder());
  UPDATE_STYLE(style, SetBorderRightStyle, EBorderStyle::kNone);
  EXPECT_TRUE(style->HasBorder());
  UPDATE_STYLE(style, SetBorderBottomStyle, EBorderStyle::kNone);
  EXPECT_TRUE(style->HasBorder());
  UPDATE_STYLE(style, SetBorderLeftStyle, EBorderStyle::kNone);
  EXPECT_FALSE(style->HasBorder());
}

#define TEST_ANIMATION_FLAG(flag, inherited)                     \
  do {                                                           \
    auto builder = CreateComputedStyleBuilder();                 \
    builder.Set##flag(true);                                     \
    const auto* style = builder.TakeStyle();                     \
    EXPECT_TRUE(style->flag());                                  \
    const auto* other = InitialComputedStyle();                  \
    EXPECT_FALSE(other->flag());                                 \
    EXPECT_EQ(ComputedStyle::Difference::inherited,              \
              ComputedStyle::ComputeDifference(style, other));   \
    auto diff = style->VisualInvalidationDiff(document, *other); \
    EXPECT_TRUE(diff.HasDifference());                           \
    EXPECT_TRUE(diff.CompositingReasonsChanged());               \
  } while (false)

#define TEST_ANIMATION_FLAG_NO_DIFF(flag)                        \
  do {                                                           \
    auto builder = CreateComputedStyleBuilder();                 \
    builder.Set##flag(true);                                     \
    const auto* style = builder.TakeStyle();                     \
    EXPECT_TRUE(style->flag());                                  \
    const auto* other = InitialComputedStyle();                  \
    EXPECT_FALSE(other->flag());                                 \
    EXPECT_EQ(ComputedStyle::Difference::kEqual,                 \
              ComputedStyle::ComputeDifference(style, other));   \
    auto diff = style->VisualInvalidationDiff(document, *other); \
    EXPECT_FALSE(diff.HasDifference());                          \
    EXPECT_FALSE(diff.CompositingReasonsChanged());              \
  } while (false)

TEST_F(ComputedStyleTest, AnimationFlags) {
  Document& document = GetDocument();
  TEST_ANIMATION_FLAG(HasCurrentTransformAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentScaleAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentRotateAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentTranslateAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentOpacityAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentFilterAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(HasCurrentBackdropFilterAnimation, kNonInherited);
  TEST_ANIMATION_FLAG(SubtreeWillChangeContents, kNonInherited);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningTransformAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningScaleAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningRotateAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningTranslateAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningOpacityAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningFilterAnimationOnCompositor);
  TEST_ANIMATION_FLAG_NO_DIFF(IsRunningBackdropFilterAnimationOnCompositor);
}

TEST_F(ComputedStyleTest, CustomPropertiesEqual_Values) {
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "0px",
                                     false);

  using UnitType = CSSPrimitiveValue::UnitType;

  const auto* value1 = CSSNumericLiteralValue::Create(1.0, UnitType::kPixels);
  const auto* value2 = CSSNumericLiteralValue::Create(2.0, UnitType::kPixels);
  const auto* value3 = CSSNumericLiteralValue::Create(1.0, UnitType::kPixels);

  Vector<AtomicString> properties;
  properties.push_back("--x");

  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetVariableValue(AtomicString("--x"), value1, false);
  const ComputedStyle* style1 = builder.TakeStyle();

  builder = CreateComputedStyleBuilder();
  builder.SetVariableValue(AtomicString("--x"), value1, false);
  const ComputedStyle* style2 = builder.TakeStyle();
  EXPECT_TRUE(style1->CustomPropertiesEqual(properties, *style2));

  builder = CreateComputedStyleBuilder();
  builder.SetVariableValue(AtomicString("--x"), value3, false);
  style2 = builder.TakeStyle();
  EXPECT_TRUE(style1->CustomPropertiesEqual(properties, *style2));

  builder = CreateComputedStyleBuilder();
  builder.SetVariableValue(AtomicString("--x"), value2, false);
  style2 = builder.TakeStyle();
  EXPECT_FALSE(style1->CustomPropertiesEqual(properties, *style2));
}

TEST_F(ComputedStyleTest, CustomPropertiesEqual_Data) {
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "0px",
                                     false);

  const ComputedStyle* style1;
  const ComputedStyle* style2;

  auto* value1 = css_test_helpers::CreateVariableData("foo");
  auto* value2 = css_test_helpers::CreateVariableData("bar");
  auto* value3 = css_test_helpers::CreateVariableData("foo");

  Vector<AtomicString> properties;
  properties.push_back("--x");

  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetVariableData(AtomicString("--x"), value1, false);
  style1 = builder.TakeStyle();

  builder = CreateComputedStyleBuilder();
  builder.SetVariableData(AtomicString("--x"), value1, false);
  style2 = builder.TakeStyle();
  EXPECT_TRUE(style1->CustomPropertiesEqual(properties, *style2));

  builder = CreateComputedStyleBuilder();
  builder.SetVariableData(AtomicString("--x"), value3, false);
  style2 = builder.TakeStyle();
  EXPECT_TRUE(style1->CustomPropertiesEqual(properties, *style2));

  builder = CreateComputedStyleBuilder();
  builder.SetVariableData(AtomicString("--x"), value2, false);
  style2 = builder.TakeStyle();
  EXPECT_FALSE(style1->CustomPropertiesEqual(properties, *style2));
}

TEST_F(ComputedStyleTest, CustomPropertiesInheritance_FastPath) {
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "0px",
                                     true);

  ComputedStyleBuilder old_builder = CreateComputedStyleBuilder();
  ComputedStyleBuilder new_builder = CreateComputedStyleBuilder();

  using UnitType = CSSPrimitiveValue::UnitType;

  const auto* value1 = CSSNumericLiteralValue::Create(1.0, UnitType::kPixels);
  const auto* value2 = CSSNumericLiteralValue::Create(2.0, UnitType::kPixels);

  const ComputedStyle* old_style = old_builder.TakeStyle();
  const ComputedStyle* new_style = new_builder.TakeStyle();
  EXPECT_FALSE(old_style->HasVariableDeclaration());
  EXPECT_FALSE(old_style->HasVariableReference());
  EXPECT_FALSE(new_style->HasVariableReference());
  EXPECT_FALSE(new_style->HasVariableDeclaration());

  // Removed variable
  old_builder = CreateComputedStyleBuilder();
  old_builder.SetVariableValue(AtomicString("--x"), value1, true);
  old_style = old_builder.TakeStyle();
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style, new_style));

  old_builder = CreateComputedStyleBuilder();
  new_builder = CreateComputedStyleBuilder();

  // Added a new variable
  new_builder.SetVariableValue(AtomicString("--x"), value2, true);
  old_style = old_builder.TakeStyle();
  new_style = new_builder.TakeStyle();
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style, new_style));

  // Change value of variable
  old_builder = CreateComputedStyleBuilder();
  new_builder = ComputedStyleBuilder(*new_style);
  old_builder.SetVariableValue(AtomicString("--x"), value1, true);
  new_builder.SetVariableValue(AtomicString("--x"), value2, true);
  new_builder.SetHasVariableReference();
  old_style = old_builder.TakeStyle();
  new_style = new_builder.TakeStyle();
  EXPECT_FALSE(new_style->HasVariableDeclaration());
  EXPECT_TRUE(new_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style, new_style));

  old_builder = CreateComputedStyleBuilder();
  new_builder = CreateComputedStyleBuilder();

  // New styles with variable declaration don't force style recalc
  old_builder.SetVariableValue(AtomicString("--x"), value1, true);
  new_builder.SetVariableValue(AtomicString("--x"), value2, true);
  new_builder.SetHasVariableDeclaration();
  old_style = old_builder.TakeStyle();
  new_style = new_builder.TakeStyle();
  EXPECT_TRUE(new_style->HasVariableDeclaration());
  EXPECT_FALSE(new_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style, new_style));

  old_builder = CreateComputedStyleBuilder();
  new_builder = CreateComputedStyleBuilder();

  // New styles with variable reference don't force style recalc
  old_builder.SetVariableValue(AtomicString("--x"), value1, true);
  new_builder.SetVariableValue(AtomicString("--x"), value2, true);
  new_builder.SetHasVariableDeclaration();
  new_builder.SetHasVariableReference();
  old_style = old_builder.TakeStyle();
  new_style = new_builder.TakeStyle();
  EXPECT_TRUE(new_style->HasVariableDeclaration());
  EXPECT_TRUE(new_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kIndependentInherited,
            ComputedStyle::ComputeDifference(old_style, new_style));
}

TEST_F(ComputedStyleTest, CustomPropertiesInheritance_StyleRecalc) {
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "0px",
                                     true);

  ComputedStyleBuilder old_builder = CreateComputedStyleBuilder();
  ComputedStyleBuilder new_builder = CreateComputedStyleBuilder();

  using UnitType = CSSPrimitiveValue::UnitType;

  const auto* value1 = CSSNumericLiteralValue::Create(1.0, UnitType::kPixels);
  const auto* value2 = CSSNumericLiteralValue::Create(2.0, UnitType::kPixels);

  const ComputedStyle* old_style = old_builder.TakeStyle();
  const ComputedStyle* new_style = new_builder.TakeStyle();
  EXPECT_FALSE(old_style->HasVariableDeclaration());
  EXPECT_FALSE(old_style->HasVariableReference());
  EXPECT_FALSE(new_style->HasVariableReference());
  EXPECT_FALSE(new_style->HasVariableDeclaration());

  // Removed variable value
  // Old styles with variable reference force style recalc
  old_builder = CreateComputedStyleBuilder();
  old_builder.SetHasVariableReference();
  old_builder.SetVariableValue(AtomicString("--x"), value2, true);
  old_style = old_builder.TakeStyle();
  EXPECT_TRUE(old_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style, new_style));

  old_builder = CreateComputedStyleBuilder();
  new_builder = CreateComputedStyleBuilder();

  // New variable value
  // Old styles with variable declaration force style recalc
  old_builder.SetHasVariableDeclaration();
  new_builder.SetVariableValue(AtomicString("--x"), value2, true);
  old_style = old_builder.TakeStyle();
  new_style = new_builder.TakeStyle();
  EXPECT_TRUE(old_style->HasVariableDeclaration());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style, new_style));

  old_builder = CreateComputedStyleBuilder();
  new_builder = CreateComputedStyleBuilder();

  // Change variable value
  // Old styles with variable declaration force style recalc
  old_builder.SetVariableValue(AtomicString("--x"), value1, true);
  new_builder.SetVariableValue(AtomicString("--x"), value2, true);
  old_builder.SetHasVariableDeclaration();
  old_style = old_builder.TakeStyle();
  new_style = new_builder.TakeStyle();
  EXPECT_TRUE(old_style->HasVariableDeclaration());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style, new_style));

  old_builder = CreateComputedStyleBuilder();
  new_builder = CreateComputedStyleBuilder();

  // Change variable value
  // Old styles with variable reference force style recalc
  old_builder.SetVariableValue(AtomicString("--x"), value1, true);
  new_builder.SetVariableValue(AtomicString("--x"), value2, true);
  old_builder.SetHasVariableReference();
  old_style = old_builder.TakeStyle();
  new_style = new_builder.TakeStyle();
  EXPECT_TRUE(old_style->HasVariableReference());
  EXPECT_EQ(ComputedStyle::Difference::kInherited,
            ComputedStyle::ComputeDifference(old_style, new_style));
}

TEST_F(ComputedStyleTest, ApplyColorSchemeLightOnDark) {
  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  ColorSchemeHelper color_scheme_helper(document);
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  CSSPropertyRef ref("color-scheme", state.GetDocument());

  CSSValueList* dark_value = CSSValueList::CreateSpaceSeparated();
  dark_value->Append(*CSSIdentifierValue::Create(CSSValueID::kDark));

  CSSValueList* light_value = CSSValueList::CreateSpaceSeparated();
  light_value->Append(*CSSIdentifierValue::Create(CSSValueID::kLight));

  To<Longhand>(ref.GetProperty())
      .ApplyValue(state, *dark_value, CSSProperty::ValueMode::kNormal);
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            state.StyleBuilder().UsedColorScheme());

  To<Longhand>(ref.GetProperty())
      .ApplyValue(state, *light_value, CSSProperty::ValueMode::kNormal);
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            state.StyleBuilder().UsedColorScheme());
}

TEST_F(ComputedStyleTest, ApplyLightDarkColor) {
  using css_test_helpers::ParseDeclarationBlock;

  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  ColorSchemeHelper color_scheme_helper(document);
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  CSSValueList* dark_value = CSSValueList::CreateSpaceSeparated();
  dark_value->Append(*CSSIdentifierValue::Create(CSSValueID::kDark));

  CSSValueList* light_value = CSSValueList::CreateSpaceSeparated();
  light_value->Append(*CSSIdentifierValue::Create(CSSValueID::kLight));

  auto* color_declaration = ParseDeclarationBlock(
      "color:light-dark(black, white)", CSSParserMode::kUASheetMode);
  auto* dark_declaration = ParseDeclarationBlock("color-scheme:dark");
  auto* light_declaration = ParseDeclarationBlock("color-scheme:light");

  StyleCascade cascade1(state);
  cascade1.MutableMatchResult().BeginAddingAuthorRulesForTreeScope(document);
  cascade1.MutableMatchResult().AddMatchedProperties(
      color_declaration, {.origin = CascadeOrigin::kAuthor});
  cascade1.MutableMatchResult().AddMatchedProperties(
      dark_declaration, {.origin = CascadeOrigin::kAuthor});
  cascade1.Apply();
  const ComputedStyle* style = state.StyleBuilder().CloneStyle();
  EXPECT_EQ(Color::kWhite, style->VisitedDependentColor(GetCSSPropertyColor()));

  StyleCascade cascade2(state);
  cascade2.MutableMatchResult().BeginAddingAuthorRulesForTreeScope(document);
  cascade2.MutableMatchResult().AddMatchedProperties(
      color_declaration, {.origin = CascadeOrigin::kAuthor});
  cascade2.MutableMatchResult().AddMatchedProperties(
      light_declaration, {.origin = CascadeOrigin::kAuthor});
  cascade2.Apply();
  style = state.StyleBuilder().CloneStyle();
  EXPECT_EQ(Color::kBlack, style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(ComputedStyleTest, ApplyLightDarkBackgroundImage) {
  using css_test_helpers::ParseDeclarationBlock;

  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  ColorSchemeHelper color_scheme_helper(document);
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  auto* bgimage_declaration = ParseDeclarationBlock(
      "background-image:light-dark(none, url(dummy.png))", kUASheetMode);
  auto* dark_declaration = ParseDeclarationBlock("color-scheme:dark");
  auto* light_declaration = ParseDeclarationBlock("color-scheme:light");

  StyleCascade cascade1(state);
  cascade1.MutableMatchResult().BeginAddingAuthorRulesForTreeScope(document);
  cascade1.MutableMatchResult().AddMatchedProperties(
      bgimage_declaration, {.origin = CascadeOrigin::kAuthor});
  cascade1.MutableMatchResult().AddMatchedProperties(
      dark_declaration, {.origin = CascadeOrigin::kAuthor});
  cascade1.Apply();
  EXPECT_TRUE(state.TakeStyle()->HasBackgroundImage());

  state.SetStyle(*initial);

  StyleCascade cascade2(state);
  cascade2.MutableMatchResult().BeginAddingAuthorRulesForTreeScope(document);
  cascade2.MutableMatchResult().AddMatchedProperties(
      bgimage_declaration, {.origin = CascadeOrigin::kAuthor});
  cascade2.MutableMatchResult().AddMatchedProperties(
      light_declaration, {.origin = CascadeOrigin::kAuthor});
  cascade2.Apply();
  EXPECT_FALSE(state.TakeStyle()->HasBackgroundImage());
}

TEST_F(ComputedStyleTest, StrokeWidthZoomAndCalc) {
  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);
  state.StyleBuilder().SetEffectiveZoom(1.5);

  auto* calc_value = CSSMathFunctionValue::Create(
      CSSMathExpressionNumericLiteral::Create(CSSNumericLiteralValue::Create(
          10, CSSPrimitiveValue::UnitType::kNumber)));

  GetCSSPropertyStrokeWidth().ApplyValue(state, *calc_value,
                                         CSSProperty::ValueMode::kNormal);
  const ComputedStyle* style = state.TakeStyle();
  auto* computed_value =
      GetCSSPropertyStrokeWidth().CSSValueFromComputedStyleInternal(
          *style, nullptr /* layout_object */, false /* allow_visited_style */,
          CSSValuePhase::kComputedValue);
  ASSERT_TRUE(computed_value);
  ASSERT_EQ("calc(10px)", computed_value->CssText());
}

TEST_F(ComputedStyleTest, InitialVariableNamesEmpty) {
  const ComputedStyle* style = InitialComputedStyle();
  EXPECT_TRUE(style->GetVariableNames().empty());
}

TEST_F(ComputedStyleTest, InitialVariableNames) {
  using css_test_helpers::CreateLengthRegistration;

  PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
  registry->RegisterProperty(AtomicString("--x"),
                             *CreateLengthRegistration("--x", 1));
  registry->RegisterProperty(AtomicString("--y"),
                             *CreateLengthRegistration("--y", 2));

  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetInitialData(
      MakeGarbageCollected<StyleInitialData>(GetDocument(), *registry));
  const ComputedStyle* style = builder.TakeStyle();

  EXPECT_EQ(2u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--y"));
}

TEST_F(ComputedStyleTest, InheritedVariableNames) {
  using css_test_helpers::CreateVariableData;

  const bool inherited = true;
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetVariableData(AtomicString("--a"), CreateVariableData("foo"),
                          inherited);
  builder.SetVariableData(AtomicString("--b"), CreateVariableData("bar"),
                          inherited);
  const ComputedStyle* style = builder.TakeStyle();

  EXPECT_EQ(2u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
}

TEST_F(ComputedStyleTest, NonInheritedVariableNames) {
  using css_test_helpers::CreateVariableData;

  const bool inherited = true;
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetVariableData(AtomicString("--a"), CreateVariableData("foo"),
                          !inherited);
  builder.SetVariableData(AtomicString("--b"), CreateVariableData("bar"),
                          !inherited);
  const ComputedStyle* style = builder.TakeStyle();

  EXPECT_EQ(2u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
}

TEST_F(ComputedStyleTest, InheritedAndNonInheritedVariableNames) {
  using css_test_helpers::CreateVariableData;

  const bool inherited = true;
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetVariableData(AtomicString("--a"), CreateVariableData("foo"),
                          inherited);
  builder.SetVariableData(AtomicString("--b"), CreateVariableData("bar"),
                          inherited);
  builder.SetVariableData(AtomicString("--d"), CreateVariableData("foz"),
                          !inherited);
  builder.SetVariableData(AtomicString("--c"), CreateVariableData("baz"),
                          !inherited);
  const ComputedStyle* style = builder.TakeStyle();

  EXPECT_EQ(4u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--c"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--d"));
}

TEST_F(ComputedStyleTest, InitialAndInheritedAndNonInheritedVariableNames) {
  using css_test_helpers::CreateLengthRegistration;
  using css_test_helpers::CreateVariableData;

  PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
  registry->RegisterProperty(AtomicString("--b"),
                             *CreateLengthRegistration("--b", 1));
  registry->RegisterProperty(AtomicString("--e"),
                             *CreateLengthRegistration("--e", 2));

  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetInitialData(
      MakeGarbageCollected<StyleInitialData>(GetDocument(), *registry));

  const bool inherited = true;
  builder.SetVariableData(AtomicString("--a"), CreateVariableData("foo"),
                          inherited);
  builder.SetVariableData(AtomicString("--b"), CreateVariableData("bar"),
                          inherited);
  builder.SetVariableData(AtomicString("--d"), CreateVariableData("foz"),
                          !inherited);
  builder.SetVariableData(AtomicString("--c"), CreateVariableData("baz"),
                          !inherited);
  const ComputedStyle* style = builder.TakeStyle();

  EXPECT_EQ(5u, style->GetVariableNames().size());
  EXPECT_TRUE(style->GetVariableNames().Contains("--a"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--b"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--c"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--d"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--e"));
}

TEST_F(ComputedStyleTest, GetVariableNamesCount_Invalidation) {
  const ComputedStyle* style = InitialComputedStyle();
  EXPECT_EQ(style->GetVariableNamesCount(), 0u);

  auto* data = css_test_helpers::CreateVariableData("foo");
  ComputedStyleBuilder builder(*style);
  builder.SetVariableData(AtomicString("--x"), data, false);
  style = builder.TakeStyle();
  EXPECT_EQ(style->GetVariableNamesCount(), 1u);

  builder = ComputedStyleBuilder(*style);
  builder.SetVariableData(AtomicString("--y"), data, false);
  style = builder.TakeStyle();
  EXPECT_EQ(style->GetVariableNamesCount(), 2u);

  builder = ComputedStyleBuilder(*style);
  builder.SetVariableData(AtomicString("--z"), data, true);
  style = builder.TakeStyle();
  EXPECT_EQ(style->GetVariableNamesCount(), 3u);
}

TEST_F(ComputedStyleTest, GetVariableNames_Invalidation) {
  const ComputedStyle* style;

  auto* data = css_test_helpers::CreateVariableData("foo");
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetVariableData(AtomicString("--x"), data, false);
  style = builder.TakeStyle();
  EXPECT_EQ(style->GetVariableNames().size(), 1u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));

  builder = ComputedStyleBuilder(*style);
  builder.SetVariableData(AtomicString("--y"), data, false);
  style = builder.TakeStyle();
  EXPECT_EQ(style->GetVariableNames().size(), 2u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--y"));

  builder = ComputedStyleBuilder(*style);
  builder.SetVariableData(AtomicString("--z"), data, true);
  style = builder.TakeStyle();
  EXPECT_EQ(style->GetVariableNames().size(), 3u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--y"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--z"));
}

TEST_F(ComputedStyleTest, GetVariableNamesWithInitialData_Invalidation) {
  using css_test_helpers::CreateLengthRegistration;

  const ComputedStyle* style;

  {
    ComputedStyleBuilder builder = CreateComputedStyleBuilder();
    PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
    registry->RegisterProperty(AtomicString("--x"),
                               *CreateLengthRegistration("--x", 1));
    builder.SetInitialData(
        MakeGarbageCollected<StyleInitialData>(GetDocument(), *registry));
    style = builder.TakeStyle();
  }
  EXPECT_EQ(style->GetVariableNames().size(), 1u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--x"));

  // Not set StyleInitialData to something else.
  {
    ComputedStyleBuilder builder(*style);
    PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
    registry->RegisterProperty(AtomicString("--y"),
                               *CreateLengthRegistration("--y", 2));
    registry->RegisterProperty(AtomicString("--z"),
                               *CreateLengthRegistration("--z", 3));
    builder.SetInitialData(
        MakeGarbageCollected<StyleInitialData>(GetDocument(), *registry));
    style = builder.TakeStyle();
  }
  EXPECT_EQ(style->GetVariableNames().size(), 2u);
  EXPECT_TRUE(style->GetVariableNames().Contains("--y"));
  EXPECT_TRUE(style->GetVariableNames().Contains("--z"));
}

TEST_F(ComputedStyleTest, BorderWidthZoom) {
  Document& document = GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <style>
      div {
        border-top-style: solid;
        column-rule-style: solid;
        outline-style: solid;
        border-top-width: var(--x);
        column-rule-width: var(--x);
        outline-width: var(--x);
        zoom: 2;
      }
      #thin { --x: thin; }
      #medium { --x: medium; }
      #thick { --x: thick; }
    </style>
    <div id="thin"></div>
    <div id="medium"></div>
    <div id="thick"></div>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();

  const struct {
    const ComputedStyle* style;
    double expected_px;
    STACK_ALLOCATED();
  } tests[] = {
      {document.getElementById(AtomicString("thin"))->GetComputedStyle(), 1.0},
      {document.getElementById(AtomicString("medium"))->GetComputedStyle(),
       3.0},
      {document.getElementById(AtomicString("thick"))->GetComputedStyle(), 5.0},
  };

  for (const auto& test : tests) {
    for (const Longhand* property :
         {static_cast<const Longhand*>(&GetCSSPropertyBorderTopWidth()),
          static_cast<const Longhand*>(&GetCSSPropertyOutlineWidth()),
          static_cast<const Longhand*>(&GetCSSPropertyColumnRuleWidth())}) {
      const Longhand& longhand = To<Longhand>(*property);
      auto* computed_value = longhand.CSSValueFromComputedStyleInternal(
          *test.style, nullptr /* layout_object */,
          false /* allow_visited_style */, CSSValuePhase::kComputedValue);
      AtomicString prop_name = longhand.GetCSSPropertyName().ToAtomicString();
      ASSERT_TRUE(computed_value) << prop_name;
      auto* numeric_value = DynamicTo<CSSNumericLiteralValue>(computed_value);
      ASSERT_TRUE(numeric_value) << prop_name;
      EXPECT_TRUE(numeric_value->IsPx()) << prop_name;
      EXPECT_EQ(test.expected_px, numeric_value->DoubleValue()) << prop_name;
    }
  }
}

TEST_F(ComputedStyleTest, BorderWidthConversion) {
  // Tests that Border, Outline and Column Rule Widths
  // are converted as expected.

  Document& document = GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <style>
      div {
        border-top-style: solid;
        column-rule-style: solid;
        outline-style: solid;
        border-top-width: var(--x);
        column-rule-width: var(--x);
        outline-width: var(--x);
      }
      #t1 { --x: 0px; }
      #t2 { --x: 0.1px; }
      #t3 { --x: 0.5px; }
      #t4 { --x: 0.9px; }
      #t5 { --x: 1.0px; }
      #t6 { --x: 3.0px; }
      #t7 { --x: 3.3px; }
      #t8 { --x: 3.5px; }
      #t9 { --x: 3.9px; }
      #t10 { --x: 3.999px; }
    </style>
    <div id="t1"></div>
    <div id="t2"></div>
    <div id="t3"></div>
    <div id="t4"></div>
    <div id="t5"></div>
    <div id="t6"></div>
    <div id="t7"></div>
    <div id="t8"></div>
    <div id="t9"></div>
    <div id="t10"></div>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();

  const struct {
    const ComputedStyle* style;
    double expected_px;
    STACK_ALLOCATED();
  } tests[] = {
      {document.getElementById(AtomicString("t1"))->GetComputedStyle(), 0.0},
      {document.getElementById(AtomicString("t2"))->GetComputedStyle(), 1.0},
      {document.getElementById(AtomicString("t3"))->GetComputedStyle(), 1.0},
      {document.getElementById(AtomicString("t4"))->GetComputedStyle(), 1.0},
      {document.getElementById(AtomicString("t5"))->GetComputedStyle(), 1.0},
      {document.getElementById(AtomicString("t6"))->GetComputedStyle(), 3.0},
      {document.getElementById(AtomicString("t7"))->GetComputedStyle(), 3.0},
      {document.getElementById(AtomicString("t8"))->GetComputedStyle(), 3.0},
      {document.getElementById(AtomicString("t9"))->GetComputedStyle(), 3.0},
      {document.getElementById(AtomicString("t10"))->GetComputedStyle(), 3.0},
  };

  for (const auto& test : tests) {
    for (const Longhand* longhand :
         {static_cast<const Longhand*>(&GetCSSPropertyBorderTopWidth()),
          static_cast<const Longhand*>(&GetCSSPropertyOutlineWidth()),
          static_cast<const Longhand*>(&GetCSSPropertyColumnRuleWidth())}) {
      auto* computed_value = longhand->CSSValueFromComputedStyleInternal(
          *test.style, nullptr /* layout_object */,
          false /* allow_visited_style */, CSSValuePhase::kComputedValue);
      ASSERT_NE(computed_value, nullptr);
      auto* numeric_value = DynamicTo<CSSNumericLiteralValue>(computed_value);
      ASSERT_NE(numeric_value, nullptr);
      EXPECT_TRUE(numeric_value->IsPx());
      EXPECT_DOUBLE_EQ(test.expected_px, numeric_value->DoubleValue());
    }
  }
}

TEST_F(ComputedStyleTest, BorderWidthConversionWithZoom) {
  // Tests that Border Widths
  // are converted as expected when Zoom is applied.

  Document& document = GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <style>
      div {
        border-top-style: solid;
        border-top-width: var(--x);
        zoom: 2;
      }
      #t1 { --x: thin; }
      #t2 { --x: medium; }
      #t3 { --x: thick; }
      #t4 { --x: 0px; }
      #t5 { --x: 0.1px; }
      #t6 { --x: 0.5px; }
      #t7 { --x: 0.9px; }
      #t8 { --x: 1.0px; }
      #t9 { --x: 1.5px; }
      #t10 { --x: 3.0px; }
      #t11 { --x: 3.3px; }
      #t12 { --x: 3.5px; }
      #t13 { --x: 3.9px; }
    </style>
    <div id="t1"></div>
    <div id="t2"></div>
    <div id="t3"></div>
    <div id="t4"></div>
    <div id="t5"></div>
    <div id="t6"></div>
    <div id="t7"></div>
    <div id="t8"></div>
    <div id="t9"></div>
    <div id="t10"></div>
    <div id="t11"></div>
    <div id="t12"></div>
    <div id="t13"></div>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();

  const struct {
    const ComputedStyle* style;
    int expected_px;
    STACK_ALLOCATED();
  } tests[] = {
      {document.getElementById(AtomicString("t1"))->GetComputedStyle(), 2},
      {document.getElementById(AtomicString("t2"))->GetComputedStyle(), 6},
      {document.getElementById(AtomicString("t3"))->GetComputedStyle(), 10},
      {document.getElementById(AtomicString("t4"))->GetComputedStyle(), 0},
      {document.getElementById(AtomicString("t5"))->GetComputedStyle(), 1},
      {document.getElementById(AtomicString("t6"))->GetComputedStyle(), 1},
      {document.getElementById(AtomicString("t7"))->GetComputedStyle(), 1},
      {document.getElementById(AtomicString("t8"))->GetComputedStyle(), 2},
      {document.getElementById(AtomicString("t9"))->GetComputedStyle(), 3},
      {document.getElementById(AtomicString("t10"))->GetComputedStyle(), 6},
      {document.getElementById(AtomicString("t11"))->GetComputedStyle(), 6},
      {document.getElementById(AtomicString("t12"))->GetComputedStyle(), 7},
      {document.getElementById(AtomicString("t13"))->GetComputedStyle(), 7},
  };

  for (const auto& test : tests) {
    auto width = test.style->BorderTopWidth();
    EXPECT_EQ(test.expected_px, width);
  }
}

TEST_F(ComputedStyleTest,
       TextDecorationEqualDoesNotRequireRecomputeInkOverflow) {
  using css_test_helpers::ParseDeclarationBlock;

  Document& document = GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <style>
      div {
        text-decoration: underline solid green 5px;
        text-underline-offset: 2px;
        text-underline-position: under;
      }
    </style>
    <div id="style"></div>
    <div id="clone"></div>
    <div id="other" style="text-decoration-color: blue;"></div>
  )HTML",
                                ASSERT_NO_EXCEPTION);
  document.View()->UpdateAllLifecyclePhasesForTest();

  const ComputedStyle* style =
      document.getElementById(AtomicString("style"))->GetComputedStyle();
  const ComputedStyle* clone =
      document.getElementById(AtomicString("clone"))->GetComputedStyle();
  const ComputedStyle* other =
      document.getElementById(AtomicString("other"))->GetComputedStyle();

  EXPECT_EQ(TextDecorationLine::kUnderline, style->TextDecorationsInEffect());

  StyleDifference diff1 = style->VisualInvalidationDiff(GetDocument(), *clone);
  EXPECT_FALSE(diff1.NeedsRecomputeVisualOverflow());

  // Different color, should not invalidate.
  StyleDifference diff2 = style->VisualInvalidationDiff(GetDocument(), *other);
  EXPECT_FALSE(diff2.NeedsRecomputeVisualOverflow());
}

TEST_F(ComputedStyleTest, TextDecorationNotEqualRequiresRecomputeInkOverflow) {
  using css_test_helpers::ParseDeclarationBlock;

  Document& document = GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <style>
      div {
        text-decoration: underline solid green 5px;
        text-underline-offset: 2px;
        text-underline-position: under;
      }
    </style>
    <div id="style"></div>
    <div id="wavy" style="text-decoration-style: wavy;"></div>
    <div id="overline" style="text-decoration-line: overline;"></div>
    <div id="thickness" style="text-decoration-thickness: 3px;"></div>
    <div id="offset" style="text-underline-offset: 4px;"></div>
    <div id="position" style="text-underline-position: left;"></div>
  )HTML",
                                ASSERT_NO_EXCEPTION);
  document.View()->UpdateAllLifecyclePhasesForTest();

  const ComputedStyle* style =
      document.getElementById(AtomicString("style"))->GetComputedStyle();
  const ComputedStyle* wavy =
      document.getElementById(AtomicString("wavy"))->GetComputedStyle();
  const ComputedStyle* overline =
      document.getElementById(AtomicString("overline"))->GetComputedStyle();
  const ComputedStyle* thickness =
      document.getElementById(AtomicString("thickness"))->GetComputedStyle();
  const ComputedStyle* offset =
      document.getElementById(AtomicString("offset"))->GetComputedStyle();
  const ComputedStyle* position =
      document.getElementById(AtomicString("position"))->GetComputedStyle();

  // Change decoration style
  StyleDifference diff_decoration_style =
      style->VisualInvalidationDiff(GetDocument(), *wavy);
  EXPECT_TRUE(diff_decoration_style.NeedsRecomputeVisualOverflow());

  // Change decoration line
  StyleDifference diff_decoration_line =
      style->VisualInvalidationDiff(GetDocument(), *overline);
  EXPECT_TRUE(diff_decoration_line.NeedsRecomputeVisualOverflow());

  // Change decoration thickness
  StyleDifference diff_decoration_thickness =
      style->VisualInvalidationDiff(GetDocument(), *thickness);
  EXPECT_TRUE(diff_decoration_thickness.NeedsRecomputeVisualOverflow());

  // Change underline offset
  StyleDifference diff_underline_offset =
      style->VisualInvalidationDiff(GetDocument(), *offset);
  EXPECT_TRUE(diff_underline_offset.NeedsRecomputeVisualOverflow());

  // Change underline position
  StyleDifference diff_underline_position =
      style->VisualInvalidationDiff(GetDocument(), *position);
  EXPECT_TRUE(diff_underline_position.NeedsRecomputeVisualOverflow());
}

// Verify that cloned ComputedStyle is independent from source, i.e.
// copy-on-write works as expected.
TEST_F(ComputedStyleTest, ClonedStyleAnimationsAreIndependent) {
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();

  auto& animations = builder.AccessAnimations();
  animations.DelayStartList().clear();
  animations.DelayStartList().push_back(CSSAnimationData::InitialDelayStart());
  const ComputedStyle* style = builder.TakeStyle();
  EXPECT_EQ(1u, style->Animations()->DelayStartList().size());

  builder = ComputedStyleBuilder(*style);
  auto& cloned_style_animations = builder.AccessAnimations();
  EXPECT_EQ(1u, cloned_style_animations.DelayStartList().size());
  cloned_style_animations.DelayStartList().push_back(
      CSSAnimationData::InitialDelayStart());
  const ComputedStyle* cloned_style = builder.TakeStyle();

  EXPECT_EQ(2u, cloned_style->Animations()->DelayStartList().size());
  EXPECT_EQ(1u, style->Animations()->DelayStartList().size());
}

TEST_F(ComputedStyleTest, ClonedStyleTransitionsAreIndependent) {
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();

  auto& transitions = builder.AccessTransitions();
  transitions.PropertyList().clear();
  transitions.PropertyList().push_back(CSSTransitionData::InitialProperty());
  const ComputedStyle* style = builder.TakeStyle();
  EXPECT_EQ(1u, style->Transitions()->PropertyList().size());

  builder = ComputedStyleBuilder(*style);
  auto& cloned_style_transitions = builder.AccessTransitions();
  EXPECT_EQ(1u, cloned_style_transitions.PropertyList().size());
  cloned_style_transitions.PropertyList().push_back(
      CSSTransitionData::InitialProperty());
  const ComputedStyle* cloned_style = builder.TakeStyle();

  EXPECT_EQ(2u, cloned_style->Transitions()->PropertyList().size());
  EXPECT_EQ(1u, style->Transitions()->PropertyList().size());
}

TEST_F(ComputedStyleTest, ApplyInitialAnimationNameAndTransitionProperty) {
  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);
  EXPECT_FALSE(state.StyleBuilder().Animations());
  EXPECT_FALSE(state.StyleBuilder().Transitions());

  GetCSSPropertyAnimationName().ApplyInitial(state);
  GetCSSPropertyTransitionProperty().ApplyInitial(state);
  EXPECT_FALSE(state.StyleBuilder().Animations());
  EXPECT_FALSE(state.StyleBuilder().Transitions());
}

#define TEST_STYLE_VALUE_NO_DIFF(field_name)                       \
  {                                                                \
    ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();  \
    ComputedStyleBuilder builder2 = CreateComputedStyleBuilder();  \
    builder1.Set##field_name(                                      \
        ComputedStyleInitialValues::Initial##field_name());        \
    builder2.Set##field_name(                                      \
        ComputedStyleInitialValues::Initial##field_name());        \
    const ComputedStyle* style1 = builder1.TakeStyle();            \
    const ComputedStyle* style2 = builder2.TakeStyle();            \
    auto diff = style1->VisualInvalidationDiff(document, *style2); \
    EXPECT_FALSE(diff.HasDifference());                            \
  }

// Ensures ref-counted values are compared by their values, not by pointers.
#define TEST_STYLE_REFCOUNTED_VALUE_NO_DIFF(type, field_name)              \
  {                                                                        \
    ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();          \
    ComputedStyleBuilder builder2 = CreateComputedStyleBuilder();          \
    scoped_refptr<type> value1 = base::MakeRefCounted<type>();             \
    scoped_refptr<type> value2 = base::MakeRefCounted<type>(value1->data); \
    builder1.Set##field_name(value1);                                      \
    builder2.Set##field_name(value2);                                      \
    const ComputedStyle* style1 = builder1.TakeStyle();                    \
    const ComputedStyle* style2 = builder2.TakeStyle();                    \
    auto diff = style1->VisualInvalidationDiff(document, *style2);         \
    EXPECT_FALSE(diff.HasDifference());                                    \
  }

TEST_F(ComputedStyleTest, SvgStrokeStyleShouldCompareValue) {
  Document& document = GetDocument();
  TEST_STYLE_VALUE_NO_DIFF(StrokeOpacity);
  TEST_STYLE_VALUE_NO_DIFF(StrokeMiterLimit);
  TEST_STYLE_VALUE_NO_DIFF(StrokeWidth);
  TEST_STYLE_VALUE_NO_DIFF(StrokeDashOffset);
  TEST_STYLE_REFCOUNTED_VALUE_NO_DIFF(SVGDashArray, StrokeDashArray);

  TEST_STYLE_VALUE_NO_DIFF(StrokePaint);
  TEST_STYLE_VALUE_NO_DIFF(InternalVisitedStrokePaint);
}

TEST_F(ComputedStyleTest, SvgMiscStyleShouldCompareValue) {
  Document& document = GetDocument();
  TEST_STYLE_VALUE_NO_DIFF(FloodColor);
  TEST_STYLE_VALUE_NO_DIFF(FloodOpacity);
  TEST_STYLE_VALUE_NO_DIFF(LightingColor);
  TEST_STYLE_VALUE_NO_DIFF(BaselineShift);
}

TEST_F(ComputedStyleTest, ShouldApplyAnyContainment) {
  Document& document = GetDocument();

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
                               CSSValueID::kNone,
                               CSSValueID::kMath};
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

  ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
  ComputedStyleBuilder builder2 = CreateComputedStyleBuilder();

  builder1.SetWidth(Length(100.0, Length::kFixed));
  builder2.SetWidth(Length(200.0, Length::kFixed));

  const ComputedStyle* style1 = builder1.TakeStyle();
  const ComputedStyle* style2 = builder2.TakeStyle();

  EXPECT_EQ(0u, style1->DebugDiffFields(*style1).size());
  EXPECT_EQ(0u, style2->DebugDiffFields(*style2).size());

  EXPECT_EQ(1u, style1->DebugDiffFields(*style2).size());

  // The extra quotes are unfortunate, but comes from operator<< on String.
  EXPECT_EQ(DebugField::width_, style1->DebugDiffFields(*style2)[0].field);
  EXPECT_EQ("\"Length(Fixed, 100)\"",
            style1->DebugDiffFields(*style2)[0].actual);
  EXPECT_EQ("\"Length(Fixed, 200)\"",
            style1->DebugDiffFields(*style2)[0].correct);

  EXPECT_EQ("width_",
            ComputedStyleBase::DebugFieldToString(DebugField::width_));
}

TEST_F(ComputedStyleTest, DerivedDebugDiff) {
  using DebugField = ComputedStyleBase::DebugField;

  ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
  ComputedStyleBuilder builder2 = CreateComputedStyleBuilder();

  builder1.SetForcesStackingContext(true);

  const ComputedStyle* style1 = builder1.TakeStyle();
  const ComputedStyle* style2 = builder2.TakeStyle();

  ASSERT_EQ(2u, style1->DebugDiffFields(*style2).size());

  EXPECT_EQ(DebugField::forces_stacking_context_,
            style1->DebugDiffFields(*style2)[0].field);
  EXPECT_EQ("1", style1->DebugDiffFields(*style2)[0].actual);
  EXPECT_EQ("0", style1->DebugDiffFields(*style2)[0].correct);

  EXPECT_EQ(DebugField::is_stacking_context_without_containment_,
            style1->DebugDiffFields(*style2)[1].field);
  EXPECT_EQ("true", style1->DebugDiffFields(*style2)[1].actual);
  EXPECT_EQ("false", style1->DebugDiffFields(*style2)[1].correct);
}

TEST_F(ComputedStyleTest, DerivedDebugDiffLazy) {
  ComputedStyleBuilder builder1 = CreateComputedStyleBuilder();
  ComputedStyleBuilder builder2 = CreateComputedStyleBuilder();

  const ComputedStyle* style1 = builder1.TakeStyle();
  const ComputedStyle* style2 = builder2.TakeStyle();

  // Trigger lazy-evaluation of the field on *one* of the styles.
  EXPECT_FALSE(style1->IsStackingContextWithoutContainment());

  // We should not detect a difference, because ComputedStyle(Base) should
  // evaluate the field automatically when needed.
  EXPECT_EQ(0u, style1->DebugDiffFields(*style2).size());
}

#endif  // #if DCHECK_IS_ON()

TEST_F(ComputedStyleTest, LogicalScrollPaddingUtils) {
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();

  Length left = Length::Fixed(1.0f);
  Length right = Length::Fixed(2.0f);
  Length top = Length::Fixed(3.0f);
  Length bottom = Length::Fixed(4.0f);

  builder.SetScrollPaddingLeft(left);
  builder.SetScrollPaddingRight(right);
  builder.SetScrollPaddingTop(top);
  builder.SetScrollPaddingBottom(bottom);

  // ltr

  builder.SetDirection(TextDirection::kLtr);
  builder.SetWritingMode(WritingMode::kHorizontalTb);
  const ComputedStyle* style = builder.TakeStyle();
  EXPECT_EQ(left, style->ScrollPaddingInlineStart());
  EXPECT_EQ(right, style->ScrollPaddingInlineEnd());
  EXPECT_EQ(top, style->ScrollPaddingBlockStart());
  EXPECT_EQ(bottom, style->ScrollPaddingBlockEnd());

  builder = ComputedStyleBuilder(*style);
  builder.SetDirection(TextDirection::kLtr);
  builder.SetWritingMode(WritingMode::kVerticalLr);
  style = builder.TakeStyle();
  EXPECT_EQ(top, style->ScrollPaddingInlineStart());
  EXPECT_EQ(bottom, style->ScrollPaddingInlineEnd());
  EXPECT_EQ(left, style->ScrollPaddingBlockStart());
  EXPECT_EQ(right, style->ScrollPaddingBlockEnd());

  builder = ComputedStyleBuilder(*style);
  builder.SetDirection(TextDirection::kLtr);
  builder.SetWritingMode(WritingMode::kVerticalRl);
  style = builder.TakeStyle();
  EXPECT_EQ(top, style->ScrollPaddingInlineStart());
  EXPECT_EQ(bottom, style->ScrollPaddingInlineEnd());
  EXPECT_EQ(right, style->ScrollPaddingBlockStart());
  EXPECT_EQ(left, style->ScrollPaddingBlockEnd());

  // rtl

  builder = ComputedStyleBuilder(*style);
  builder.SetDirection(TextDirection::kRtl);
  builder.SetWritingMode(WritingMode::kHorizontalTb);
  style = builder.TakeStyle();
  EXPECT_EQ(right, style->ScrollPaddingInlineStart());
  EXPECT_EQ(left, style->ScrollPaddingInlineEnd());
  EXPECT_EQ(top, style->ScrollPaddingBlockStart());
  EXPECT_EQ(bottom, style->ScrollPaddingBlockEnd());

  builder = ComputedStyleBuilder(*style);
  builder.SetDirection(TextDirection::kRtl);
  builder.SetWritingMode(WritingMode::kVerticalLr);
  style = builder.TakeStyle();
  EXPECT_EQ(bottom, style->ScrollPaddingInlineStart());
  EXPECT_EQ(top, style->ScrollPaddingInlineEnd());
  EXPECT_EQ(left, style->ScrollPaddingBlockStart());
  EXPECT_EQ(right, style->ScrollPaddingBlockEnd());

  builder = ComputedStyleBuilder(*style);
  builder.SetDirection(TextDirection::kRtl);
  builder.SetWritingMode(WritingMode::kVerticalRl);
  style = builder.TakeStyle();
  EXPECT_EQ(bottom, style->ScrollPaddingInlineStart());
  EXPECT_EQ(top, style->ScrollPaddingInlineEnd());
  EXPECT_EQ(right, style->ScrollPaddingBlockStart());
  EXPECT_EQ(left, style->ScrollPaddingBlockEnd());
}

TEST_F(ComputedStyleTest, BasicBuilder) {
  const ComputedStyle* original = InitialComputedStyle();

  Length left = Length::Fixed(1.0f);
  Length right = Length::Fixed(2.0f);

  ComputedStyleBuilder builder(*original);
  builder.SetScrollPaddingLeft(left);
  builder.SetScrollPaddingRight(right);

  const ComputedStyle* style = builder.TakeStyle();

  EXPECT_NE(left, original->ScrollPaddingLeft());
  EXPECT_NE(right, original->ScrollPaddingRight());

  EXPECT_EQ(left, style->ScrollPaddingLeft());
  EXPECT_EQ(right, style->ScrollPaddingRight());
}

TEST_F(ComputedStyleTest, MoveBuilder) {
  Length one = Length::Fixed(1.0f);

  ComputedStyleBuilder builder1(*InitialComputedStyle());
  builder1.SetScrollPaddingLeft(one);

  ComputedStyleBuilder builder2(std::move(builder1));

  const ComputedStyle* style2 = builder2.TakeStyle();
  ASSERT_TRUE(style2);
  EXPECT_EQ(one, style2->ScrollPaddingLeft());
}

TEST_F(ComputedStyleTest, MoveAssignBuilder) {
  Length one = Length::Fixed(1.0f);

  ComputedStyleBuilder builder1(*InitialComputedStyle());
  builder1.SetScrollPaddingLeft(one);

  ComputedStyleBuilder builder2(*InitialComputedStyle());
  builder2 = std::move(builder1);

  const ComputedStyle* style2 = builder2.TakeStyle();
  ASSERT_TRUE(style2);
  EXPECT_EQ(one, style2->ScrollPaddingLeft());
}

TEST_F(ComputedStyleTest, ScrollTimelineNameNoDiff) {
  ComputedStyleBuilder builder1(*InitialComputedStyle());
  ComputedStyleBuilder builder2(*InitialComputedStyle());

  builder1.SetScrollTimelineName(MakeGarbageCollected<ScopedCSSNameList>(
      HeapVector<Member<const ScopedCSSName>>(
          1u, MakeGarbageCollected<ScopedCSSName>(AtomicString("test"),
                                                  /* tree_scope */ nullptr))));
  builder2.SetScrollTimelineName(MakeGarbageCollected<ScopedCSSNameList>(
      HeapVector<Member<const ScopedCSSName>>(
          1u, MakeGarbageCollected<ScopedCSSName>(AtomicString("test"),
                                                  /* tree_scope */ nullptr))));

  const ComputedStyle* style1 = builder1.TakeStyle();
  const ComputedStyle* style2 = builder2.TakeStyle();

  EXPECT_EQ(ComputedStyle::Difference::kEqual,
            ComputedStyle::ComputeDifference(style1, style2));
}

TEST_F(ComputedStyleTest, ScrollTimelineAxisNoDiff) {
  ComputedStyleBuilder builder1(*InitialComputedStyle());
  ComputedStyleBuilder builder2(*InitialComputedStyle());

  builder1.SetScrollTimelineAxis(Vector<TimelineAxis>(1u, TimelineAxis::kY));
  builder2.SetScrollTimelineAxis(Vector<TimelineAxis>(1u, TimelineAxis::kY));

  const ComputedStyle* style1 = builder1.TakeStyle();
  const ComputedStyle* style2 = builder2.TakeStyle();

  EXPECT_EQ(ComputedStyle::Difference::kEqual,
            ComputedStyle::ComputeDifference(style1, style2));
}

TEST_F(ComputedStyleTest, ViewTimelineNameNoDiff) {
  ComputedStyleBuilder builder1(*InitialComputedStyle());
  ComputedStyleBuilder builder2(*InitialComputedStyle());

  builder1.SetViewTimelineName(MakeGarbageCollected<ScopedCSSNameList>(
      HeapVector<Member<const ScopedCSSName>>(
          1u, MakeGarbageCollected<ScopedCSSName>(AtomicString("test"),
                                                  /* tree_scope */ nullptr))));
  builder2.SetViewTimelineName(MakeGarbageCollected<ScopedCSSNameList>(
      HeapVector<Member<const ScopedCSSName>>(
          1u, MakeGarbageCollected<ScopedCSSName>(AtomicString("test"),
                                                  /* tree_scope */ nullptr))));

  const ComputedStyle* style1 = builder1.TakeStyle();
  const ComputedStyle* style2 = builder2.TakeStyle();

  EXPECT_EQ(ComputedStyle::Difference::kEqual,
            ComputedStyle::ComputeDifference(style1, style2));
}

TEST_F(ComputedStyleTest, ViewTimelineAxisNoDiff) {
  ComputedStyleBuilder builder1(*InitialComputedStyle());
  ComputedStyleBuilder builder2(*InitialComputedStyle());

  builder1.SetViewTimelineAxis(Vector<TimelineAxis>(1u, TimelineAxis::kY));
  builder2.SetViewTimelineAxis(Vector<TimelineAxis>(1u, TimelineAxis::kY));

  const ComputedStyle* style1 = builder1.TakeStyle();
  const ComputedStyle* style2 = builder2.TakeStyle();

  EXPECT_EQ(ComputedStyle::Difference::kEqual,
            ComputedStyle::ComputeDifference(style1, style2));
}

TEST_F(ComputedStyleTest, ViewTimelineInsetNoDiff) {
  ComputedStyleBuilder builder1(*InitialComputedStyle());
  ComputedStyleBuilder builder2(*InitialComputedStyle());

  builder1.SetViewTimelineInset(Vector<TimelineInset>(
      1u, TimelineInset(Length::Fixed(1.0f), Length::Fixed(1.0f))));
  builder2.SetViewTimelineInset(Vector<TimelineInset>(
      1u, TimelineInset(Length::Fixed(1.0f), Length::Fixed(1.0f))));

  const ComputedStyle* style1 = builder1.TakeStyle();
  const ComputedStyle* style2 = builder2.TakeStyle();

  EXPECT_EQ(ComputedStyle::Difference::kEqual,
            ComputedStyle::ComputeDifference(style1, style2));
}

TEST_F(ComputedStyleTest, ContainerNameNoDiff) {
  ComputedStyleBuilder builder1(*InitialComputedStyle());
  ComputedStyleBuilder builder2(*InitialComputedStyle());

  builder1.SetContainerName(MakeGarbageCollected<ScopedCSSNameList>(
      HeapVector<Member<const ScopedCSSName>>(
          1u, MakeGarbageCollected<ScopedCSSName>(AtomicString("test"),
                                                  /* tree_scope */ nullptr))));
  builder1.SetContainerType(kContainerTypeSize);
  builder2.SetContainerName(MakeGarbageCollected<ScopedCSSNameList>(
      HeapVector<Member<const ScopedCSSName>>(
          1u, MakeGarbageCollected<ScopedCSSName>(AtomicString("test"),
                                                  /* tree_scope */ nullptr))));
  builder2.SetContainerType(kContainerTypeSize);

  const ComputedStyle* style1 = builder1.TakeStyle();
  const ComputedStyle* style2 = builder2.TakeStyle();

  EXPECT_EQ(ComputedStyle::Difference::kEqual,
            ComputedStyle::ComputeDifference(style1, style2));
}

TEST_F(ComputedStyleTest, BackgroundRepeat) {
  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  auto* repeat_style_value = MakeGarbageCollected<CSSRepeatStyleValue>(
      CSSIdentifierValue::Create(CSSValueID::kRepeatX));

  GetCSSPropertyBackgroundRepeat().ApplyValue(state, *repeat_style_value,
                                              CSSProperty::ValueMode::kNormal);
  const ComputedStyle* style = state.TakeStyle();
  auto* computed_value =
      GetCSSPropertyBackgroundRepeat().CSSValueFromComputedStyleInternal(
          *style, nullptr /* layout_object */, false /* allow_visited_style */,
          CSSValuePhase::kComputedValue);
  ASSERT_TRUE(computed_value);
  ASSERT_EQ("repeat-x", computed_value->CssText());
}

TEST_F(ComputedStyleTest, MaskRepeat) {
  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  auto* repeat_style_value = MakeGarbageCollected<CSSRepeatStyleValue>(
      CSSIdentifierValue::Create(CSSValueID::kRepeatY));

  GetCSSPropertyMaskRepeat().ApplyValue(state, *repeat_style_value,
                                        CSSProperty::ValueMode::kNormal);
  const ComputedStyle* style = state.TakeStyle();
  auto* computed_value =
      GetCSSPropertyMaskRepeat().CSSValueFromComputedStyleInternal(
          *style, nullptr /* layout_object */, false /* allow_visited_style */,
          CSSValuePhase::kComputedValue);
  ASSERT_TRUE(computed_value);
  ASSERT_EQ("repeat-y", computed_value->CssText());
}

TEST_F(ComputedStyleTest, MaskMode) {
  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  auto* mode_style_value = CSSIdentifierValue::Create(CSSValueID::kAlpha);

  GetCSSPropertyMaskMode().ApplyValue(state, *mode_style_value,
                                      CSSProperty::ValueMode::kNormal);
  const ComputedStyle* style = state.TakeStyle();
  auto* computed_value =
      GetCSSPropertyMaskMode().CSSValueFromComputedStyleInternal(
          *style, nullptr /* layout_object */, false /* allow_visited_style */,
          CSSValuePhase::kComputedValue);
  ASSERT_TRUE(computed_value);
  ASSERT_EQ("alpha", computed_value->CssText());
}

TEST_F(ComputedStyleTest, DynamicRangeLimitMixStandardToConstrainedHigh) {
  const DynamicRangeLimit limit(/*standard_mix=*/0.3f,
                                /*constrained_high_mix=*/0.7f);
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetDynamicRangeLimit(limit);
  auto* dynamic_range_limit_mix_value =
      GetCSSPropertyDynamicRangeLimit().CSSValueFromComputedStyleInternal(
          *builder.TakeStyle(), nullptr /* layout_object */,
          false /* allow_visited_style */, CSSValuePhase::kComputedValue);
  ASSERT_NE(dynamic_range_limit_mix_value, nullptr);

  EXPECT_EQ(dynamic_range_limit_mix_value->CssText(),
            "dynamic-range-limit-mix(standard, constrained-high, 70%)");

  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  GetCSSPropertyDynamicRangeLimit().ApplyValue(
      state, *dynamic_range_limit_mix_value, CSSProperty::ValueMode::kNormal);

  const DynamicRangeLimit converted_limit =
      state.TakeStyle()->GetDynamicRangeLimit();
  EXPECT_FLOAT_EQ(converted_limit.standard_mix, limit.standard_mix);
  EXPECT_FLOAT_EQ(converted_limit.constrained_high_mix,
                  limit.constrained_high_mix);
}

TEST_F(ComputedStyleTest, DynamicRangeLimitMixStandardToHigh) {
  const DynamicRangeLimit limit(/*standard_mix=*/0.4f,
                                /*constrained_high_mix=*/0.f);
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetDynamicRangeLimit(limit);
  auto* dynamic_range_limit_mix_value =
      GetCSSPropertyDynamicRangeLimit().CSSValueFromComputedStyleInternal(
          *builder.TakeStyle(), nullptr /* layout_object */,
          false /* allow_visited_style */, CSSValuePhase::kComputedValue);
  ASSERT_NE(dynamic_range_limit_mix_value, nullptr);

  EXPECT_EQ(dynamic_range_limit_mix_value->CssText(),
            "dynamic-range-limit-mix(standard, high, 60%)");

  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  GetCSSPropertyDynamicRangeLimit().ApplyValue(
      state, *dynamic_range_limit_mix_value, CSSProperty::ValueMode::kNormal);

  const DynamicRangeLimit converted_limit =
      state.TakeStyle()->GetDynamicRangeLimit();
  EXPECT_FLOAT_EQ(converted_limit.standard_mix, limit.standard_mix);
  EXPECT_FLOAT_EQ(converted_limit.constrained_high_mix,
                  limit.constrained_high_mix);
}

TEST_F(ComputedStyleTest, DynamicRangeLimitMixConstrainedHighToHigh) {
  const DynamicRangeLimit limit(/*standard_mix=*/0.f,
                                /*constrained_high_mix=*/0.55f);
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetDynamicRangeLimit(limit);
  auto* dynamic_range_limit_mix_value =
      GetCSSPropertyDynamicRangeLimit().CSSValueFromComputedStyleInternal(
          *builder.TakeStyle(), nullptr /* layout_object */,
          false /* allow_visited_style */, CSSValuePhase::kComputedValue);
  ASSERT_NE(dynamic_range_limit_mix_value, nullptr);

  EXPECT_EQ(dynamic_range_limit_mix_value->CssText(),
            "dynamic-range-limit-mix(constrained-high, high, 45%)");

  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  GetCSSPropertyDynamicRangeLimit().ApplyValue(
      state, *dynamic_range_limit_mix_value, CSSProperty::ValueMode::kNormal);

  const DynamicRangeLimit converted_limit =
      state.TakeStyle()->GetDynamicRangeLimit();
  EXPECT_FLOAT_EQ(converted_limit.standard_mix, limit.standard_mix);
  EXPECT_FLOAT_EQ(converted_limit.constrained_high_mix,
                  limit.constrained_high_mix);
}

TEST_F(ComputedStyleTest, DynamicRangeLimitMixAllThree) {
  const DynamicRangeLimit limit(/*standard_mix=*/0.2f,
                                /*constrained_high_mix=*/0.6f);
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetDynamicRangeLimit(limit);
  auto* dynamic_range_limit_mix_value =
      GetCSSPropertyDynamicRangeLimit().CSSValueFromComputedStyleInternal(
          *builder.TakeStyle(), nullptr /* layout_object */,
          false /* allow_visited_style */, CSSValuePhase::kComputedValue);
  ASSERT_NE(dynamic_range_limit_mix_value, nullptr);

  EXPECT_EQ(dynamic_range_limit_mix_value->CssText(),
            "dynamic-range-limit-mix(standard, "
            "dynamic-range-limit-mix(constrained-high, high, 25%), 80%)");

  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));

  state.SetStyle(*initial);

  GetCSSPropertyDynamicRangeLimit().ApplyValue(
      state, *dynamic_range_limit_mix_value, CSSProperty::ValueMode::kNormal);

  const DynamicRangeLimit converted_limit =
      state.TakeStyle()->GetDynamicRangeLimit();
  EXPECT_FLOAT_EQ(converted_limit.standard_mix, limit.standard_mix);
  EXPECT_FLOAT_EQ(converted_limit.constrained_high_mix,
                  limit.constrained_high_mix);
}

TEST_F(ComputedStyleTest, UseCountInsideListMarkerPositionQuirk) {
  Document& document = GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <style>.marker-content-none::marker { content: none }</style>
    <ul><li></li></ul>
    <ol><li></li></ol>
    <ul><div><li></li></ul>
    <ol><li><li></li></li></ol>
    <div style="display: list-item"></div>
    <li style="list-style-position: inside"></li>
    <li style="list-style: none"></li>
    <li class="marker-content-none"></li>
    <li style="display: flex"></li>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(
      document.IsUseCounted(WebFeature::kInsideListMarkerPositionQuirk));

  document.body()->setInnerHTML("<li></li>");
  document.View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      document.IsUseCounted(WebFeature::kInsideListMarkerPositionQuirk));
}

TEST_F(ComputedStyleTest, ZoomInheritance) {
  Document& document = GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <div id="target" style="line-height: revert; zoom: 2;">Hello, world!</div>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(true) << "Test passes if it doesn't hit a DCHECK.";
}

TEST_F(ComputedStyleTest, ColorSchemeFlagsIsNormal) {
  Document& document = GetDocument();
  ColorSchemeHelper color_scheme_helper(document);
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);

  document.body()->setInnerHTML(R"HTML(
    <div id="normal" style="color-scheme: normal"></div>
    <div id="light" style="color-scheme: light"></div>
    <div id="dark" style="color-scheme: dark"></div>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("normal"))
                  ->ComputedStyleRef()
                  .ColorSchemeFlagsIsNormal());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("light"))
                   ->ComputedStyleRef()
                   .ColorSchemeFlagsIsNormal());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("dark"))
                   ->ComputedStyleRef()
                   .ColorSchemeFlagsIsNormal());
}

TEST_F(ComputedStyleTest, ColorSchemeFlagsIsNormal_WithMeta) {
  Document& document = GetDocument();
  ColorSchemeHelper color_scheme_helper(document);
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);

  document.body()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="light">
    <div id="normal" style="color-scheme: normal"></div>
    <div id="light" style="color-scheme: light"></div>
    <div id="dark" style="color-scheme: dark"></div>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("normal"))
                   ->ComputedStyleRef()
                   .ColorSchemeFlagsIsNormal());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("light"))
                   ->ComputedStyleRef()
                   .ColorSchemeFlagsIsNormal());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("dark"))
                   ->ComputedStyleRef()
                   .ColorSchemeFlagsIsNormal());
}

}  // namespace blink
