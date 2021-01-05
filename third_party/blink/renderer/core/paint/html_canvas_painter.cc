// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/html_canvas_painter.h"

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/scoped_interpolation_quality.h"

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

  bool flatten_composited_layers =
      paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers;

  if (auto* layer = canvas->ContentsCcLayer()) {
    // TODO(crbug.com/705019): For a texture layer canvas, setting the layer
    // background color to an opaque color will cause the layer to be treated as
    // opaque. For a surface layer canvas, contents could be opaque, but that
    // cannot be determined from the main thread. Or can it?
    if (layout_html_canvas_.DrawsBackgroundOntoContentLayer()) {
      Color background_color =
          layout_html_canvas_.ResolveColor(GetCSSPropertyBackgroundColor());
      layer->SetBackgroundColor(background_color.Rgb());
    }
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
        !flatten_composited_layers) {
      IntRect pixel_snapped_rect = PixelSnappedIntRect(paint_rect);
      layer->SetBounds(gfx::Size(pixel_snapped_rect.Size()));
      layer->SetIsDrawable(true);
      layer->SetHitTestable(true);
      RecordForeignLayer(context, layout_html_canvas_,
                         DisplayItem::kForeignLayerCanvas, layer,
                         pixel_snapped_rect.Location());
      return;
    }
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, layout_html_canvas_,
                                                  paint_info.phase))
    return;

  BoxDrawingRecorder recorder(context, layout_html_canvas_, paint_info.phase,
                              paint_offset);
  ScopedInterpolationQuality interpolation_quality_scope(
      context, InterpolationQualityForCanvas(layout_html_canvas_.StyleRef()));
  canvas->Paint(context, paint_rect, flatten_composited_layers);
}

}  // namespace blink
