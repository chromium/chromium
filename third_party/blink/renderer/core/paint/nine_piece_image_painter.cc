// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/nine_piece_image_painter.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/nine_piece_image_grid.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/scoped_image_rendering_settings.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

std::optional<float> CalculateSpaceNeeded(const float destination,
                                          const float source) {
  DCHECK_GT(source, 0);
  DCHECK_GT(destination, 0);

  float repeat_tiles_count = floorf(destination / source);
  if (!repeat_tiles_count)
    return std::nullopt;

  float space = destination;
  space -= source * repeat_tiles_count;
  space /= repeat_tiles_count + 1.0;
  return space;
}

struct TileParameters {
  float scale_factor;
  float phase;
  float spacing;
  STACK_ALLOCATED();
};

std::optional<TileParameters> ComputeTileParameters(
    ENinePieceImageRule tile_rule,
    float dst_extent,
    float src_extent) {
  switch (tile_rule) {
    case kRoundImageRule: {
      const float repetitions = std::max(1.0f, roundf(dst_extent / src_extent));
      const float scale_factor = dst_extent / (src_extent * repetitions);
      return TileParameters{scale_factor, 0, 0};
    }
    case kRepeatImageRule: {
      // We want to construct the phase such that the pattern is centered (when
      // stretch is not set for a particular rule).
      const float phase = (dst_extent - src_extent) / 2;
      return TileParameters{1, phase, 0};
    }
    case kSpaceImageRule: {
      const std::optional<float> spacing =
          CalculateSpaceNeeded(dst_extent, src_extent);
      if (!spacing)
        return std::nullopt;
      return TileParameters{1, *spacing, *spacing};
    }
    case kStretchImageRule:
      return TileParameters{1, 0, 0};
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return std::nullopt;
}

bool ShouldTile(const NinePieceImageGrid::NinePieceDrawInfo& draw_info) {
  // Corner pieces shouldn't be tiled.
  if (draw_info.is_corner_piece)
    return false;
  // If we're supposed to stretch in both dimensions, we can skip tiling
  // calculations.
  if (draw_info.tile_rule.horizontal == kStretchImageRule &&
      draw_info.tile_rule.vertical == kStretchImageRule)
    return false;
  return true;
}

void PaintPieces(GraphicsContext& context,
                 const PhysicalRect& border_image_rect,
                 const ComputedStyle& style,
                 const Document& document,
                 const NinePieceImage& nine_piece_image,
                 Image& image,
                 const gfx::SizeF& unzoomed_image_size,
                 PhysicalBoxSides sides_to_include) {
  const RespectImageOrientationEnum respect_orientation =
      style.ImageOrientation();
  // |image_size| is in the image's native resolution and |slice_scale| defines
  // the effective size of a CSS pixel in the image.
  const gfx::SizeF image_size = image.SizeAsFloat(respect_orientation);
  // Compute the scale factor to apply to the slice values by relating the
  // zoomed size to the "unzoomed" (CSS pixel) size. For raster images this
  // should match any DPR scale while for generated images it should match the
  // effective zoom. (Modulo imprecisions introduced by the computation.) This
  // scale should in theory be uniform.
  gfx::Vector2dF slice_scale(
      image_size.width() / unzoomed_image_size.width(),
      image_size.height() / unzoomed_image_size.height());

  auto border_widths =
      gfx::Outsets()
          .set_left_right(style.BorderLeftWidth(), style.BorderRightWidth())
          .set_top_bottom(style.BorderTopWidth(), style.BorderBottomWidth());
  NinePieceImageGrid grid(
      nine_piece_image, image_size, slice_scale, style.EffectiveZoom(),
      ToPixelSnappedRect(border_image_rect), border_widths, sides_to_include);

  // TODO(penglin):  We need to make a single classification for the entire grid
  auto image_auto_dark_mode = ImageAutoDarkMode::Disabled();

  ScopedImageRenderingSettings image_rendering_settings_scope(
      context, style.GetInterpolationQuality(), style.GetDynamicRangeLimit());
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    if (!draw_info.is_drawable)
      continue;

    if (!ShouldTile(draw_info)) {
      // When respecting image orientation, the drawing code expects the source
      // rect to be in the unrotated image space, but we have computed it here
      // in the rotated space in order to position and size the background. Undo
      // the src rect rotation if necessary.
      gfx::RectF src_rect = draw_info.source;
      if (respect_orientation && !image.HasDefaultOrientation()) {
        src_rect =
            image.CorrectSrcRectForImageOrientation(image_size, src_rect);
      }
      // Since there is no way for the developer to specify decode behavior,
      // use kSync by default.
      // TODO(sohom): Per crbug.com/1351498 investigate and set
      // ImagePaintTimingInfo parameters correctly
      context.DrawImage(image, Image::kSyncDecode, image_auto_dark_mode,
                        ImagePaintTimingInfo(), draw_info.destination,
                        &src_rect, SkBlendMode::kSrcOver, respect_orientation);
      continue;
    }

    // TODO(cavalcantii): see crbug.com/662513.
    const std::optional<TileParameters> h_tile = ComputeTileParameters(
        draw_info.tile_rule.horizontal, draw_info.destination.width(),
        draw_info.source.width() * draw_info.tile_scale.x());
    const std::optional<TileParameters> v_tile = ComputeTileParameters(
        draw_info.tile_rule.vertical, draw_info.destination.height(),
        draw_info.source.height() * draw_info.tile_scale.y());
    if (!h_tile || !v_tile)
      continue;

    ImageTilingInfo tiling_info;
    tiling_info.image_rect = draw_info.source;
    tiling_info.scale = gfx::ScaleVector2d(
        draw_info.tile_scale, h_tile->scale_factor, v_tile->scale_factor);
    // The phase defines the origin of the whole image - not the image
    // rect (see ImageTilingInfo) - so we need to adjust it to account
    // for that.
    gfx::PointF tile_origin_in_dest_space = draw_info.source.origin();
    tile_origin_in_dest_space.Scale(tiling_info.scale.x(),
                                    tiling_info.scale.y());
    tiling_info.phase =
        draw_info.destination.origin() +
        (gfx::PointF(h_tile->phase, v_tile->phase) - tile_origin_in_dest_space);
    tiling_info.spacing = gfx::SizeF(h_tile->spacing, v_tile->spacing);
    // TODO(sohom): Per crbug.com/1351498 investigate and set
    // ImagePaintTimingInfo parameters correctly
    context.DrawImageTiled(image, draw_info.destination, tiling_info,
                           image_auto_dark_mode, ImagePaintTimingInfo(),
                           SkBlendMode::kSrcOver, respect_orientation);
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
  const RespectImageOrientationEnum respect_orientation =
      style.ImageOrientation();
  const gfx::SizeF default_object_size(border_image_rect.size);
  gfx::SizeF image_size = style_image->ImageSize(
      style.EffectiveZoom(), default_object_size, respect_orientation);
  scoped_refptr<Image> image =
      style_image->GetImage(observer, document, style, image_size);
  if (!image)
    return true;

  // Resolve the image size again, this time with a size-multiplier of one, to
  // yield the size in CSS pixels. This is the unit/scale we expect the
  // 'border-image-slice' values to be in.
  gfx::SizeF unzoomed_image_size = style_image->ImageSize(
      1, gfx::ScaleSize(default_object_size, 1 / style.EffectiveZoom()),
      respect_orientation);

  DEVTOOLS_TIMELINE_TRACE_EVENT_WITH_CATEGORIES(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
      inspector_paint_image_event::Data, node, *style_image,
      gfx::RectF(image->Rect()), gfx::RectF(border_image_rect));
  PaintPieces(graphics_context, border_image_rect, style, document,
              nine_piece_image, *image, unzoomed_image_size, sides_to_include);
  return true;
}

}  // namespace blink
