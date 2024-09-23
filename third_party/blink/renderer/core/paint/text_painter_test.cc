// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_painter.h"

#include <memory>

#include "cc/paint/paint_op.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace blink {
namespace {

class TextPainterTest : public RenderingTest {
 protected:
  const LayoutText& GetLayoutText() { return *layout_text_; }

  PaintInfo CreatePaintInfoForBackground(GraphicsContext& context) {
    return PaintInfo(context, CullRect(), PaintPhase::kSelfBlockBackgroundOnly,
                     /*descendant_painting_blocked=*/false);
  }

  PaintInfo CreatePaintInfoForTextClip(GraphicsContext& context) {
    return PaintInfo(context, CullRect(), PaintPhase::kTextClip,
                     /*descendant_painting_blocked=*/false);
  }

 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    SetBodyInnerHTML("Hello world");
    UpdateLayoutText();
  }
  void UpdateLayoutText() {
    layout_text_ =
        To<LayoutText>(GetDocument().body()->firstChild()->GetLayoutObject());
    ASSERT_TRUE(layout_text_);
    ASSERT_EQ("Hello world", layout_text_->TransformedText());
  }

  Persistent<LayoutText> layout_text_;
};

TEST_F(TextPainterTest, TextPaintingStyle_Simple) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kColor,
                                               CSSValueID::kBlue);
  UpdateAllLifecyclePhasesForTest();

  PaintController controller;
  GraphicsContext context(controller);
  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLayoutText().GetDocument(), GetLayoutText().StyleRef(),
      CreatePaintInfoForBackground(context));
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
      CSSPropertyID::kTextEmphasisColor, CSSValueID::kBlue);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextStrokeWidth, 4,
      CSSPrimitiveValue::UnitType::kPixels);
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kTextShadow,
                                               "1px 2px 3px yellow");
  UpdateAllLifecyclePhasesForTest();

  PaintController controller;
  GraphicsContext context(controller);
  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLayoutText().GetDocument(), GetLayoutText().StyleRef(),
      CreatePaintInfoForBackground(context));
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
      CSSPropertyID::kTextEmphasisColor, CSSValueID::kBlue);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitTextStrokeWidth, 4,
      CSSPrimitiveValue::UnitType::kPixels);
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kTextShadow,
                                               "1px 2px 3px yellow");
  UpdateAllLifecyclePhasesForTest();

  PaintController controller;
  GraphicsContext context(controller);
  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLayoutText().GetDocument(), GetLayoutText().StyleRef(),
      CreatePaintInfoForTextClip(context));
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
      CSSPropertyID::kTextEmphasisColor, CSSValueID::kBlue);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitPrintColorAdjust, CSSValueID::kEconomy);
  GetDocument().GetSettings()->SetShouldPrintBackgrounds(false);
  gfx::SizeF page_size(500, 800);
  GetFrame().StartPrinting(WebPrintParams(page_size));
  UpdateAllLifecyclePhasesForTest();
  // In LayoutNG, printing currently forces layout tree reattachment,
  // so we need to re-get layout_text_.
  UpdateLayoutText();

  PaintController controller;
  GraphicsContext context(controller);
  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLayoutText().GetDocument(), GetLayoutText().StyleRef(),
      CreatePaintInfoForBackground(context));
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
      CSSPropertyID::kTextEmphasisColor, "rgb(220, 220, 255)");
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyID::kWebkitPrintColorAdjust, CSSValueID::kEconomy);
  GetDocument().GetSettings()->SetShouldPrintBackgrounds(false);
  gfx::SizeF page_size(500, 800);
  GetFrame().StartPrinting(WebPrintParams(page_size));
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();
  // In LayoutNG, printing currently forces layout tree reattachment,
  // so we need to re-get layout_text_.
  UpdateLayoutText();

  PaintController controller;
  GraphicsContext context(controller);
  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLayoutText().GetDocument(), GetLayoutText().StyleRef(),
      CreatePaintInfoForBackground(context));
  EXPECT_EQ(Color(255, 220, 220).Dark(), text_style.fill_color);
  EXPECT_EQ(Color(220, 255, 220).Dark(), text_style.stroke_color);
  EXPECT_EQ(Color(220, 220, 255).Dark(), text_style.emphasis_mark_color);
}

TEST_F(TextPainterTest, CachedTextBlob) {
  auto& persistent_data =
      GetDocument().View()->GetPaintControllerPersistentDataForTesting();
  auto* item =
      DynamicTo<DrawingDisplayItem>(persistent_data.GetDisplayItemList()[1]);
  ASSERT_TRUE(item);
  auto* op = static_cast<const cc::DrawTextBlobOp*>(
      &item->GetPaintRecord().GetFirstOp());
  ASSERT_EQ(cc::PaintOpType::kDrawTextBlob, op->GetType());
  cc::PaintFlags flags = op->flags;
  sk_sp<SkTextBlob> blob = op->blob;

  // Should reuse text blob on color change.
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kColor, "red");
  UpdateAllLifecyclePhasesForTest();
  item = DynamicTo<DrawingDisplayItem>(persistent_data.GetDisplayItemList()[1]);
  ASSERT_TRUE(item);
  op = static_cast<const cc::DrawTextBlobOp*>(
      &item->GetPaintRecord().GetFirstOp());
  ASSERT_EQ(cc::PaintOpType::kDrawTextBlob, op->GetType());
  EXPECT_FALSE(flags.EqualsForTesting(op->flags));
  flags = op->flags;
  EXPECT_EQ(blob, op->blob);

  // Should not reuse text blob on font-size change.
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kFontSize,
                                               "30px");
  UpdateAllLifecyclePhasesForTest();
  item = DynamicTo<DrawingDisplayItem>(persistent_data.GetDisplayItemList()[1]);
  ASSERT_TRUE(item);
  op = static_cast<const cc::DrawTextBlobOp*>(
      &item->GetPaintRecord().GetFirstOp());
  ASSERT_EQ(cc::PaintOpType::kDrawTextBlob, op->GetType());
  EXPECT_TRUE(flags.EqualsForTesting(op->flags));
  EXPECT_NE(blob, op->blob);
  blob = op->blob;

  // Should not reuse text blob on text content change.
  GetDocument().body()->firstChild()->setTextContent("Hello, Hello");
  UpdateAllLifecyclePhasesForTest();
  item = DynamicTo<DrawingDisplayItem>(persistent_data.GetDisplayItemList()[1]);
  ASSERT_TRUE(item);
  op = static_cast<const cc::DrawTextBlobOp*>(
      &item->GetPaintRecord().GetFirstOp());
  ASSERT_EQ(cc::PaintOpType::kDrawTextBlob, op->GetType());
  EXPECT_TRUE(flags.EqualsForTesting(op->flags));
  EXPECT_NE(blob, op->blob);

  // In dark mode, the text should be drawn with dark mode flags.
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  UpdateAllLifecyclePhasesForTest();
  item = DynamicTo<DrawingDisplayItem>(persistent_data.GetDisplayItemList()[1]);
  ASSERT_TRUE(item);
  op = static_cast<const cc::DrawTextBlobOp*>(
      &item->GetPaintRecord().GetFirstOp());
  ASSERT_EQ(cc::PaintOpType::kDrawTextBlob, op->GetType());
  EXPECT_FALSE(flags.EqualsForTesting(op->flags));
}

}  // namespace
}  // namespace blink
