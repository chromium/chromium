// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_painter.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {
namespace {

class TextPainterTest : public RenderingTest {
 public:
  TextPainterTest()
      : layout_text_(nullptr),
        paint_controller_(std::make_unique<PaintController>()),
        context_(*paint_controller_) {}

 protected:
  LineLayoutText GetLineLayoutText() { return LineLayoutText(layout_text_); }

  PaintInfo CreatePaintInfo(bool uses_text_as_clip, bool is_printing) {
    return PaintInfo(
        context_, IntRect(),
        uses_text_as_clip ? PaintPhase::kTextClip
                          : PaintPhase::kSelfBlockBackgroundOnly,
        is_printing ? kGlobalPaintPrinting : kGlobalPaintNormalPhase, 0);
  }

 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    SetBodyInnerHTML("Hello world");
    UpdateLayoutText();
  }
  void UpdateLayoutText() {
    layout_text_ =
        ToLayoutText(GetDocument().body()->firstChild()->GetLayoutObject());
    ASSERT_TRUE(layout_text_);
    ASSERT_EQ("Hello world", layout_text_->GetText());
  }

  LayoutText* layout_text_;
  std::unique_ptr<PaintController> paint_controller_;
  GraphicsContext context_;
};

TEST_F(TextPainterTest, TextPaintingStyle_Simple) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kColor,
                                               CSSValueID::kBlue);
  UpdateAllLifecyclePhasesForTest();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(false /* usesTextAsClip */, false /* isPrinting */));
  EXPECT_EQ(Color(0, 0, 255), text_style.fill_color);
  EXPECT_EQ(Color(0, 0, 255), text_style.stroke_color);
  EXPECT_EQ(Color(0, 0, 255), text_style.emphasis_mark_color);
  EXPECT_EQ(0, text_style.stroke_width);
  EXPECT_EQ(nullptr, text_style.shadow);
}

TEST_F(TextPainterTest, TextPaintingStyle_AllProperties) {
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextFillColor, CSSValueID::kRed);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextStrokeColor, CSSValueID::kLime);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextEmphasisColor, CSSValueID::kBlue);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextStrokeWidth, 4,
      CSSPrimitiveValue::UnitType::kPixels);
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kTextShadow,
                                               "1px 2px 3px yellow");
  UpdateAllLifecyclePhasesForTest();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(false /* usesTextAsClip */, false /* isPrinting */));
  EXPECT_EQ(Color(255, 0, 0), text_style.fill_color);
  EXPECT_EQ(Color(0, 255, 0), text_style.stroke_color);
  EXPECT_EQ(Color(0, 0, 255), text_style.emphasis_mark_color);
  EXPECT_EQ(4, text_style.stroke_width);
  ASSERT_NE(nullptr, text_style.shadow);
  EXPECT_EQ(1u, text_style.shadow->Shadows().size());
  EXPECT_EQ(1, text_style.shadow->Shadows()[0].X());
  EXPECT_EQ(2, text_style.shadow->Shadows()[0].Y());
  EXPECT_EQ(3, text_style.shadow->Shadows()[0].Blur());
  EXPECT_EQ(Color(255, 255, 0),
            text_style.shadow->Shadows()[0].GetColor().GetColor());
}

TEST_F(TextPainterTest, TextPaintingStyle_UsesTextAsClip) {
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextFillColor, CSSValueID::kRed);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextStrokeColor, CSSValueID::kLime);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextEmphasisColor, CSSValueID::kBlue);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextStrokeWidth, 4,
      CSSPrimitiveValue::UnitType::kPixels);
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kTextShadow,
                                               "1px 2px 3px yellow");
  UpdateAllLifecyclePhasesForTest();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(true /* usesTextAsClip */, false /* isPrinting */));
  EXPECT_EQ(Color::kBlack, text_style.fill_color);
  EXPECT_EQ(Color::kBlack, text_style.stroke_color);
  EXPECT_EQ(Color::kBlack, text_style.emphasis_mark_color);
  EXPECT_EQ(4, text_style.stroke_width);
  EXPECT_EQ(nullptr, text_style.shadow);
}

TEST_F(TextPainterTest,
       TextPaintingStyle_ForceBackgroundToWhite_NoAdjustmentNeeded) {
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextFillColor, CSSValueID::kRed);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextStrokeColor, CSSValueID::kLime);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextEmphasisColor, CSSValueID::kBlue);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitPrintColorAdjust, CSSValueID::kEconomy);
  GetDocument().GetSettings()->SetShouldPrintBackgrounds(false);
  FloatSize page_size(500, 800);
  GetFrame().StartPrinting(page_size, page_size, 1);
  UpdateAllLifecyclePhasesForTest();
  // In LayoutNG, printing currently forces layout tree reattachment,
  // so we need to re-get layout_text_.
  UpdateLayoutText();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(false /* usesTextAsClip */, true /* isPrinting */));
  EXPECT_EQ(Color(255, 0, 0), text_style.fill_color);
  EXPECT_EQ(Color(0, 255, 0), text_style.stroke_color);
  EXPECT_EQ(Color(0, 0, 255), text_style.emphasis_mark_color);
}

TEST_F(TextPainterTest, TextPaintingStyle_ForceBackgroundToWhite_Darkened) {
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextFillColor, "rgb(255, 220, 220)");
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextStrokeColor, "rgb(220, 255, 220)");
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextEmphasisColor, "rgb(220, 220, 255)");
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitPrintColorAdjust, CSSValueID::kEconomy);
  GetDocument().GetSettings()->SetShouldPrintBackgrounds(false);
  FloatSize page_size(500, 800);
  GetFrame().StartPrinting(page_size, page_size, 1);
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();
  // In LayoutNG, printing currently forces layout tree reattachment,
  // so we need to re-get layout_text_.
  UpdateLayoutText();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(false /* usesTextAsClip */, true /* isPrinting */));
  EXPECT_EQ(Color(255, 220, 220).Dark(), text_style.fill_color);
  EXPECT_EQ(Color(220, 255, 220).Dark(), text_style.stroke_color);
  EXPECT_EQ(Color(220, 220, 255).Dark(), text_style.emphasis_mark_color);
}

}  // namespace
}  // namespace blink
