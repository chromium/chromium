// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/draw_gainmap_image.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "skia/ext/geometry.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrRecordingContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/private/SkGainmapInfo.h"
#include "third_party/skia/include/private/SkGainmapShader.h"

namespace skia {

// Resample `gain_image` so that it has the same dimensions as `base_image`.
// Update `gain_rect` so that it has the same relationship to the new bounds
// of `gain_image` as it did to the old bounds. On failure, leave `gain_image`
// and `gain_rect` unchanged.
void ResampleGainmap(sk_sp<SkImage> base_image,
                     SkRect base_rect,
                     sk_sp<SkImage>& gain_image,
                     SkRect& gain_rect,
                     GrRecordingContext* context,
                     skgpu::graphite::Recorder* recorder) {
  SkImageInfo surface_info = gain_image->imageInfo()
                                 .makeDimensions(base_image->dimensions())
                                 .makeColorSpace(nullptr);
  sk_sp<SkSurface> surface;
#if defined(SK_GANESH)
  if (context) {
    surface =
        SkSurfaces::RenderTarget(context, skgpu::Budgeted::kNo, surface_info,
                                 /*sampleCount=*/0, kTopLeft_GrSurfaceOrigin,
                                 /*surfaceProps=*/nullptr,
                                 /*shouldCreateWithMips=*/false);
  }
#endif
#if defined(SK_GRAPHITE)
  if (recorder) {
    surface =
        SkSurfaces::RenderTarget(recorder, surface_info, skgpu::Mipmapped::kNo,
                                 /*surfaceProps=*/nullptr);
  }
#endif
  if (!context && !recorder) {
    surface = SkSurfaces::Raster(surface_info, surface_info.minRowBytes(),
                                 /*surfaceProps=*/nullptr);
  }
  if (!surface) {
    return;
  }

  SkRect dst_rect = SkRect::MakeSize(SkSize::Make(surface_info.dimensions()));
  SkRect src_rect = ScaleSkRectProportional(gain_rect, base_rect, dst_rect);
  surface->getCanvas()->drawImageRect(
      gain_image, src_rect, dst_rect, SkSamplingOptions(SkFilterMode::kLinear),
      nullptr, SkCanvas::kStrict_SrcRectConstraint);
  auto resampled_gain_image = surface->makeImageSnapshot();
  if (!resampled_gain_image) {
    return;
  }

  gain_image = resampled_gain_image;
  gain_rect = base_rect;
}

void DrawGainmapImageRect(SkCanvas* canvas,
                          sk_sp<SkImage> base_image,
                          sk_sp<SkImage> gain_image,
                          const SkGainmapInfo& gainmap_info,
                          float hdr_headroom,
                          const SkRect& source_rect,
                          const SkRect& dest_rect,
                          const SkSamplingOptions& sampling,
                          const SkPaint& paint) {
  // Compute `dest_rect_clipped` to be intersected with the pre-image of the
  // clip rect of `canvas`.
  SkRect dest_rect_clipped = dest_rect;
  const SkMatrix& dest_to_device = canvas->getTotalMatrix();
  SkMatrix device_to_dest;
  if (dest_to_device.invert(&device_to_dest)) {
    SkRect dest_clip_rect;
    device_to_dest.mapRect(&dest_clip_rect,
                           SkRect::Make(canvas->getDeviceClipBounds()));
    dest_rect_clipped.intersect(dest_clip_rect);
  }

  // Compute the input rect for the base and gainmap images.
  SkRect base_rect =
      skia::ScaleSkRectProportional(source_rect, dest_rect, dest_rect_clipped);
  SkRect gain_rect = skia::ScaleSkRectProportional(
      SkRect::Make(gain_image->bounds()), SkRect::Make(base_image->bounds()),
      base_rect);

  auto* context = canvas->recordingContext();
  auto* recorder = canvas->recorder();

  // Compute a tiling that ensures that the base and gainmap images fit on the
  // GPU.
  const std::vector<SkRect> source_rects = {base_rect, gain_rect};
  const std::vector<sk_sp<SkImage>> source_images = {base_image, gain_image};
  int max_texture_size = 0;
  if (context) {
    max_texture_size = context->maxTextureSize();
  } else if (recorder) {
    // TODO(b/279234024): Retrieve correct max texture size for graphite.
    max_texture_size = 8192;
  }
  skia::Tiling tiling(dest_rect_clipped, source_rects, source_images,
                      max_texture_size);

  // Draw tile-by-tile.
  for (int tx = 0; tx < tiling.GetTileCountX(); ++tx) {
    for (int ty = 0; ty < tiling.GetTileCountY(); ++ty) {
      // Retrieve the tile geometry.
      SkRect tile_dest_rect;
      std::vector<SkRect> tile_source_rects;
      std::vector<std::optional<SkIRect>> tile_subset_rects;
      tiling.GetTileRect(tx, ty, tile_dest_rect, tile_source_rects,
                         tile_subset_rects);

      // Extract the images' subsets (if needed).
      auto tile_source_images = source_images;
      for (size_t i = 0; i < 2; ++i) {
        if (!tile_subset_rects[i].has_value()) {
          continue;
        }
        if (tile_subset_rects[i]->isEmpty()) {
          tile_source_images[i] = nullptr;
        } else {
          tile_source_images[i] = tile_source_images[i]->makeSubset(
              nullptr, tile_subset_rects[i].value());
        }
      }

      // When nearnest-neighbor filtering is requested, first resample the
      // gainmap image to have the same size as the base image using linear
      // filtering. See b/341758170.
      if (sampling.filter == SkFilterMode::kNearest &&
          tile_source_rects[0] != tile_source_rects[1]) {
        ResampleGainmap(tile_source_images[0], tile_source_rects[0],
                        tile_source_images[1], tile_source_rects[1], context,
                        recorder);
      }

      // Draw the tile.
      SkPaint tile_paint = paint;
      auto shader = SkGainmapShader::Make(
          tile_source_images[0], tile_source_rects[0], sampling,
          tile_source_images[1], tile_source_rects[1], sampling, gainmap_info,
          tile_dest_rect, hdr_headroom, canvas->imageInfo().refColorSpace());
      tile_paint.setShader(std::move(shader));
      canvas->drawRect(tile_dest_rect, tile_paint);
    }
  }
}

void DrawGainmapImage(SkCanvas* canvas,
                      sk_sp<SkImage> base_image,
                      sk_sp<SkImage> gainmap_image,
                      const SkGainmapInfo& gainmap_info,
                      float hdr_headroom,
                      SkScalar left,
                      SkScalar top,
                      const SkSamplingOptions& sampling,
                      const SkPaint& paint) {
  SkRect source_rect = SkRect::Make(base_image->bounds());
  SkRect dest_rect =
      SkRect::MakeXYWH(left, top, base_image->width(), base_image->height());
  DrawGainmapImageRect(canvas, std::move(base_image), std::move(gainmap_image),
                       gainmap_info, hdr_headroom, source_rect, dest_rect,
                       sampling, paint);
}

}  // namespace skia
