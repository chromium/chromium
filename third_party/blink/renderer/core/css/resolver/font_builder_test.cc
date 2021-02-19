// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/font_builder.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class FontBuilderTest {
 public:
  FontBuilderTest()
      : dummy_(std::make_unique<DummyPageHolder>(IntSize(800, 600))) {
    GetSettings().SetDefaultFontSize(16.0f);
  }

  Document& GetDocument() { return dummy_->GetDocument(); }
  Settings& GetSettings() { return *GetDocument().GetSettings(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_;
};

using BuilderFunc = void (*)(FontBuilder&);
using DescriptionFunc = void (*)(FontDescription&);

struct FunctionPair {
  FunctionPair(DescriptionFunc base, BuilderFunc value)
      : set_base_value(base), set_value(value) {}

  DescriptionFunc set_base_value;
  BuilderFunc set_value;
};

class FontBuilderInitTest : public FontBuilderTest, public testing::Test {};
class FontBuilderAdditiveTest : public FontBuilderTest,
                                public testing::TestWithParam<FunctionPair> {};

TEST_F(FontBuilderInitTest, InitialFontSizeNotScaled) {
  scoped_refptr<ComputedStyle> initial = ComputedStyle::Create();

  FontBuilder builder(&GetDocument());
  builder.SetInitial(1.0f);  // FIXME: Remove unused param.
  builder.CreateFont(*initial, initial.get());

  EXPECT_EQ(16.0f, initial->GetFontDescription().ComputedSize());
}

TEST_F(FontBuilderInitTest, NotDirty) {
  FontBuilder builder(&GetDocument());
  ASSERT_FALSE(builder.FontDirty());
}

// This test verifies that when you are setting some field F via FontBuilder,
// only F is actually modified on the incoming
// ComputedStyle::GetFontDescription.
TEST_P(FontBuilderAdditiveTest, OnlySetValueIsModified) {
  FunctionPair funcs = GetParam();

  FontDescription parent_description;
  funcs.set_base_value(parent_description);

  scoped_refptr<ComputedStyle> parent_style = ComputedStyle::Create();
  parent_style->SetFontDescription(parent_description);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->InheritFrom(*parent_style);

  FontBuilder font_builder(&GetDocument());
  funcs.set_value(font_builder);
  font_builder.CreateFont(*style, parent_style.get());

  FontDescription output_description = style->GetFontDescription();

  // FontBuilder should have overwritten our base value set in the parent,
  // hence the descriptions should not be equal.
  ASSERT_NE(parent_description, output_description);

  // Overwrite the value set by FontBuilder with the base value, directly
  // on outputDescription.
  funcs.set_base_value(output_description);

  // Now the descriptions should be equal again. If they are, we know that
  // FontBuilder did not change something it wasn't supposed to.
  ASSERT_EQ(parent_description, output_description);
}

static void FontWeightBase(FontDescription& d) {
  d.SetWeight(FontSelectionValue(900));
}
static void FontWeightValue(FontBuilder& b) {
  b.SetWeight(NormalWeightValue());
}

static void FontStretchBase(FontDescription& d) {
  d.SetStretch(UltraExpandedWidthValue());
}
static void FontStretchValue(FontBuilder& b) {
  b.SetStretch(ExtraCondensedWidthValue());
}

static void FontFamilyBase(FontDescription& d) {
  d.SetGenericFamily(FontDescription::kFantasyFamily);
}
static void FontFamilyValue(FontBuilder& b) {
  b.SetFamilyDescription(
      FontDescription::FamilyDescription(FontDescription::kCursiveFamily));
}

static void FontFeatureSettingsBase(FontDescription& d) {
  d.SetFeatureSettings(nullptr);
}
static void FontFeatureSettingsValue(FontBuilder& b) {
  b.SetFeatureSettings(FontFeatureSettings::Create());
}

static void FontStyleBase(FontDescription& d) {
  d.SetStyle(ItalicSlopeValue());
}
static void FontStyleValue(FontBuilder& b) {
  b.SetStyle(NormalSlopeValue());
}

static void FontVariantCapsBase(FontDescription& d) {
  d.SetVariantCaps(FontDescription::kSmallCaps);
}
static void FontVariantCapsValue(FontBuilder& b) {
  b.SetVariantCaps(FontDescription::kCapsNormal);
}

static void FontVariantLigaturesBase(FontDescription& d) {
  d.SetVariantLigatures(FontDescription::VariantLigatures(
      FontDescription::kEnabledLigaturesState));
}
static void FontVariantLigaturesValue(FontBuilder& b) {
  b.SetVariantLigatures(FontDescription::VariantLigatures(
      FontDescription::kDisabledLigaturesState));
}

static void FontVariantNumericBase(FontDescription& d) {
  d.SetVariantNumeric(FontVariantNumeric());
}
static void FontVariantNumericValue(FontBuilder& b) {
  FontVariantNumeric variant_numeric;
  variant_numeric.SetNumericFraction(FontVariantNumeric::kStackedFractions);
  b.SetVariantNumeric(variant_numeric);
}

static void FontTextRenderingBase(FontDescription& d) {
  d.SetTextRendering(kGeometricPrecision);
}
static void FontTextRenderingValue(FontBuilder& b) {
  b.SetTextRendering(kOptimizeLegibility);
}

static void FontKerningBase(FontDescription& d) {
  d.SetKerning(FontDescription::kNormalKerning);
}
static void FontKerningValue(FontBuilder& b) {
  b.SetKerning(FontDescription::kNoneKerning);
}

static void FontOpticalSizingBase(FontDescription& d) {
  d.SetFontOpticalSizing(kAutoOpticalSizing);
}
static void FontOpticalSizingValue(FontBuilder& b) {
  b.SetFontOpticalSizing(kNoneOpticalSizing);
}

static void FontFontSmoothingBase(FontDescription& d) {
  d.SetFontSmoothing(kAntialiased);
}
static void FontFontSmoothingValue(FontBuilder& b) {
  b.SetFontSmoothing(kSubpixelAntialiased);
}

static void FontSizeBase(FontDescription& d) {
  d.SetSpecifiedSize(37.0f);
  d.SetComputedSize(37.0f);
  d.SetIsAbsoluteSize(true);
  d.SetKeywordSize(7);
}
static void FontSizeValue(FontBuilder& b) {
  b.SetSize(FontDescription::Size(20.0f, 0, false));
}

static void FontScriptBase(FontDescription& d) {
  d.SetLocale(LayoutLocale::Get("no"));
}
static void FontScriptValue(FontBuilder& b) {
  b.SetLocale(LayoutLocale::Get("se"));
}

INSTANTIATE_TEST_SUITE_P(
    AllFields,
    FontBuilderAdditiveTest,
    testing::Values(
        FunctionPair(FontWeightBase, FontWeightValue),
        FunctionPair(FontStretchBase, FontStretchValue),
        FunctionPair(FontFamilyBase, FontFamilyValue),
        FunctionPair(FontFeatureSettingsBase, FontFeatureSettingsValue),
        FunctionPair(FontStyleBase, FontStyleValue),
        FunctionPair(FontVariantCapsBase, FontVariantCapsValue),
        FunctionPair(FontVariantLigaturesBase, FontVariantLigaturesValue),
        FunctionPair(FontVariantNumericBase, FontVariantNumericValue),
        FunctionPair(FontTextRenderingBase, FontTextRenderingValue),
        FunctionPair(FontKerningBase, FontKerningValue),
        FunctionPair(FontFontSmoothingBase, FontFontSmoothingValue),
        FunctionPair(FontSizeBase, FontSizeValue),
        FunctionPair(FontScriptBase, FontScriptValue),
        FunctionPair(FontOpticalSizingBase, FontOpticalSizingValue)));

}  // namespace blink
