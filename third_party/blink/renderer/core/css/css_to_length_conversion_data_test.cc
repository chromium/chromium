// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"

#include <optional>

#include "third_party/blink/renderer/core/css/anchor_evaluator.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

// Evaluates any query to `result`.
class TestAnchorEvaluator : public AnchorEvaluator {
  STACK_ALLOCATED();

 public:
  explicit TestAnchorEvaluator(std::optional<LayoutUnit> result)
      : result_(result) {}

  std::optional<LayoutUnit> Evaluate(
      const AnchorQuery&,
      const ScopedCSSName* position_anchor,
      const std::optional<PositionAreaOffsets>&) override {
    return result_;
  }
  std::optional<PositionAreaOffsets> ComputePositionAreaOffsetsForLayout(
      const ScopedCSSName*,
      PositionArea) override {
    return PositionAreaOffsets();
  }
  std::optional<PhysicalOffset> ComputeAnchorCenterOffsets(
      const ComputedStyleBuilder&) override {
    return std::nullopt;
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
    AnchorEvaluator* anchor_evaluator = nullptr;
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
        CSSToLengthConversionData::AnchorData(
            options.anchor_evaluator,
            /* position_anchor */ nullptr,
            /* position_area_offsets */ std::nullopt),
        options.data_zoom.value_or(div->GetComputedStyle()->EffectiveZoom()),
        options.flags ? *options.flags : ignored_flags_);
  }

  CSSToLengthConversionData ConversionData() {
    return ConversionData(DataOptions{});
  }

  // Parses the given string a <length>, and converts the result to a Length.
  //
  // A property may be specified to invoke the parsing behavior of that
  // specific property.
  Length ConvertLength(const CSSToLengthConversionData& data,
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

    return primitive_value->ConvertToLength(data);
  }

  float ConvertPx(const CSSToLengthConversionData& data,
                  String value,
                  CSSPropertyID property_id = CSSPropertyID::kLeft) {
    return ConvertLength(data, value, property_id).Pixels();
  }

  CSSToLengthConversionData::Flags ConversionFlags(String value) {
    CSSToLengthConversionData::Flags flags = 0;
    CSSToLengthConversionData data = ConversionData({.flags = &flags});
    ConvertPx(data, value);
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
  EXPECT_FLOAT_EQ(1.0f, ConvertPx(data, "1px"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1em"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1ex"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1ch"));
  EXPECT_FLOAT_EQ(10.0f, ConvertPx(data, "1rem"));
  EXPECT_FLOAT_EQ(8.0f, ConvertPx(data, "1rex"));
  EXPECT_FLOAT_EQ(10.0f, ConvertPx(data, "1rch"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1ic"));
  EXPECT_FLOAT_EQ(10.0f, ConvertPx(data, "1ric"));
  EXPECT_FLOAT_EQ(36.0f, ConvertPx(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(100.0f, ConvertPx(data, "1lh"));
  EXPECT_FLOAT_EQ(50.0f, ConvertPx(data, "1rlh"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1cap"));
  EXPECT_FLOAT_EQ(8.0f, ConvertPx(data, "1rcap"));
}

TEST_F(CSSToLengthConversionDataTest, Zoomed) {
  CSSToLengthConversionData data = ConversionData({.css_zoom = 2.0f});
  EXPECT_FLOAT_EQ(2.0f, ConvertPx(data, "1px"));
  EXPECT_FLOAT_EQ(40.0f, ConvertPx(data, "1em"));
  EXPECT_FLOAT_EQ(32.0f, ConvertPx(data, "1ex"));
  EXPECT_FLOAT_EQ(40.0f, ConvertPx(data, "1ch"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1rem"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1rex"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1rch"));
  EXPECT_FLOAT_EQ(40.0f, ConvertPx(data, "1ic"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1ric"));
  EXPECT_FLOAT_EQ(72.0f, ConvertPx(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(200.0f, ConvertPx(data, "1lh"));
  EXPECT_FLOAT_EQ(100.0f, ConvertPx(data, "1rlh"));
  EXPECT_FLOAT_EQ(32.0f, ConvertPx(data, "1cap"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1rcap"));
}

TEST_F(CSSToLengthConversionDataTest, AdjustedZoom) {
  CSSToLengthConversionData data = ConversionData().CopyWithAdjustedZoom(2.0f);
  EXPECT_FLOAT_EQ(2.0f, ConvertPx(data, "1px"));
  EXPECT_FLOAT_EQ(40.0f, ConvertPx(data, "1em"));
  EXPECT_FLOAT_EQ(32.0f, ConvertPx(data, "1ex"));
  EXPECT_FLOAT_EQ(40.0f, ConvertPx(data, "1ch"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1rem"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1rex"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1rch"));
  EXPECT_FLOAT_EQ(40.0f, ConvertPx(data, "1ic"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1ric"));
  EXPECT_FLOAT_EQ(72.0f, ConvertPx(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(200.0f, ConvertPx(data, "1lh"));
  EXPECT_FLOAT_EQ(100.0f, ConvertPx(data, "1rlh"));
  EXPECT_FLOAT_EQ(32.0f, ConvertPx(data, "1cap"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1rcap"));
}

TEST_F(CSSToLengthConversionDataTest, DifferentZoom) {
  // The zoom used to calculate fonts is different from the requested
  // zoom in the CSSToLengthConversionData constructor.
  CSSToLengthConversionData data =
      ConversionData({.css_zoom = 1.0f, .data_zoom = 2.0f});
  EXPECT_FLOAT_EQ(2.0f, ConvertPx(data, "1px"));
  EXPECT_FLOAT_EQ(40.0f, ConvertPx(data, "1em"));
  EXPECT_FLOAT_EQ(32.0f, ConvertPx(data, "1ex"));
  EXPECT_FLOAT_EQ(40.0f, ConvertPx(data, "1ch"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1rem"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1rex"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1rch"));
  EXPECT_FLOAT_EQ(40.0f, ConvertPx(data, "1ic"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1ric"));
  EXPECT_FLOAT_EQ(72.0f, ConvertPx(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(200.0f, ConvertPx(data, "1lh"));
  EXPECT_FLOAT_EQ(100.0f, ConvertPx(data, "1rlh"));
  EXPECT_FLOAT_EQ(32.0f, ConvertPx(data, "1cap"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1rcap"));
}

TEST_F(CSSToLengthConversionDataTest, Unzoomed) {
  CSSToLengthConversionData data =
      ConversionData({.css_zoom = 2.0f}).Unzoomed();
  EXPECT_FLOAT_EQ(1.0f, ConvertPx(data, "1px"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1em"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1ex"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1ch"));
  EXPECT_FLOAT_EQ(10.0f, ConvertPx(data, "1rem"));
  EXPECT_FLOAT_EQ(8.0f, ConvertPx(data, "1rex"));
  EXPECT_FLOAT_EQ(10.0f, ConvertPx(data, "1rch"));
  EXPECT_FLOAT_EQ(20.0f, ConvertPx(data, "1ic"));
  EXPECT_FLOAT_EQ(10.0f, ConvertPx(data, "1ric"));
  EXPECT_FLOAT_EQ(36.0f, ConvertPx(data, "calc(1em + 1ex)"));
  EXPECT_FLOAT_EQ(100.0f, ConvertPx(data, "1lh"));
  EXPECT_FLOAT_EQ(50.0f, ConvertPx(data, "1rlh"));
  EXPECT_FLOAT_EQ(16.0f, ConvertPx(data, "1cap"));
  EXPECT_FLOAT_EQ(8.0f, ConvertPx(data, "1rcap"));
}

TEST_F(CSSToLengthConversionDataTest, StyleLessContainerUnitConversion) {
  // No ComputedStyle associated.
  CSSToLengthConversionData data;

  // Don't crash:
  ConvertPx(data, "1cqw");
  ConvertPx(data, "1cqh");
}

TEST_F(CSSToLengthConversionDataTest, SetLineHeightSize) {
  CSSToLengthConversionData data = ConversionData();
  EXPECT_FLOAT_EQ(100.0f, ConvertPx(data, "1lh"));
  Element* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);
  SetLineHeightSize(*div, data);
  EXPECT_FLOAT_EQ(200.0f, ConvertPx(data, "1lh"));
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
  ConvertPx(data, "1em");
  ConvertPx(data, "1rem");
  ConvertPx(data, "1ex");
  ConvertPx(data, "1rex");
  ConvertPx(data, "1ch");
  ConvertPx(data, "1rch");
  ConvertPx(data, "1ic");
  ConvertPx(data, "1ric");
  ConvertPx(data, "1lh");
  ConvertPx(data, "1rlh");
}

TEST_F(CSSToLengthConversionDataTest, AnchorFunction) {
  TestAnchorEvaluator anchor_evaluator(/* result */ LayoutUnit(60.0));
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID right = CSSPropertyID::kRight;

  EXPECT_FLOAT_EQ(60.0f, ConvertPx(data, "anchor(--a left)", right));
  EXPECT_FLOAT_EQ(2.0f, ConvertPx(data, "calc(anchor(--a left) / 30)", right));
}

TEST_F(CSSToLengthConversionDataTest, AnchorFunctionFallback) {
  TestAnchorEvaluator anchor_evaluator(/* result */ std::nullopt);
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID right = CSSPropertyID::kRight;

  EXPECT_FLOAT_EQ(42.0f, ConvertPx(data, "anchor(--a left, 42px)", right));
  EXPECT_FLOAT_EQ(
      52.0f, ConvertPx(data, "anchor(--a left, calc(42px + 10px))", right));
  EXPECT_FLOAT_EQ(10.0f,
                  ConvertPx(data, "anchor(--a left, min(42px, 10px))", right));
}

TEST_F(CSSToLengthConversionDataTest, AnchorSizeFunction) {
  TestAnchorEvaluator anchor_evaluator(/* result */ LayoutUnit(60.0));
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID width = CSSPropertyID::kWidth;

  EXPECT_FLOAT_EQ(60.0f, ConvertPx(data, "anchor-size(width)", width));
  EXPECT_FLOAT_EQ(60.0f, ConvertPx(data, "anchor-size(--a width)", width));
  EXPECT_FLOAT_EQ(2.0f,
                  ConvertPx(data, "calc(anchor-size(--a width) / 30)", width));
}

TEST_F(CSSToLengthConversionDataTest, AnchorSizeFunctionFallback) {
  TestAnchorEvaluator anchor_evaluator(/* result */ std::nullopt);
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID width = CSSPropertyID::kWidth;

  EXPECT_FLOAT_EQ(42.0f,
                  ConvertPx(data, "anchor-size(--a width, 42px)", width));
  EXPECT_FLOAT_EQ(
      52.0f,
      ConvertPx(data, "anchor-size(--a width, calc(42px + 10px))", width));
  EXPECT_FLOAT_EQ(
      10.0f, ConvertPx(data, "anchor-size(--a width, min(42px, 10px))", width));
}

TEST_F(CSSToLengthConversionDataTest, AnchorWithinOtherFunction) {
  TestAnchorEvaluator anchor_evaluator(/* result */ std::nullopt);
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID right = CSSPropertyID::kRight;

  EXPECT_FLOAT_EQ(
      42.0f, ConvertPx(data, "calc(anchor(--a left, 10px) + 32px)", right));
  EXPECT_EQ(ConvertLength(data, "calc(10px + 42%)", right),
            ConvertLength(data, "calc(anchor(--a left, 10px) + 42%)", right));
  EXPECT_EQ(ConvertLength(data, "calc(0px + 42%)", right),
            ConvertLength(data, "calc(anchor(--a left, 0px) + 42%)", right));
  EXPECT_EQ(ConvertLength(data, "min(10px, 42%)", right),
            ConvertLength(data, "min(anchor(--a left, 10px), 42%)", right));
  EXPECT_EQ(ConvertLength(data, "min(10px, 42%)", right),
            ConvertLength(data, "min(anchor(--a left, 10px), 42%)", right));
  EXPECT_FLOAT_EQ(
      10.0f,
      ConvertLength(data, "min(anchor(--a left, 10px), 42px)", right).Pixels());
  // TODO(crbug.com/326088870): This result is to be expected from the current
  // implementation, but it's not consistent with what you get if you specify
  // min(10%, 42%) directly (52%).
  EXPECT_EQ("min(10%, 42%)",
            CSSPrimitiveValue::CreateFromLength(
                ConvertLength(data, "min(anchor(--a left, 10%), 42%)", right),
                /* zoom */ 1.0f)
                ->CssText());
}

TEST_F(CSSToLengthConversionDataTest, AnchorFunctionPercentageFallback) {
  TestAnchorEvaluator anchor_evaluator(/* result */ std::nullopt);
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID right = CSSPropertyID::kRight;

  EXPECT_EQ("42%", CSSPrimitiveValue::CreateFromLength(
                       ConvertLength(data, "anchor(--a left, 42%)", right),
                       /* zoom */ 1.0f)
                       ->CssText());
  EXPECT_EQ("52%",
            CSSPrimitiveValue::CreateFromLength(
                ConvertLength(data, "anchor(--a left, calc(42% + 10%))", right),
                /* zoom */ 1.0f)
                ->CssText());
  EXPECT_EQ("10%",
            CSSPrimitiveValue::CreateFromLength(
                ConvertLength(data, "anchor(--a left, min(42%, 10%))", right),
                /* zoom */ 1.0f)
                ->CssText());
}

TEST_F(CSSToLengthConversionDataTest,
       AnchorFunctionPercentageFallbackNotTaken) {
  TestAnchorEvaluator anchor_evaluator(/* result */ LayoutUnit(60.0));
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID right = CSSPropertyID::kRight;

  // TODO(crbug.com/326088870): This result is probably not what we want.
  EXPECT_EQ("calc(60px)",
            CSSPrimitiveValue::CreateFromLength(
                ConvertLength(data, "anchor(--a left, 42%)", right),
                /* zoom */ 1.0f)
                ->CssText());
}

TEST_F(CSSToLengthConversionDataTest, AnchorFunctionFallbackNullEvaluator) {
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = nullptr});

  CSSPropertyID right = CSSPropertyID::kRight;

  EXPECT_FLOAT_EQ(42.0f, ConvertPx(data, "anchor(--a right, 42px)", right));
}

TEST_F(CSSToLengthConversionDataTest, AnchorFunctionLengthPercentageFallback) {
  TestAnchorEvaluator anchor_evaluator(/* result */ std::nullopt);
  CSSToLengthConversionData data =
      ConversionData({.anchor_evaluator = &anchor_evaluator});

  CSSPropertyID right = CSSPropertyID::kRight;

  EXPECT_EQ(ConvertLength(data, "calc(10px + 42%)", right),
            ConvertLength(data, "anchor(--a left, calc(10px + 42%))", right));
  EXPECT_EQ(ConvertLength(data, "min(10px, 42%)", right),
            ConvertLength(data, "anchor(--a left, min(10px, 42%))", right));
}

TEST_F(CSSToLengthConversionDataTest, ContainerUnitsWithContainerName) {
  auto* container = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  container->SetInlineStyleProperty(CSSPropertyID::kWidth, "322px");
  container->SetInlineStyleProperty(CSSPropertyID::kHeight, "228px");
  container->SetInlineStyleProperty(CSSPropertyID::kContainerName,
                                    "root_container");
  container->SetInlineStyleProperty(CSSPropertyID::kContainerType, "size");
  auto* child = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->AppendChild(container);
  container->AppendChild(child);
  UpdateAllLifecyclePhasesForTest();

  CSSToLengthConversionData::Flags flags = 0;
  CSSToLengthConversionData length_resolver(
      child->ComputedStyleRef(), GetDocument().body()->GetComputedStyle(),
      GetDocument().documentElement()->GetComputedStyle(),
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(child),
      CSSToLengthConversionData::AnchorData(
          nullptr,
          /* position_anchor */ nullptr,
          /* position_area_offsets */ std::nullopt),
      child->GetComputedStyle()->EffectiveZoom(), flags);

  ScopedCSSName* name = MakeGarbageCollected<ScopedCSSName>(
      AtomicString("root_container"), nullptr);
  EXPECT_EQ(length_resolver.ContainerWidth(*name), 322);
  EXPECT_EQ(length_resolver.ContainerHeight(*name), 228);
}

}  // namespace blink
