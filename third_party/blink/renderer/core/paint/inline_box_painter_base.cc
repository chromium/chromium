// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_box_painter_base.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/nine_piece_image_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

PhysicalRect InlineBoxPainterBase::ClipRectForNinePieceImageStrip(
    const ComputedStyle& style,
    PhysicalBoxSides sides_to_include,
    const NinePieceImage& image,
    const PhysicalRect& paint_rect) {
  PhysicalRect clip_rect(paint_rect);
  LayoutRectOutsets outsets = style.ImageOutsets(image);
  if (sides_to_include.left) {
    clip_rect.SetX(paint_rect.X() - outsets.Left());
    clip_rect.SetWidth(paint_rect.Width() + outsets.Left());
  }
  if (sides_to_include.right)
    clip_rect.SetWidth(clip_rect.Width() + outsets.Right());
  if (sides_to_include.top) {
    clip_rect.SetY(paint_rect.Y() - outsets.Top());
    clip_rect.SetHeight(paint_rect.Height() + outsets.Top());
  }
  if (sides_to_include.bottom)
    clip_rect.SetHeight(clip_rect.Height() + outsets.Bottom());
  return clip_rect;
}

void InlineBoxPainterBase::PaintBoxDecorationBackground(
    BoxPainterBase& box_painter,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const PhysicalRect& adjusted_frame_rect,
    BackgroundImageGeometry geometry,
    bool object_has_multiple_boxes,
    PhysicalBoxSides sides_to_include) {
  // Shadow comes first and is behind the background and border.
  PaintNormalBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  Color background_color =
      line_style_.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  PaintFillLayers(box_painter, paint_info, background_color,
                  line_style_.BackgroundLayers(), adjusted_frame_rect, geometry,
                  object_has_multiple_boxes);

  PaintInsetBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  gfx::Rect adjusted_clip_rect;
  BorderPaintingType border_painting_type = GetBorderPaintType(
      adjusted_frame_rect, adjusted_clip_rect, object_has_multiple_boxes);
  switch (border_painting_type) {
    case kDontPaintBorders:
      break;
    case kPaintBordersWithoutClip:
      BoxPainterBase::PaintBorder(image_observer_, *document_, node_,
                                  paint_info, adjusted_frame_rect, line_style_,
                                  kBackgroundBleedNone, sides_to_include);
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
  paint_info.context.Clip(ToPixelSnappedRect(paint_rect));
  box_painter.PaintFillLayer(paint_info, c, fill_layer, rect,
                             kBackgroundBleedNone, geometry, multi_line,
                             paint_rect.size);
}

void InlineBoxPainterBase::PaintMask(BoxPainterBase& box_painter,
                                     const PaintInfo& paint_info,
                                     const PhysicalRect& paint_rect,
                                     BackgroundImageGeometry& geometry,
                                     bool object_has_multiple_boxes,
                                     PhysicalBoxSides sides_to_include) {
  // Figure out if we need to push a transparency layer to render our mask.
  PaintFillLayers(box_painter, paint_info, Color::kTransparent,
                  style_.MaskLayers(), paint_rect, geometry,
                  object_has_multiple_boxes);

  const auto& mask_nine_piece_image = style_.MaskBoxImage();
  const auto* mask_box_image = mask_nine_piece_image.GetImage();
  bool has_box_image = mask_box_image && mask_box_image->CanRender();
  if (!has_box_image || !mask_box_image->IsLoaded()) {
    // Don't paint anything while we wait for the image to load.
    return;
  }

  // The simple case is where we are the only box for this object. In those
  // cases only a single call to draw is required.
  PhysicalRect mask_image_paint_rect = paint_rect;
  GraphicsContextStateSaver state_saver(paint_info.context, false);
  if (object_has_multiple_boxes) {
    // We have a mask image that spans multiple lines.
    state_saver.Save();
    // FIXME: What the heck do we do with RTL here? The math we're using is
    // obviously not right, but it isn't even clear how this should work at all.
    mask_image_paint_rect =
        PaintRectForImageStrip(paint_rect, TextDirection::kLtr);
    gfx::RectF clip_rect(ClipRectForNinePieceImageStrip(
        style_, sides_to_include, mask_nine_piece_image, paint_rect));
    // TODO(chrishtr): this should be pixel-snapped.
    paint_info.context.Clip(clip_rect);
  }
  NinePieceImagePainter::Paint(paint_info.context, image_observer_, *document_,
                               node_, mask_image_paint_rect, style_,
                               mask_nine_piece_image);
}

}  // namespace blink
