// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_box_painter_base.h"

#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void InlineBoxPainterBase::PaintBoxDecorationBackground(
    BoxPainterBase& box_painter,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const PhysicalRect& adjusted_frame_rect,
    BackgroundImageGeometry geometry,
    bool object_has_multiple_boxes,
    bool include_logical_left_edge,
    bool include_logical_right_edge) {
  // Shadow comes first and is behind the background and border.
  PaintNormalBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  Color background_color =
      line_style_.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  PaintFillLayers(box_painter, paint_info, background_color,
                  line_style_.BackgroundLayers(), adjusted_frame_rect, geometry,
                  object_has_multiple_boxes);

  PaintInsetBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  IntRect adjusted_clip_rect;
  BorderPaintingType border_painting_type = GetBorderPaintType(
      adjusted_frame_rect, adjusted_clip_rect, object_has_multiple_boxes);
  switch (border_painting_type) {
    case kDontPaintBorders:
      break;
    case kPaintBordersWithoutClip:
      BoxPainterBase::PaintBorder(
          image_observer_, *document_, node_, paint_info, adjusted_frame_rect,
          line_style_, kBackgroundBleedNone, include_logical_left_edge,
          include_logical_right_edge);
      break;
    case kPaintBordersWithClip:
      // FIXME: What the heck do we do with RTL here? The math we're using is
      // obviously not right, but it isn't even clear how this should work at
      // all.
      PhysicalRect image_strip_paint_rect =
          PaintRectForImageStrip(adjusted_frame_rect, TextDirection::kLtr);
      GraphicsContextStateSaver state_saver(paint_info.context);
      paint_info.context.Clip(adjusted_clip_rect);
      BoxPainterBase::PaintBorder(image_observer_, *document_, node_,
                                  paint_info, image_strip_paint_rect,
                                  line_style_);
      break;
  }
}

void InlineBoxPainterBase::PaintFillLayers(BoxPainterBase& box_painter,
                                           const PaintInfo& info,
                                           const Color& c,
                                           const FillLayer& layer,
                                           const PhysicalRect& rect,
                                           BackgroundImageGeometry& geometry,
                                           bool object_has_multiple_boxes) {
  // FIXME: This should be a for loop or similar. It's a little non-trivial to
  // do so, however, since the layers need to be painted in reverse order.
  if (layer.Next()) {
    PaintFillLayers(box_painter, info, c, *layer.Next(), rect, geometry,
                    object_has_multiple_boxes);
  }
  PaintFillLayer(box_painter, info, c, layer, rect, geometry,
                 object_has_multiple_boxes);
}

void InlineBoxPainterBase::PaintFillLayer(BoxPainterBase& box_painter,
                                          const PaintInfo& paint_info,
                                          const Color& c,
                                          const FillLayer& fill_layer,
                                          const PhysicalRect& paint_rect,
                                          BackgroundImageGeometry& geometry,
                                          bool object_has_multiple_boxes) {
  StyleImage* img = fill_layer.GetImage();
  bool has_fill_image = img && img->CanRender();

  if (!object_has_multiple_boxes ||
      (!has_fill_image && !style_.HasBorderRadius())) {
    box_painter.PaintFillLayer(paint_info, c, fill_layer, paint_rect,
                               kBackgroundBleedNone, geometry, false);
    return;
  }

  // Handle fill images that clone or spans multiple lines.
  bool multi_line = object_has_multiple_boxes &&
                    style_.BoxDecorationBreak() != EBoxDecorationBreak::kClone;
  PhysicalRect rect =
      multi_line ? PaintRectForImageStrip(paint_rect, style_.Direction())
                 : paint_rect;
  GraphicsContextStateSaver state_saver(paint_info.context);
  paint_info.context.Clip(PixelSnappedIntRect(paint_rect));
  box_painter.PaintFillLayer(paint_info, c, fill_layer, rect,
                             kBackgroundBleedNone, geometry, multi_line,
                             paint_rect.size);
}

}  // namespace blink
