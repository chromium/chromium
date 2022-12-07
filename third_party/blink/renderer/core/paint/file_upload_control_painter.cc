// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/file_upload_control_painter.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

void FileUploadControlPainter::PaintObject(const PaintInfo& paint_info,
                                           const PhysicalOffset& paint_offset) {
  if (layout_file_upload_control_.StyleRef().Visibility() !=
      EVisibility::kVisible)
    return;

  if (paint_info.phase == PaintPhase::kForeground &&
      !DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_file_upload_control_, paint_info.phase)) {
    const String& displayed_filename =
        layout_file_upload_control_.FileTextValue();
    const Font& font = layout_file_upload_control_.StyleRef().GetFont();
    TextRun text_run = ConstructTextRun(
        font, displayed_filename, layout_file_upload_control_.StyleRef(),
        kRespectDirection | kRespectDirectionOverride);
    text_run.SetExpansionBehavior(TextRun::kAllowTrailingExpansion);

    // Determine where the filename should be placed
    LayoutUnit content_left = paint_offset.left +
                              layout_file_upload_control_.BorderLeft() +
                              layout_file_upload_control_.PaddingLeft();
    Node* button = layout_file_upload_control_.UploadButton();
    if (!button)
      return;

    int button_width = (button && button->GetLayoutBox())
                           ? button->GetLayoutBox()->PixelSnappedWidth()
                           : 0;
    LayoutUnit button_and_spacing_width(
        button_width + (layout_file_upload_control_.StyleRef().EffectiveZoom() *
                        LayoutFileUploadControl::kAfterButtonSpacing));
    gfx::RectF text_bounds;
    float text_width = font.Width(text_run, nullptr, &text_bounds);
    LayoutUnit text_x;
    if (layout_file_upload_control_.StyleRef().IsLeftToRightDirection())
      text_x = content_left + button_and_spacing_width;
    else
      text_x =
          LayoutUnit(content_left + layout_file_upload_control_.ContentWidth() -
                     button_and_spacing_width - text_width);

    LayoutUnit text_y;
    // We want to match the button's baseline
    // FIXME: Make this work with transforms.
    if (LayoutBox* button_layout_object = button->GetLayoutBox())
      text_y = paint_offset.top + layout_file_upload_control_.BorderTop() +
               layout_file_upload_control_.PaddingTop() +
               button_layout_object->BaselinePosition(
                   kAlphabeticBaseline, true, kHorizontalLine,
                   kPositionOnContainingLine);
    else
      text_y = LayoutUnit(layout_file_upload_control_.BaselinePosition(
          kAlphabeticBaseline, true, kHorizontalLine,
          kPositionOnContainingLine));
    TextRunPaintInfo text_run_paint_info(text_run);

    // Draw the filename.
    BoxDrawingRecorder recorder(paint_info.context, layout_file_upload_control_,
                                paint_info.phase, paint_offset);
    paint_info.context.SetFillColor(
        layout_file_upload_control_.ResolveColor(GetCSSPropertyColor()));
    paint_info.context.DrawBidiText(
        font, text_run_paint_info,
        gfx::PointF(RoundToInt(text_x), RoundToInt(text_y)),
        PaintAutoDarkMode(layout_file_upload_control_.StyleRef(),
                          DarkModeFilter::ElementRole::kForeground));
    if (!font.ShouldSkipDrawing()) {
      ScopedPaintTimingDetectorBlockPaintHook
          scoped_paint_timing_detector_block_paint_hook;
      scoped_paint_timing_detector_block_paint_hook.EmplaceIfNeeded(
          layout_file_upload_control_, paint_info.context.GetPaintController()
                                           .CurrentPaintChunkProperties());
      text_bounds.Offset(text_x, text_y);
      PaintTimingDetector::NotifyTextPaint(ToEnclosingRect(text_bounds));
    }
  }

  // Paint the children.
  layout_file_upload_control_.LayoutBlockFlow::PaintObject(paint_info,
                                                           paint_offset);
}

}  // namespace blink
