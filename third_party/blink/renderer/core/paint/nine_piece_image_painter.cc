// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/nine_piece_image_painter.h"

#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/nine_piece_image_grid.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/scoped_interpolation_quality.h"

namespace blink {

namespace {

base::Optional<float> CalculateSpaceNeeded(const float destination,
                                           const float source) {
  DCHECK_GT(source, 0);
  DCHECK_GT(destination, 0);

  float repeat_tiles_count = floorf(destination / source);
  if (!repeat_tiles_count)
    return base::nullopt;

  float space = destination;
  space -= source * repeat_tiles_count;
  space /= repeat_tiles_count + 1.0;
  return space;
}

struct TileParameters {
  float scale_factor;
  float phase;
  float spacing;
};

base::Optional<TileParameters> ComputeTileParameters(
    ENinePieceImageRule tile_rule,
    float dst_pos,
    float dst_extent,
    float src_pos,
    float src_extent,
    float in_scale_factor) {
  switch (tile_rule) {
    case kRoundImageRule: {
      float repetitions =
          std::max(1.0f, roundf(dst_extent / (src_extent * in_scale_factor)));
      float scale_factor = dst_extent / (src_extent * repetitions);
      return TileParameters{scale_factor, src_pos * scale_factor, 0};
    }
    case kRepeatImageRule: {
      float scaled_tile_extent = src_extent * in_scale_factor;
      // We want to construct the phase such that the pattern is centered (when
      // stretch is not set for a particular rule).
      float phase = src_pos * in_scale_factor;
      phase -= (dst_extent - scaled_tile_extent) / 2;
      return TileParameters{in_scale_factor, phase, 0};
    }
    case kSpaceImageRule: {
      base::Optional<float> spacing =
          CalculateSpaceNeeded(dst_extent, src_extent);
      if (!spacing)
        return base::nullopt;
      return TileParameters{1, src_pos - *spacing, *spacing};
    }
    case kStretchImageRule:
      return TileParameters{in_scale_factor, src_pos * in_scale_factor, 0};
    default:
      NOTREACHED();
  }
  return base::nullopt;
}

void PaintPieces(GraphicsContext& context,
                 const PhysicalRect& border_image_rect,
                 const ComputedStyle& style,
                 const NinePieceImage& nine_piece_image,
                 Image* image,
                 IntSize image_size,
                 bool include_logical_left_edge,
                 bool include_logical_right_edge) {
  IntRectOutsets border_widths(style.BorderTopWidth(), style.BorderRightWidth(),
                               style.BorderBottomWidth(),
                               style.BorderLeftWidth());
  NinePieceImageGrid grid(
      nine_piece_image, image_size, PixelSnappedIntRect(border_image_rect),
      border_widths, include_logical_left_edge, include_logical_right_edge);

  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info = grid.GetNinePieceDrawInfo(
        piece, nine_piece_image.GetImage()->ImageScaleFactor());

    if (draw_info.is_drawable) {
      if (draw_info.is_corner_piece) {
        // Since there is no way for the developer to specify decode behavior,
        // use kSync by default.
        context.DrawImage(image, Image::kSyncDecode, draw_info.destination,
                          &draw_info.source, style.HasFilterInducingProperty());
      } else if (draw_info.tile_rule.horizontal == kStretchImageRule &&
                 draw_info.tile_rule.vertical == kStretchImageRule) {
        // Just do a scale.
        // Since there is no way for the developer to specify decode behavior,
        // use kSync by default.
        context.DrawImage(image, Image::kSyncDecode, draw_info.destination,
                          &draw_info.source, style.HasFilterInducingProperty());
      } else {
        // TODO(cavalcantii): see crbug.com/662513.
        base::Optional<TileParameters> h_tile = ComputeTileParameters(
            draw_info.tile_rule.horizontal, draw_info.destination.X(),
            draw_info.destination.Width(), draw_info.source.X(),
            draw_info.source.Width(), draw_info.tile_scale.Width());
        base::Optional<TileParameters> v_tile = ComputeTileParameters(
            draw_info.tile_rule.vertical, draw_info.destination.Y(),
            draw_info.destination.Height(), draw_info.source.Y(),
            draw_info.source.Height(), draw_info.tile_scale.Height());
        if (!h_tile || !v_tile)
          continue;

        FloatSize tile_scale_factor(h_tile->scale_factor, v_tile->scale_factor);
        FloatPoint tile_phase(draw_info.destination.X() - h_tile->phase,
                              draw_info.destination.Y() - v_tile->phase);
        FloatSize tile_spacing(h_tile->spacing, v_tile->spacing);

        // TODO(cavalcantii): see crbug.com/662507.
        base::Optional<ScopedInterpolationQuality> interpolation_quality_scope;
        if (draw_info.tile_rule.horizontal == kRoundImageRule ||
            draw_info.tile_rule.vertical == kRoundImageRule)
          interpolation_quality_scope.emplace(context, kInterpolationLow);

        context.DrawImageTiled(image, draw_info.destination, draw_info.source,
                               tile_scale_factor, tile_phase, tile_spacing);
      }
    }
  }
}

}  // anonymous namespace

bool NinePieceImagePainter::Paint(GraphicsContext& graphics_context,
                                  const ImageResourceObserver& observer,
                                  const Document& document,
                                  Node* node,
                                  const PhysicalRect& rect,
                                  const ComputedStyle& style,
                                  const NinePieceImage& nine_piece_image,
                                  bool include_logical_left_edge,
                                  bool include_logical_right_edge) {
  StyleImage* style_image = nine_piece_image.GetImage();
  if (!style_image)
    return false;

  if (!style_image->IsLoaded())
    return true;  // Never paint a nine-piece image incrementally, but don't
                  // paint the fallback borders either.

  if (!style_image->CanRender())
    return false;

  // FIXME: border-image is broken with full page zooming when tiling has to
  // happen, since the tiling function doesn't have any understanding of the
  // zoom that is in effect on the tile.
  PhysicalRect rect_with_outsets = rect;
  rect_with_outsets.Expand(style.ImageOutsets(nine_piece_image));
  PhysicalRect border_image_rect = rect_with_outsets;

  // NinePieceImage returns the image slices without effective zoom applied and
  // thus we compute the nine piece grid on top of the image in unzoomed
  // coordinates.
  //
  // FIXME: The default object size passed to imageSize() should be scaled by
  // the zoom factor passed in. In this case it means that borderImageRect
  // should be passed in compensated by effective zoom, since the scale factor
  // is one. For generated images, the actual image data (gradient stops, etc.)
  // are scaled to effective zoom instead so we must take care not to cause
  // scale of them again.
  IntSize image_size = RoundedIntSize(style_image->ImageSize(
      document, 1, border_image_rect.size.ToLayoutSize()));
  scoped_refptr<Image> image =
      style_image->GetImage(observer, document, style, FloatSize(image_size));
  if (!image)
    return true;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data",
               inspector_paint_image_event::Data(node, *style_image,
                                                 FloatRect(image->Rect()),
                                                 FloatRect(border_image_rect)));

  ScopedInterpolationQuality interpolation_quality_scope(
      graphics_context, style.GetInterpolationQuality());
  PaintPieces(graphics_context, border_image_rect, style, nine_piece_image,
              image.get(), image_size, include_logical_left_edge,
              include_logical_right_edge);

  return true;
}

}  // namespace blink
