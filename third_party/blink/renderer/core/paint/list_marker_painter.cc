// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/list_marker_painter.h"

#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void ListMarkerPainter::PaintSymbol(const PaintInfo& paint_info,
                                    const LayoutObject* object,
                                    const ComputedStyle& style,
                                    const IntRect& marker) {
  DCHECK(object);
  GraphicsContext& context = paint_info.context;
  Color color(object->ResolveColor(GetCSSPropertyColor()));
  if (BoxModelObjectPainter::ShouldForceWhiteBackgroundForPrintEconomy(
          object->GetDocument(), style))
    color = TextPainter::TextColorForWhiteBackground(color);
  // Apply the color to the list marker text.
  context.SetFillColor(color);
  context.SetStrokeColor(color);
  context.SetStrokeStyle(kSolidStroke);
  context.SetStrokeThickness(1.0f);
  switch (style.ListStyleType()) {
    case EListStyleType::kDisc:
      context.FillEllipse(FloatRect(marker));
      break;
    case EListStyleType::kCircle:
      context.StrokeEllipse(FloatRect(marker));
      break;
    case EListStyleType::kSquare:
      context.FillRect(marker);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ListMarkerPainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground)
    return;

  if (layout_list_marker_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_list_marker_, paint_info.phase))
    return;

  ScopedPaintState paint_state(layout_list_marker_, paint_info);
  if (!paint_state.LocalRectIntersectsCullRect(
          layout_list_marker_.PhysicalVisualOverflowRect()))
    return;

  const auto& local_paint_info = paint_state.GetPaintInfo();
  auto box_origin = paint_state.PaintOffset().ToLayoutPoint();

  DrawingRecorder recorder(local_paint_info.context, layout_list_marker_,
                           local_paint_info.phase);

  LayoutRect box(box_origin, layout_list_marker_.Size());

  LayoutRect marker = layout_list_marker_.GetRelativeMarkerRect();
  marker.MoveBy(box_origin);

  GraphicsContext& context = local_paint_info.context;

  if (layout_list_marker_.IsImage()) {
    // Since there is no way for the developer to specify decode behavior, use
    // kSync by default.
    context.DrawImage(
        layout_list_marker_.GetImage()
            ->GetImage(layout_list_marker_, layout_list_marker_.GetDocument(),
                       layout_list_marker_.StyleRef(), FloatSize(marker.Size()))
            .get(),
        Image::kSyncDecode, FloatRect(marker));
    return;
  }

  LayoutListMarker::ListStyleCategory style_category =
      layout_list_marker_.GetListStyleCategory();
  if (style_category == LayoutListMarker::ListStyleCategory::kNone)
    return;

  if (style_category == LayoutListMarker::ListStyleCategory::kSymbol) {
    PaintSymbol(paint_info, &layout_list_marker_,
                layout_list_marker_.StyleRef(), PixelSnappedIntRect(marker));
    return;
  }

  if (layout_list_marker_.GetText().IsEmpty())
    return;

  Color color(layout_list_marker_.ResolveColor(GetCSSPropertyColor()));

  if (BoxModelObjectPainter::ShouldForceWhiteBackgroundForPrintEconomy(
          layout_list_marker_.ListItem()->GetDocument(),
          layout_list_marker_.StyleRef()))
    color = TextPainter::TextColorForWhiteBackground(color);

  // Apply the color to the list marker text.
  context.SetFillColor(color);

  const Font& font = layout_list_marker_.StyleRef().GetFont();
  TextRun text_run = ConstructTextRun(font, layout_list_marker_.GetText(),
                                      layout_list_marker_.StyleRef());

  GraphicsContextStateSaver state_saver(context, false);
  if (!layout_list_marker_.StyleRef().IsHorizontalWritingMode()) {
    marker.MoveBy(-box_origin);
    marker = marker.TransposedRect();
    marker.MoveBy(
        IntPoint(RoundToInt(box.X()),
                 RoundToInt(box.Y() - layout_list_marker_.LogicalHeight())));
    state_saver.Save();
    context.Translate(marker.X(), marker.MaxY());
    context.Rotate(static_cast<float>(deg2rad(90.)));
    context.Translate(-marker.X(), -marker.MaxY());
  }

  TextRunPaintInfo text_run_paint_info(text_run);
  const SimpleFontData* font_data =
      layout_list_marker_.StyleRef().GetFont().PrimaryFont();
  FloatPoint text_origin =
      FloatPoint(marker.X().Round(),
                 marker.Y().Round() +
                     (font_data ? font_data->GetFontMetrics().Ascent() : 0));

  // Text is not arbitrary. We can judge whether it's RTL from the first
  // character, and we only need to handle the direction RightToLeft for now.
  bool text_needs_reversing =
      WTF::unicode::Direction(layout_list_marker_.GetText()[0]) ==
      WTF::unicode::kRightToLeft;
  StringBuilder reversed_text;
  if (text_needs_reversing) {
    unsigned length = layout_list_marker_.GetText().length();
    reversed_text.ReserveCapacity(length);
    for (int i = length - 1; i >= 0; --i)
      reversed_text.Append(layout_list_marker_.GetText()[i]);
    DCHECK(reversed_text.length() == length);
    text_run.SetText(reversed_text.ToString());
  }

  if (style_category == LayoutListMarker::ListStyleCategory::kStaticString) {
    // Don't add a suffix.
    context.DrawText(font, text_run_paint_info, text_origin, kInvalidDOMNodeId);
    context.GetPaintController().SetTextPainted();
    return;
  }

  const UChar suffix =
      list_marker_text::Suffix(layout_list_marker_.StyleRef().ListStyleType(),
                               layout_list_marker_.ListItem()->Value());
  UChar suffix_str[2] = {suffix, static_cast<UChar>(' ')};
  TextRun suffix_run =
      ConstructTextRun(font, suffix_str, 2, layout_list_marker_.StyleRef(),
                       layout_list_marker_.StyleRef().Direction());
  TextRunPaintInfo suffix_run_info(suffix_run);

  if (layout_list_marker_.StyleRef().IsLeftToRightDirection()) {
    context.DrawText(font, text_run_paint_info, text_origin, kInvalidDOMNodeId);
    context.DrawText(font, suffix_run_info,
                     text_origin + FloatSize(IntSize(font.Width(text_run), 0)),
                     kInvalidDOMNodeId);
  } else {
    context.DrawText(font, suffix_run_info, text_origin, kInvalidDOMNodeId);
    // Is the truncation to IntSize below meaningful or a bug?
    context.DrawText(
        font, text_run_paint_info,
        text_origin + FloatSize(IntSize(font.Width(suffix_run), 0)),
        kInvalidDOMNodeId);
  }
  // TODO(npm): Check that there are non-whitespace characters. See
  // crbug.com/788444.
  context.GetPaintController().SetTextPainted();
}

}  // namespace blink
