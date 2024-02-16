// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"

#include <optional>

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

// Evaluates any query to `result`.
class TestAnchorEvaluator : public Length::AnchorEvaluator {
  STACK_ALLOCATED();

 public:
  explicit TestAnchorEvaluator(std::optional<LayoutUnit> result)
      : result_(result) {}

  std::optional<LayoutUnit> Evaluate(
      const CalculationExpressionNode&) const override {
    return result_;
  }

 private:
  std::optional<LayoutUnit> result_;
};

}  // namespace

class CSSToLengthConversionDataTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    LoadAhem();
  }

  struct DataOptions {
    // The zoom to apply to :root.
    std::optional<float> css_zoom;
    // The zoom to pass to the CSSToLengthConversionData constructor.
    std::optional<float> data_zoom;
    // Used to evaluate anchor() and anchor-size() queries.
    Length::AnchorEvaluator* anchor_evaluator = nullptr;
    // Any flags set by conversion is stored here.
    // See CSSToLengthConversionData::Flag.
    CSSToLengthConversionData::Flags* flags = nullptr;
  };

  // Set up a page with "Ahem 10px" as :root, and "Ahem 20px" at some <div>,
  // then return a CSSToLengthConversionData constructed from that.
  CSSToLengthConversionData ConversionData(DataOptions options) {
    Element* root = GetDocument().documentElement();
    DCHECK(root);
    if (options.css_zoom.has_value()) {
      root->SetInlineStyleProperty(CSSPropertyID::kZoom,
                                   String::Format("%f", *options.css_zoom));
    }
    root->SetInlineStyleProperty(CSSPropertyID::kFontSize, "10px");
    root->SetInlineStyleProperty(CSSPropertyID::kFontFamily, "Ahem");
    root->SetInlineStyleProperty(CSSPropertyID::kLineHeight, "5");
    auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    div->SetInlineStyleProperty(CSSPropertyID::kFontSize, "20px");
    div->SetIdAttribute(AtomicString("div"));
    GetDocument().body()->AppendChild(div);
    GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kLineHeight,
                                                 "10");
    UpdateAllLifecyclePhasesForTest();

    return CSSToLengthConversionData(
        div->ComputedStyleRef(), GetDocument().body()->GetComputedStyle(),
        GetDocument().documentElement()->GetComputedStyle(),
        CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
        CSSToLengthConversionData::ContainerSizes(),
        CSSToLengthConversionData::AnchorData(options.anchor_evaluator),
        options.data_zoom.value_or(div->GetComputedStyle()->EffectiveZoom()),
        options.flags ? *options.flags : ignored_flags_);
  }

  CSSToLengthConversionData ConversionData() {
    return ConversionData(DataOptions{});
  }

  // Parses the given string a <length>, and converts the result to pixels.
  //
  // A property may be specified to invoke the parsing behavior of that
  // specific property.
  float Convert(const CSSToLengthConversionData& data,
                String value,
                CSSPropertyID property_id = CSSPropertyID::kLeft) {
    const CSSValue* result = css_test_helpers::ParseLonghand(
        GetDocument(), CSSProperty::Get(property_id), value);
    CHECK(result);
    // Any tree-scoped references within `result` need to be populated with
    // their TreeScope. This is normally done by StyleCascade before length
    // conversion, and we're simulating that here.
    result = &result->EnsureScopedValue(&GetDocument());

    auto* primitive_value = DynamicTo<CSSPrimitiveValue>(result);
    DCHECK(primitive_value);

    return primitive_value->ConvertToLength(data).Pixels();
  }

  CSSToLengthConversionData::Flags ConversionFlags(String value) {
    CSSToLengthConversionData::Flags flags = 0;
    CSSToLengthConversionData data = ConversionData({.flags = &flags});
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
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1cap"));
  EXPECT_FLOAT_EQ(8.0f, Convert(data, "1rcap"));
}

TEST_F(CSSToLengthConversionDataTest, Zoomed) {
  CSSToLengthConversionData data = ConversionData({.css_zoom = 2.0f});
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
  EXPECT_FLOAT_EQ(32.0f, Convert(data, "1cap"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1rcap"));
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
  EXPECT_FLOAT_EQ(32.0f, Convert(data, "1cap"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1rcap"));
}

TEST_F(CSSToLengthConversionDataTest, DifferentZoom) {
  // The zoom used to calculate fonts is different from the requested
  // zoom in the CSSToLengthConversionData constructor.
  CSSToLengthConversionData data =
      ConversionData({.css_zoom = 1.0f, .data_zoom = 2.0f});
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
  EXPECT_FLOAT_EQ(32.0f, Convert(data, "1cap"));
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1rcap"));
}

TEST_F(CSSToLengthConversionDataTest, Unzoomed) {
  CSSToLengthConversionData data =
      ConversionData({.css_zoom = 2.0f}).Unzoomed();
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
  EXPECT_FLOAT_EQ(16.0f, Convert(data, "1cap"));
  EXPECT_FLOAT_EQ(8.0f, Convert(data, "1rcap"));
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
  Element* div = GetDocument().getElementById(AtomicString("div"));
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
  Flags cap = glyph;
  Flags rcap = glyph | rem;
  Flags lh = static_cast<Flags>(Flag::kLineHeightRelative);
  Flags rlh = glyph | rem | lh;
  Flags sv = static_cast<Flags>(Flag::kStaticViewport);
  Flags dv = static_cast<Flags>(Flag::kDynamicViewport);
  Flags cq = static_cast<Flags>(Flag::kContainerRelative);
  Flags ldr = static_cast<Flags>(Flag::kLogicalDirectionRelative);

  EXPECT_EQ(0u, ConversionFlags("1px"));

  EXPECT_EQ(em, ConversionFlags("1em"));
  EXPECT_EQ(cap, ConversionFlags("1cap"));

  EXPECT_EQ(rem, ConversionFlags("1rem"));
  EXPECT_EQ(rex, ConversionFlags("1rex"));
  EXPECT_EQ(rch, ConversionFlags("1rch"));
  EXPECT_EQ(ric, ConversionFlags("1ric"));
  EXPECT_EQ(rcap, ConversionFlags("1rcap"));

  EXPECT_EQ(glyph, ConversionFlags("1ex"));
  EXPECT_EQ(glyph, ConversionFlags("1ch"));
  EXPECT_EQ(glyph, ConversionFlags("1ic"));

  EXPECT_EQ(glyph | lh, ConversionFlags("1lh"));
  EXPECT_EQ(rlh, ConversionFlags("1rlh"));

  EXPECT_EQ(sv, ConversionFlags("1svw"));
  EXPECT_EQ(sv, ConversionFlags("1svh"));
  EXPECT_EQ(sv | ldr, ConversionFlags("1svi"));
  EXPECT_EQ(sv | ldr, ConversionFlags("1svb"));
  EXPECT_EQ(sv, ConversionFlags("1svmin"));
  EXPECT_EQ(sv, ConversionFlags("1svmax"));

  EXPECT_EQ(sv, ConversionFlags("1lvw"));
  EXPECT_EQ(sv, ConversionFlags("1lvh"));
  EXPECT_EQ(sv | ldr, ConversionFlags("1lvi"));
  EXPECT_EQ(sv | ldr, ConversionFlags("1lvb"));
  EXPECT_EQ(sv, ConversionFlags("1lvmin"));
  EXPECT_EQ(sv, ConversionFlags("1lvmax"));

  EXPECT_EQ(sv, ConversionFlags("1vw"));
  EXPECT_EQ(sv, ConversionFlags("1vh"));
  EXPECT_EQ(sv | ldr, ConversionFlags("1vi"));
  EXPECT_EQ(sv | ldr, ConversionFlags("1vb"));
  EXPECT_EQ(sv, ConversionFlags("1vmin"));
  EXPECT_EQ(sv, ConversionFlags("1vmax"));

  EXPECT_EQ(dv, ConversionFlags("1dvw"));
  EXPECT_EQ(dv, ConversionFlags("1dvh"));
  EXPECT_EQ(dv | ldr, ConversionFlags("1dvi"));
  EXPECT_EQ(dv | ldr, ConversionFlags("1dvb"));
  EXPECT_EQ(dv, ConversionFlags("1dvmin"));
  EXPECT_EQ(dv, ConversionFlags("1dvmax"));

  // Since there is no container, these units fall back to the small viewport.
  EXPECT_EQ(cq | sv, ConversionFlags("1cqh"));
  EXPECT_EQ(cq | sv, ConversionFlags("1cqw"));
  EXPECT_EQ(cq | sv | ldr, ConversionFlags("1cqi"));
  EXPECT_EQ(cq | sv | ldr, ConversionFlags("1cqb"));
  EXPECT_EQ(cq | sv, ConversionFlags("1cqmin"));
  EXPECT_EQ(cq | sv, ConversionFlags("1cqmax"));

  EXPECT_EQ(em | glyph, ConversionFlags("calc(1em + 1ex)"));
}

TEST_F(CSSToLengthConversionDataTest, ConversionWithoutPrimaryFont) {
  FontDescription font_description;
  Font font(font_description);
  font.NullifyPrimaryFontForTesting();

  ASSERT_FALSE(font.PrimaryFont());

  CSSToLengthConversionData data;
  CSSToLengthConversionData::FontSizes font_sizes(
      /* em */ 16.0f, /* rem */ 16.0f, &font, /* font_zoom */ 1.0f);
  CSSToLengthConversionData::LineHeightSize line_height_size(
      Length::Fixed(16.0f), &font, /* font_zoom */ 1.0f);
  data.SetFontSizes(font_sizes);
  data.SetLineHeightSize(line_height_size);

  // Don't crash:
  Convert(data, "1em");
  Convert(data, "1rem");
  Convert(data, "1ex");
  Convert(data, "1rex");
  Convert(data, "1ch");
  Convert(data, "1rch");
  Convert(data, "1ic");
  Convert(data, "1ric");
  Convert(data, "1lh");
  Convert(data, "1rlh");
}

TEST_F(CSSToLengthConversionDataTest, AnchorFunction) {
  ScopedCSSAnchorPositioningForTest anchor_feature(true);
  ScopedCSSAnchorPositioningComputeAnchorForTest compute_anchor_feature(true);

  TestAnchorEvaluator anchor_evaluator(/* result */ LayoutUnit(60.0));
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID right = CSSPropertyID::kRight;

  EXPECT_FLOAT_EQ(60.0f, Convert(data, "anchor(left)", right));
  EXPECT_FLOAT_EQ(60.0f, Convert(data, "anchor(--a left)", right));
  EXPECT_FLOAT_EQ(2.0f, Convert(data, "calc(anchor(--a left) / 30)", right));
}

TEST_F(CSSToLengthConversionDataTest, AnchorFunctionFallback) {
  ScopedCSSAnchorPositioningForTest anchor_feature(true);
  ScopedCSSAnchorPositioningComputeAnchorForTest compute_anchor_feature(true);

  TestAnchorEvaluator anchor_evaluator(/* result */ std::nullopt);
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID right = CSSPropertyID::kRight;

  EXPECT_FLOAT_EQ(0.0f, Convert(data, "anchor(left)", right));
  EXPECT_FLOAT_EQ(42.0f, Convert(data, "anchor(--a left, 42px)", right));
}

TEST_F(CSSToLengthConversionDataTest, AnchorSizeFunction) {
  ScopedCSSAnchorPositioningForTest anchor_feature(true);
  ScopedCSSAnchorPositioningComputeAnchorForTest compute_anchor_feature(true);

  TestAnchorEvaluator anchor_evaluator(/* result */ LayoutUnit(60.0));
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID width = CSSPropertyID::kWidth;

  EXPECT_FLOAT_EQ(60.0f, Convert(data, "anchor-size(width)", width));
  EXPECT_FLOAT_EQ(60.0f, Convert(data, "anchor-size(--a width)", width));
  EXPECT_FLOAT_EQ(2.0f,
                  Convert(data, "calc(anchor-size(--a width) / 30)", width));
}

TEST_F(CSSToLengthConversionDataTest, AnchorSizeFunctionFallback) {
  ScopedCSSAnchorPositioningForTest anchor_feature(true);
  ScopedCSSAnchorPositioningComputeAnchorForTest compute_anchor_feature(true);

  TestAnchorEvaluator anchor_evaluator(/* result */ std::nullopt);
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID width = CSSPropertyID::kWidth;

  EXPECT_FLOAT_EQ(0.0f, Convert(data, "anchor-size(width)", width));
  EXPECT_FLOAT_EQ(42.0f, Convert(data, "anchor-size(--a width, 42px)", width));
}

}  // namespace blink
