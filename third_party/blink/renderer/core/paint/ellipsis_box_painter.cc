// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ellipsis_box_painter.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/line/ellipsis_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void EllipsisBoxPainter::Paint(const PaintInfo& paint_info,
                               const PhysicalOffset& paint_offset,
                               LayoutUnit line_top,
                               LayoutUnit line_bottom) {
  if (paint_info.phase == PaintPhase::kSelectionDragImage)
    return;

  const ComputedStyle& style = ellipsis_box_.GetLineLayoutItem().StyleRef(
      ellipsis_box_.IsFirstLineStyle());
  PaintEllipsis(paint_info, paint_offset, line_top, line_bottom, style);
}

void EllipsisBoxPainter::PaintEllipsis(const PaintInfo& paint_info,
                                       const PhysicalOffset& paint_offset,
                                       LayoutUnit line_top,
                                       LayoutUnit line_bottom,
                                       const ComputedStyle& style) {
  PhysicalOffset box_origin = ellipsis_box_.PhysicalLocation() + paint_offset;

  GraphicsContext& context = paint_info.context;
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, ellipsis_box_,
                                                  paint_info.phase))
    return;

  // If vertical, |box_rect| is in the physical coordinates space under the
  // rotation transform.
  PhysicalRect box_rect(box_origin,
                        PhysicalSize(ellipsis_box_.LogicalWidth(),
                                     ellipsis_box_.VirtualLogicalHeight()));
  DCHECK(ellipsis_box_.KnownToHaveNoOverflow());
  gfx::Rect visual_rect = ToEnclosingRect(box_rect);
  if (!ellipsis_box_.IsHorizontal())
    visual_rect.set_size(gfx::TransposeSize(visual_rect.size()));
  DrawingRecorder recorder(context, ellipsis_box_, paint_info.phase,
                           visual_rect);

  GraphicsContextStateSaver state_saver(context);
  if (!ellipsis_box_.IsHorizontal())
    context.ConcatCTM(TextPainter::Rotation(box_rect, TextPainter::kClockwise));

  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  const Document& document = ellipsis_box_.GetLineLayoutItem().GetDocument();
  TextPaintStyle text_style =
      TextPainter::TextPaintingStyle(document, style, paint_info);
  TextRun text_run = ConstructTextRun(font, ellipsis_box_.EllipsisStr(), style,
                                      TextRun::kAllowTrailingExpansion);
  PhysicalOffset text_origin(
      box_origin.left, box_origin.top + font_data->GetFontMetrics().Ascent());
  TextPainter text_painter(context, font, text_run, text_origin, box_rect,
                           ellipsis_box_.IsHorizontal());

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));

  text_painter.Paint(0, ellipsis_box_.EllipsisStr().length(),
                     ellipsis_box_.EllipsisStr().length(), text_style,
                     kInvalidDOMNodeId, auto_dark_mode);
  // TODO(npm): Check that there are non-whitespace characters. See
  // crbug.com/788444.
  context.GetPaintController().SetTextPainted();

  if (!font.ShouldSkipDrawing())
    PaintTimingDetector::NotifyTextPaint(visual_rect);
}

}  // namespace blink
