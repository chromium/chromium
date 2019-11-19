// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/computed_style.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/style/clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_value.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

TEST(ComputedStyleTest, ShapeOutsideBoxEqual) {
  ShapeValue* shape1 = ShapeValue::CreateBoxShapeValue(CSSBoxType::kContent);
  ShapeValue* shape2 = ShapeValue::CreateBoxShapeValue(CSSBoxType::kContent);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetShapeOutside(shape1);
  style2->SetShapeOutside(shape2);
  EXPECT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ShapeOutsideCircleEqual) {
  scoped_refptr<BasicShapeCircle> circle1 = BasicShapeCircle::Create();
  scoped_refptr<BasicShapeCircle> circle2 = BasicShapeCircle::Create();
  ShapeValue* shape1 =
      ShapeValue::CreateShapeValue(circle1, CSSBoxType::kContent);
  ShapeValue* shape2 =
      ShapeValue::CreateShapeValue(circle2, CSSBoxType::kContent);
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
  style->SetEffectiveZoom(3.5);
  style->SetOutlineStyle(EBorderStyle::kSolid);
#if defined(OS_MACOSX)
  EXPECT_EQ(3, style->GetOutlineStrokeWidthForFocusRing());
#else
  style->SetOutlineStyleIsAuto(static_cast<bool>(OutlineIsAuto::kOn));
  static uint16_t outline_width = 4;
  style->SetOutlineWidth(outline_width);

  double expected_width =
      LayoutTheme::GetTheme().IsFocusRingOutset() ? outline_width : 3.5;
  EXPECT_EQ(expected_width, style->GetOutlineStrokeWidthForFocusRing());

  expected_width =
      LayoutTheme::GetTheme().IsFocusRingOutset() ? outline_width : 1.0;
  style->SetEffectiveZoom(0.5);
  EXPECT_EQ(expected_width, style->GetOutlineStrokeWidthForFocusRing());
#endif
}

TEST(ComputedStyleTest, FocusRingOutset) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetOutlineStyle(EBorderStyle::kSolid);
  style->SetOutlineStyleIsAuto(static_cast<bool>(OutlineIsAuto::kOn));
  style->SetEffectiveZoom(4.75);
#if defined(OS_MACOSX)
  EXPECT_EQ(4, style->OutlineOutsetExtent());
#else
  EXPECT_EQ(3, style->OutlineOutsetExtent());
#endif
}

TEST(ComputedStyleTest, SVGStackingContext) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->UpdateIsStackingContext(false, false, true);
  EXPECT_TRUE(style->IsStackingContext());
}

TEST(ComputedStyleTest, Preserve3dForceStackingContext) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  style->SetOverflowX(EOverflow::kHidden);
  style->SetOverflowY(EOverflow::kHidden);
  style->UpdateIsStackingContext(false, false, false);
  EXPECT_EQ(ETransformStyle3D::kFlat, style->UsedTransformStyle3D());
  EXPECT_TRUE(style->IsStackingContext());
}

TEST(ComputedStyleTest, LayoutContainmentStackingContext) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  EXPECT_FALSE(style->IsStackingContext());
  style->SetContain(kContainsLayout);
  style->UpdateIsStackingContext(false, false, false);
  EXPECT_TRUE(style->IsStackingContext());
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

  TransformOperations operations(true);
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
  Persistent<Document> document = MakeGarbageCollected<Document>();
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
  auto* document = MakeGarbageCollected<Document>();
  css_test_helpers::RegisterProperty(*document, "--x", "<length>", "0px",
                                     false);

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
  auto* document = MakeGarbageCollected<Document>();
  css_test_helpers::RegisterProperty(*document, "--x", "<length>", "0px",
                                     false);

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

TEST(ComputedStyleTest, ApplyColorSchemeLightOnDark) {
  ScopedCSSColorSchemeForTest scoped_property_enabled(true);

  std::unique_ptr<DummyPageHolder> dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(0, 0), nullptr);
  const ComputedStyle* initial = &ComputedStyle::InitialStyle();

  ColorSchemeHelper color_scheme_helper;
  color_scheme_helper.SetPreferredColorScheme(dummy_page_holder_->GetDocument(),
                                              PreferredColorScheme::kDark);
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
  EXPECT_EQ(WebColorScheme::kDark, style->UsedColorScheme());

  To<Longhand>(ref.GetProperty()).ApplyValue(state, *light_value);
  EXPECT_EQ(WebColorScheme::kLight, style->UsedColorScheme());
}

TEST(ComputedStyleTest, ApplyInternalLightDarkColor) {
  ScopedCSSColorSchemeForTest scoped_property_enabled(true);

  std::unique_ptr<DummyPageHolder> dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(0, 0), nullptr);
  const ComputedStyle* initial = &ComputedStyle::InitialStyle();

  auto* ua_context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);
  const CSSValue* internal_light_dark = CSSParser::ParseSingleValue(
      CSSPropertyID::kColor, "-internal-light-dark-color(black, white)",
      ua_context);

  ColorSchemeHelper color_scheme_helper;
  color_scheme_helper.SetPreferredColorScheme(dummy_page_holder_->GetDocument(),
                                              PreferredColorScheme::kDark);
  StyleResolverState state(dummy_page_holder_->GetDocument(),
                           *dummy_page_holder_->GetDocument().documentElement(),
                           initial, initial);

  StyleResolver& resolver =
      dummy_page_holder_->GetDocument().EnsureStyleResolver();

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  state.SetStyle(style);

  CSSValueList* dark_value = CSSValueList::CreateSpaceSeparated();
  dark_value->Append(*CSSIdentifierValue::Create(CSSValueID::kDark));

  CSSValueList* light_value = CSSValueList::CreateSpaceSeparated();
  light_value->Append(*CSSIdentifierValue::Create(CSSValueID::kLight));

  CSSPropertyRef scheme_property("color-scheme", state.GetDocument());
  CSSPropertyRef color_property("color", state.GetDocument());

  To<Longhand>(color_property.GetProperty())
      .ApplyValue(state, *internal_light_dark);
  To<Longhand>(scheme_property.GetProperty()).ApplyValue(state, *dark_value);
  if (!RuntimeEnabledFeatures::CSSCascadeEnabled())
    resolver.ApplyCascadedColorValue(state);
  EXPECT_EQ(Color::kWhite, style->VisitedDependentColor(GetCSSPropertyColor()));

  To<Longhand>(color_property.GetProperty())
      .ApplyValue(state, *internal_light_dark);
  To<Longhand>(scheme_property.GetProperty()).ApplyValue(state, *light_value);
  if (!RuntimeEnabledFeatures::CSSCascadeEnabled())
    resolver.ApplyCascadedColorValue(state);
  EXPECT_EQ(Color::kBlack, style->VisitedDependentColor(GetCSSPropertyColor()));
}

}  // namespace blink
