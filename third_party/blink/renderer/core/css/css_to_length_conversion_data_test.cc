// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

class CSSToLengthConversionDataTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    LoadAhem();
  }

  // Set up a page with "Ahem 10px" as :root, and "Ahem 20px" at some <div>,
  // then return a CSSToLengthConversionData constructed from that.
  //
  // css_zoom - The zoom to apply to :root.
  // data_zoom - The zoom to pass to the CSSToLengthConversionData constructor.
  CSSToLengthConversionData ConversionData(
      absl::optional<float> css_zoom,
      absl::optional<float> data_zoom,
      CSSToLengthConversionData::Flags& flags) {
    Element* root = GetDocument().documentElement();
    DCHECK(root);
    if (css_zoom.has_value()) {
      root->SetInlineStyleProperty(CSSPropertyID::kZoom,
                                   String::Format("%f", *css_zoom));
    }
    root->SetInlineStyleProperty(CSSPropertyID::kFontSize, "10px");
    root->SetInlineStyleProperty(CSSPropertyID::kFontFamily, "Ahem");
    root->SetInlineStyleProperty(CSSPropertyID::kLineHeight, "5");
    auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    div->SetInlineStyleProperty(CSSPropertyID::kFontSize, "20px");
    div->SetIdAttribute("div");
    GetDocument().body()->AppendChild(div);
    GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kLineHeight,
                                                 "10");
    UpdateAllLifecyclePhasesForTest();

    return CSSToLengthConversionData(
        div->ComputedStyleRef(), GetDocument().body()->GetComputedStyle(),
        GetDocument().documentElement()->GetComputedStyle(),
        GetDocument().GetLayoutView(),
        CSSToLengthConversionData::ContainerSizes(),
        data_zoom.value_or(div->GetComputedStyle()->EffectiveZoom()), flags);
  }

  CSSToLengthConversionData ConversionData(
      absl::optional<float> css_zoom = absl::nullopt,
      absl::optional<float> data_zoom = absl::nullopt) {
    return ConversionData(css_zoom, data_zoom, ignored_flags_);
  }

  CSSToLengthConversionData ConversionData(
      CSSToLengthConversionData::Flags& flags) {
    return ConversionData(absl::nullopt, absl::nullopt, flags);
  }

  float Convert(const CSSToLengthConversionData& data, String value) {
    auto* primitive_value = DynamicTo<CSSPrimitiveValue>(
        css_test_helpers::ParseValue(GetDocument(), "<length>", value));
    DCHECK(primitive_value);
    return primitive_value->ConvertToLength(data).Pixels();
  }

  CSSToLengthConversionData::Flags ConversionFlags(String value) {
    CSSToLengthConversionData::Flags flags = 0;
    CSSToLengthConversionData data = ConversionData(flags);
    Convert(data, value);
    return flags;
  }

  void SetLineHeightSize(Element& element, CSSToLengthConversionData& data) {
    data.SetLineHeightSize(CSSToLengthConversionData::LineHeightSize(
        element.ComputedStyleRef().GetFontSizeStyle(),
        element.GetDocument().documentElement()->GetComputedStyle()));
  }

 private:
  CSSToLengthConversionData::Flags ignored_flags_ = 0;
};

TEST_F(CSSToLengthConversionDataTest, Normal) {
  CSSToLengthConversionData data = ConversionData();
  EXPECT_FLOAT_EQ(1.0f, Convert(data, "1px"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1em"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1ex"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1ch"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1rem"));
  EXPECT_FLOAT_EQ(8.0f, Convert(data, "1rex"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1rch"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1ic"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1ric"));
  EXPECT_FLOAT_EQ(36.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(100.0f, Convert(data, "1lh"));
  EXPECT_FLOAT_EQ(50.0f, Convert(data, "1rlh"));
}

TEST_F(CSSToLengthConversionDataTest, Zoomed) {
  CSSToLengthConversionData data = ConversionData(2.0f);
  EXPECT_FLOAT_EQ(2.0f, Convert(data, "1px"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1em"));
  EXPECT_FLOAT_EQ(32.0f, Convert(data, "1ex"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1ch"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1rem"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1rex"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1rch"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1ic"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1ric"));
  EXPECT_FLOAT_EQ(72.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(200.0f, Convert(data, "1lh"));
  EXPECT_FLOAT_EQ(100.0f, Convert(data, "1rlh"));
}

TEST_F(CSSToLengthConversionDataTest, AdjustedZoom) {
  CSSToLengthConversionData data = ConversionData().CopyWithAdjustedZoom(2.0f);
  EXPECT_FLOAT_EQ(2.0f, Convert(data, "1px"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1em"));
  EXPECT_FLOAT_EQ(32.0f, Convert(data, "1ex"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1ch"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1rem"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1rex"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1rch"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1ic"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1ric"));
  EXPECT_FLOAT_EQ(72.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(200.0f, Convert(data, "1lh"));
  EXPECT_FLOAT_EQ(100.0f, Convert(data, "1rlh"));
}

TEST_F(CSSToLengthConversionDataTest, DifferentZoom) {
  // The zoom used to calculate fonts is different from the requested
  // zoom in the CSSToLengthConversionData constructor.
  CSSToLengthConversionData data = ConversionData(1.0f, 2.0f);
  EXPECT_FLOAT_EQ(2.0f, Convert(data, "1px"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1em"));
  EXPECT_FLOAT_EQ(32.0f, Convert(data, "1ex"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1ch"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1rem"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1rex"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1rch"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1ic"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1ric"));
  EXPECT_FLOAT_EQ(72.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(200.0f, Convert(data, "1lh"));
  EXPECT_FLOAT_EQ(100.0f, Convert(data, "1rlh"));
}

TEST_F(CSSToLengthConversionDataTest, Unzoomed) {
  CSSToLengthConversionData data = ConversionData(2.0f).Unzoomed();
  EXPECT_FLOAT_EQ(1.0f, Convert(data, "1px"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1em"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1ex"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1ch"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1rem"));
  EXPECT_FLOAT_EQ(8.0f, Convert(data, "1rex"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1rch"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1ic"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1ric"));
  EXPECT_FLOAT_EQ(36.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(100.0f, Convert(data, "1lh"));
  EXPECT_FLOAT_EQ(50.0f, Convert(data, "1rlh"));
}

TEST_F(CSSToLengthConversionDataTest, StyleLessContainerUnitConversion) {
  // No ComputedStyle associated.
  CSSToLengthConversionData data;

  // Don't crash:
  Convert(data, "1cqw");
  Convert(data, "1cqh");
}

TEST_F(CSSToLengthConversionDataTest, SetLineHeightSize) {
  CSSToLengthConversionData data = ConversionData();
  EXPECT_FLOAT_EQ(100.0f, Convert(data, "1lh"));
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  SetLineHeightSize(*div, data);
  EXPECT_FLOAT_EQ(200.0f, Convert(data, "1lh"));
}

TEST_F(CSSToLengthConversionDataTest, Flags) {
  using Flag = CSSToLengthConversionData::Flag;
  using Flags = CSSToLengthConversionData::Flags;

  Flags em = static_cast<Flags>(Flag::kEm);
  Flags rem = static_cast<Flags>(Flag::kRootFontRelative);
  Flags glyph = static_cast<Flags>(Flag::kGlyphRelative);
  Flags rex = rem | glyph;
  Flags rch = rem | glyph;
  Flags ric = rem | glyph;
  Flags lh = static_cast<Flags>(Flag::kLineHeightRelative);
  Flags rlh = glyph | rem | lh;
  Flags sv = static_cast<Flags>(Flag::kStaticViewport);
  Flags dv = static_cast<Flags>(Flag::kDynamicViewport);
  Flags cq = static_cast<Flags>(Flag::kContainerRelative);

  EXPECT_EQ(0u, ConversionFlags("1px"));

  EXPECT_EQ(em, ConversionFlags("1em"));

  EXPECT_EQ(rem, ConversionFlags("1rem"));
  EXPECT_EQ(rex, ConversionFlags("1rex"));
  EXPECT_EQ(rch, ConversionFlags("1rch"));
  EXPECT_EQ(ric, ConversionFlags("1ric"));

  EXPECT_EQ(glyph, ConversionFlags("1ex"));
  EXPECT_EQ(glyph, ConversionFlags("1ch"));
  EXPECT_EQ(glyph, ConversionFlags("1ic"));

  EXPECT_EQ(glyph | lh, ConversionFlags("1lh"));
  EXPECT_EQ(rlh, ConversionFlags("1rlh"));

  EXPECT_EQ(sv, ConversionFlags("1svw"));
  EXPECT_EQ(sv, ConversionFlags("1svh"));
  EXPECT_EQ(sv, ConversionFlags("1svi"));
  EXPECT_EQ(sv, ConversionFlags("1svb"));
  EXPECT_EQ(sv, ConversionFlags("1svmin"));
  EXPECT_EQ(sv, ConversionFlags("1svmax"));

  EXPECT_EQ(sv, ConversionFlags("1lvw"));
  EXPECT_EQ(sv, ConversionFlags("1lvh"));
  EXPECT_EQ(sv, ConversionFlags("1lvi"));
  EXPECT_EQ(sv, ConversionFlags("1lvb"));
  EXPECT_EQ(sv, ConversionFlags("1lvmin"));
  EXPECT_EQ(sv, ConversionFlags("1lvmax"));

  EXPECT_EQ(sv, ConversionFlags("1vw"));
  EXPECT_EQ(sv, ConversionFlags("1vh"));
  EXPECT_EQ(sv, ConversionFlags("1vi"));
  EXPECT_EQ(sv, ConversionFlags("1vb"));
  EXPECT_EQ(sv, ConversionFlags("1vmin"));
  EXPECT_EQ(sv, ConversionFlags("1vmax"));

  EXPECT_EQ(dv, ConversionFlags("1dvw"));
  EXPECT_EQ(dv, ConversionFlags("1dvh"));
  EXPECT_EQ(dv, ConversionFlags("1dvi"));
  EXPECT_EQ(dv, ConversionFlags("1dvb"));
  EXPECT_EQ(dv, ConversionFlags("1dvmin"));
  EXPECT_EQ(dv, ConversionFlags("1dvmax"));

  // Since there is no container, these units fall back to the small viewport.
  EXPECT_EQ(cq | sv, ConversionFlags("1cqh"));
  EXPECT_EQ(cq | sv, ConversionFlags("1cqw"));
  EXPECT_EQ(cq | sv, ConversionFlags("1cqi"));
  EXPECT_EQ(cq | sv, ConversionFlags("1cqb"));
  EXPECT_EQ(cq | sv, ConversionFlags("1cqmin"));
  EXPECT_EQ(cq | sv, ConversionFlags("1cqmax"));

  EXPECT_EQ(em | glyph, ConversionFlags("calc(1em + 1ex)"));
}

}  // namespace blink
