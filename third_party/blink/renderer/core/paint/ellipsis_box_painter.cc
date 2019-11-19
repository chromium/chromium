// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ellipsis_box_painter.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/line/ellipsis_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void EllipsisBoxPainter::Paint(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset,
                               LayoutUnit line_top,
                               LayoutUnit line_bottom) {
  if (paint_info.phase == PaintPhase::kSelection)
    return;

  const ComputedStyle& style = ellipsis_box_.GetLineLayoutItem().StyleRef(
      ellipsis_box_.IsFirstLineStyle());
  PaintEllipsis(paint_info, paint_offset, line_top, line_bottom, style);
}

void EllipsisBoxPainter::PaintEllipsis(const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset,
                                       LayoutUnit line_top,
                                       LayoutUnit line_bottom,
                                       const ComputedStyle& style) {
  LayoutPoint box_origin = ellipsis_box_.PhysicalLocation().ToLayoutPoint();
  box_origin.MoveBy(paint_offset);

  GraphicsContext& context = paint_info.context;
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, ellipsis_box_,
                                                  paint_info.phase))
    return;

  DrawingRecorder recorder(context, ellipsis_box_, paint_info.phase);

  LayoutRect box_rect(box_origin,
                      LayoutSize(ellipsis_box_.LogicalWidth(),
                                 ellipsis_box_.VirtualLogicalHeight()));

  GraphicsContextStateSaver state_saver(context);
  if (!ellipsis_box_.IsHorizontal()) {
    context.ConcatCTM(TextPainter::Rotation(PhysicalRectToBeNoop(box_rect),
                                            TextPainter::kClockwise));
  }

  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      ellipsis_box_.GetLineLayoutItem().GetDocument(), style, paint_info);
  TextRun text_run = ConstructTextRun(font, ellipsis_box_.EllipsisStr(), style,
                                      TextRun::kAllowTrailingExpansion);
  LayoutPoint text_origin(
      box_origin.X(), box_origin.Y() + font_data->GetFontMetrics().Ascent());
  TextPainter text_painter(context, font, text_run, text_origin, box_rect,
                           ellipsis_box_.IsHorizontal());
  text_painter.Paint(0, ellipsis_box_.EllipsisStr().length(),
                     ellipsis_box_.EllipsisStr().length(), text_style,
                     kInvalidDOMNodeId);
  // TODO(npm): Check that there are non-whitespace characters. See
  // crbug.com/788444.
  context.GetPaintController().SetTextPainted();

  if (!font.ShouldSkipDrawing())
    PaintTimingDetector::NotifyTextPaint(ellipsis_box_.VisualRect());
}

}  // namespace blink
