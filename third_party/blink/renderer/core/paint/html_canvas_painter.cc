// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/html_canvas_painter.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/scoped_image_rendering_settings.h"

namespace blink {

namespace {

InterpolationQuality InterpolationQualityForCanvas(const ComputedStyle& style) {
  if (style.ImageRendering() == EImageRendering::kWebkitOptimizeContrast)
    return kInterpolationLow;

  if (style.ImageRendering() == EImageRendering::kPixelated)
    return kInterpolationNone;

  return CanvasDefaultInterpolationQuality;
}

}  // namespace

void HTMLCanvasPainter::PaintReplaced(const PaintInfo& paint_info,
                                      const PhysicalOffset& paint_offset) {
  GraphicsContext& context = paint_info.context;

  PhysicalRect paint_rect = layout_html_canvas_.ReplacedContentRect();
  paint_rect.Move(paint_offset);

  auto* canvas = To<HTMLCanvasElement>(layout_html_canvas_.GetNode());
  if (!canvas->IsCanvasClear()) {
    PaintTiming::From(layout_html_canvas_.GetDocument())
        .MarkFirstContentfulPaint();
  }

  if (auto* layer = canvas->ContentsCcLayer()) {
    // TODO(crbug.com/705019): For a texture layer canvas, setting the layer
    // background color to an opaque color will cause the layer to be treated as
    // opaque. For a surface layer canvas, contents could be opaque, but that
    // cannot be determined from the main thread. Or can it?
    if (layout_html_canvas_.DrawsBackgroundOntoContentLayer()) {
      Color background_color =
          layout_html_canvas_.ResolveColor(GetCSSPropertyBackgroundColor());
      // TODO(crbug/1308932): Remove FromColor and use just SkColor4f.
      layer->SetBackgroundColor(SkColor4f::FromColor(background_color.Rgb()));
    }
    // We do not take the foreign layer code path when printing because it
    // prevents painting canvas content as vector graphics.
    if (!paint_info.ShouldOmitCompositingInfo() && !canvas->IsPrinting()) {
      gfx::Rect pixel_snapped_rect = ToPixelSnappedRect(paint_rect);
      layer->SetBounds(pixel_snapped_rect.size());
      layer->SetIsDrawable(true);
      layer->SetHitTestable(true);
      RecordForeignLayer(context, layout_html_canvas_,
                         DisplayItem::kForeignLayerCanvas, layer,
                         pixel_snapped_rect.origin());
      return;
    }
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, layout_html_canvas_,
                                                  paint_info.phase))
    return;

  BoxDrawingRecorder recorder(context, layout_html_canvas_, paint_info.phase,
                              paint_offset);
  ScopedImageRenderingSettings image_rendering_settings_scope(
      context, InterpolationQualityForCanvas(layout_html_canvas_.StyleRef()),
      layout_html_canvas_.StyleRef().GetDynamicRangeLimit());
  canvas->Paint(context, paint_rect, paint_info.ShouldOmitCompositingInfo());
}

}  // namespace blink
