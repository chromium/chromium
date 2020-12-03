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
                 const FloatSize& unzoomed_image_size,
                 PhysicalBoxSides sides_to_include) {
  // |image_size| is in the image's native resolution and |slice_scale| defines
  // the effective size of a CSS pixel in the image.
  FloatSize image_size = image->SizeAsFloat(kRespectImageOrientation);
  // Compute the scale factor to apply to the slice values by relating the
  // zoomed size to the "unzoomed" (CSS pixel) size. For raster images this
  // should match any DPR scale while for generated images it should match the
  // effective zoom. (Modulo imprecisions introduced by the computation.) This
  // scale should in theory be uniform.
  FloatSize slice_scale(image_size.Width() / unzoomed_image_size.Width(),
                        image_size.Height() / unzoomed_image_size.Height());

  // TODO(fs): Use FloatSize here to avoid additional rounding (leave that to
  // NinePieceImageGrid if needed). For narrow slices the rounding can introduce
  // large errors (fairly visible in the TC in crbug.com/596075 when zooming).
  IntSize rounded_image_size = RoundedIntSize(image_size);
  IntRectOutsets border_widths(style.BorderTopWidth(), style.BorderRightWidth(),
                               style.BorderBottomWidth(),
                               style.BorderLeftWidth());
  NinePieceImageGrid grid(
      nine_piece_image, rounded_image_size, slice_scale, style.EffectiveZoom(),
      PixelSnappedIntRect(border_image_rect), border_widths, sides_to_include);

  ScopedInterpolationQuality interpolation_quality_scope(
      context, style.GetInterpolationQuality());
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);

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
        base::Optional<ScopedInterpolationQuality>
            interpolation_quality_override;
        if (draw_info.tile_rule.horizontal == kRoundImageRule ||
            draw_info.tile_rule.vertical == kRoundImageRule)
          interpolation_quality_override.emplace(context, kInterpolationMedium);

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
                                  PhysicalBoxSides sides_to_include) {
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

  // Resolve the image size for any image that may need it (for example
  // generated or SVG), then get an image using that size. This will yield an
  // image with either "native" size (raster images) or size scaled by effective
  // zoom.
  const FloatSize default_object_size(border_image_rect.size);
  FloatSize image_size =
      style_image->ImageSize(document, style.EffectiveZoom(),
                             default_object_size, kRespectImageOrientation);
  scoped_refptr<Image> image =
      style_image->GetImage(observer, document, style, image_size);
  if (!image)
    return true;

  // Resolve the image size again, this time with a size-multiplier of one, to
  // yield the size in CSS pixels. This is the unit/scale we expect the
  // 'border-image-slice' values to be in.
  FloatSize unzoomed_image_size = style_image->ImageSize(
      document, 1, default_object_size.ScaledBy(1 / style.EffectiveZoom()),
      kRespectImageOrientation);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data",
               inspector_paint_image_event::Data(node, *style_image,
                                                 FloatRect(image->Rect()),
                                                 FloatRect(border_image_rect)));
  PaintPieces(graphics_context, border_image_rect, style, nine_piece_image,
              image.get(), unzoomed_image_size, sides_to_include);
  return true;
}

}  // namespace blink
