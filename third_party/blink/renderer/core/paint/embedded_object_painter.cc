// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/embedded_object_painter.h"

#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/core/layout/layout_theme_font_provider.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/embedded_content_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

namespace {

constexpr float kReplacementTextRoundedRectHeight = 18;
constexpr float kReplacementTextRoundedRectLeftRightTextMargin = 6;
constexpr float kReplacementTextRoundedRectRadius = 5;

constexpr Color kReplacementTextRoundedRectColor =
    Color::FromRGBAFloat(1, 1, 1, 0.20f);
constexpr Color kReplacementTextTextColor =
    Color::FromRGBAFloat(0, 0, 0, 0.55f);

Font* ReplacementTextFont(const Document* document) {
  const AtomicString& family = LayoutThemeFontProvider::SystemFontFamily(
      CSSValueID::kWebkitSmallControl);
  const float size = LayoutThemeFontProvider::SystemFontSize(
      CSSValueID::kWebkitSmallControl, document);

  FontDescription font_description;
  font_description.SetFamily(
      FontFamily(family, FontFamily::InferredTypeFor(family)));
  font_description.SetWeight(kBoldWeightValue);
  font_description.SetSpecifiedSize(size);
  font_description.SetComputedSize(size);
  return MakeGarbageCollected<Font>(font_description);
}

}  // namespace

void EmbeddedObjectPainter::PaintReplaced(const PaintInfo& paint_info,
                                          const PhysicalOffset& paint_offset) {
  if (!layout_embedded_object_.ShowsUnavailablePluginIndicator()) {
    EmbeddedContentPainter(layout_embedded_object_)
        .PaintReplaced(paint_info, paint_offset);
    return;
  }

  if (paint_info.phase == PaintPhase::kSelectionDragImage)
    return;

  GraphicsContext& context = paint_info.context;
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, layout_embedded_object_, paint_info.phase))
    return;

  PhysicalRect content_rect = layout_embedded_object_.PhysicalContentBoxRect();
  content_rect.Move(paint_offset);
  BoxDrawingRecorder recorder(context, layout_embedded_object_,
                              paint_info.phase, paint_offset);

  Font* font = ReplacementTextFont(&layout_embedded_object_.GetDocument());
  const SimpleFontData* font_data = font->PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  TextRun text_run(layout_embedded_object_.UnavailablePluginReplacementText());
  gfx::SizeF text_geometry(
      PlainTextPainter::Shared().ComputeInlineSize(text_run, *font),
      font_data->GetFontMetrics().Height());

  PhysicalRect background_rect(
      LayoutUnit(), LayoutUnit(),
      LayoutUnit(text_geometry.width() +
                 2 * kReplacementTextRoundedRectLeftRightTextMargin),
      LayoutUnit(kReplacementTextRoundedRectHeight));
  background_rect.offset += content_rect.Center() - background_rect.Center();
  FloatRoundedRect rounded_background_rect(
      gfx::RectF(ToPixelSnappedRect(background_rect)),
      kReplacementTextRoundedRectRadius);
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(layout_embedded_object_.StyleRef(),
                        DarkModeFilter::ElementRole::kBackground));
  context.FillRoundedRect(rounded_background_rect,
                          kReplacementTextRoundedRectColor, auto_dark_mode);

  gfx::RectF text_rect(gfx::PointF(), text_geometry);
  text_rect.Offset(gfx::PointF(content_rect.Center()) -
                   text_rect.CenterPoint());
  context.SetFillColor(kReplacementTextTextColor);
  context.DrawBidiText(
      *font, text_run,
      text_rect.origin() +
          gfx::Vector2dF(0, font_data->GetFontMetrics().Ascent()),
      auto_dark_mode);
}

}  // namespace blink
