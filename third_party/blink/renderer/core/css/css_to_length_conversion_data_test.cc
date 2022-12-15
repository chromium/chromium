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
      absl::optional<float> css_zoom = absl::nullopt,
      absl::optional<float> data_zoom = absl::nullopt) {
    Element* root = GetDocument().documentElement();
    DCHECK(root);
    if (css_zoom.has_value()) {
      root->SetInlineStyleProperty(CSSPropertyID::kZoom,
                                   String::Format("%f", *css_zoom));
    }
    root->SetInlineStyleProperty(CSSPropertyID::kFontSize, "10px");
    root->SetInlineStyleProperty(CSSPropertyID::kFontFamily, "Ahem");
    auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    div->SetInlineStyleProperty(CSSPropertyID::kFontSize, "20px");
    div->SetIdAttribute("div");
    GetDocument().body()->AppendChild(div);
    UpdateAllLifecyclePhasesForTest();

    return CSSToLengthConversionData(
        div->GetComputedStyle(), GetDocument().body()->GetComputedStyle(),
        GetDocument().documentElement()->GetComputedStyle(),
        GetDocument().GetLayoutView(),
        CSSToLengthConversionData::ContainerSizes(),
        data_zoom.value_or(div->GetComputedStyle()->EffectiveZoom()));
  }

  float Convert(const CSSToLengthConversionData& data, String value) {
    auto* primitive_value = DynamicTo<CSSPrimitiveValue>(
        css_test_helpers::ParseValue(GetDocument(), "<length>", value));
    DCHECK(primitive_value);
    return primitive_value->ConvertToLength(data).Pixels();
  }

  void SetLineHeightSize(Element& element, CSSToLengthConversionData& data) {
    data.SetLineHeightSize(
        CSSToLengthConversionData::LineHeightSize(element.ComputedStyleRef()));
  }
};

TEST_F(CSSToLengthConversionDataTest, Normal) {
  CSSToLengthConversionData data = ConversionData();
  EXPECT_FLOAT_EQ(1.0f, Convert(data, "1px"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1em"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1ex"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1ch"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1rem"));
  EXPECT_FLOAT_EQ(36.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1lh"));
}

TEST_F(CSSToLengthConversionDataTest, Zoomed) {
  CSSToLengthConversionData data = ConversionData(2.0f);
  EXPECT_FLOAT_EQ(2.0f, Convert(data, "1px"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1em"));
  EXPECT_FLOAT_EQ(32.0f, Convert(data, "1ex"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1ch"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1rem"));
  EXPECT_FLOAT_EQ(72.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1lh"));
}

TEST_F(CSSToLengthConversionDataTest, AdjustedZoom) {
  CSSToLengthConversionData data = ConversionData().CopyWithAdjustedZoom(2.0f);
  EXPECT_FLOAT_EQ(2.0f, Convert(data, "1px"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1em"));
  EXPECT_FLOAT_EQ(32.0f, Convert(data, "1ex"));
  EXPECT_FLOAT_EQ(40.0f, Convert(data, "1ch"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1rem"));
  EXPECT_FLOAT_EQ(72.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1lh"));
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
  EXPECT_FLOAT_EQ(72.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1lh"));
}

TEST_F(CSSToLengthConversionDataTest, Unzoomed) {
  CSSToLengthConversionData data = ConversionData(2.0f).Unzoomed();
  EXPECT_FLOAT_EQ(1.0f, Convert(data, "1px"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1em"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1ex"));
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1ch"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1rem"));
  EXPECT_FLOAT_EQ(36.0f, Convert(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1lh"));
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
  EXPECT_FLOAT_EQ(10.0f, Convert(data, "1lh"));
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  SetLineHeightSize(*div, data);
  EXPECT_FLOAT_EQ(20.0f, Convert(data, "1lh"));
}

}  // namespace blink
