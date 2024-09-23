// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_GEOMETRY_H_
#define SKIA_EXT_GEOMETRY_H_

#include <vector>

#include "skia/config/SkUserConfig.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"

namespace skia {

// Returns a rect that has the same relationship to `output_bounds` as
// `input_rect` has to `input_bounds`. The only requirement for valid
// results is that `output_bounds` and `input_bounds` be non-empty.
SK_API SkRect ScaleSkRectProportional(const SkRect& output_bounds,
                                      const SkRect& input_bounds,
                                      const SkRect& input_rect);

// Helper structure for tiling a shader that has several input SkImages.
// In order for a shader executing in an accelerated context to sample
// from an SkImage, the SkImage must be upload-able to a texture. If
// an SkImage is too large to fit in a texture, then the draw call using
// the shader must be chopped multiple tiled draw calls.
class SK_API Tiling {
 public:
  // Create a tiling for sampling `source_rects` when writing to
  // `dest_rect`. This will split `dest_rect` into tiles such that the
  // corresponding rect of each of `source_rects` will not have width
  // or height greater than `source_max_size`.
  Tiling(const SkRect& dest_rect,
         std::vector<SkRect> source_rects,
         std::vector<sk_sp<SkImage>> source_images,
         int32_t source_max_size);
  ~Tiling();

  // Return the number of horizontal and vertical tiles in the tiling.
  int GetTileCountX() const { return tile_count_.width(); }
  int GetTileCountY() const { return tile_count_.height(); }

  // For the tile at the specified coordiantes, populate:
  // - `tile_dest_rect` with the destination rect for the tile
  // - `tile_source_rects` with the source rect for the tile
  // - `tile_subset_rects` with the subset of the source image to use. If no
  //   subset of the image should be taken then it will be empty.
  void GetTileRect(
      int x,
      int y,
      SkRect& tile_dest_rect,
      std::vector<SkRect>& tile_source_rects,
      std::vector<std::optional<SkIRect>>& tile_source_subset_rects);

 private:
  // The destination rectangle for the drawRect call.
  SkRect dest_rect_;

  // The length of both `source_rects_` and `source_images_`.
  size_t source_count_ = 0;

  // The input rects that `source_images_` are sampled from.
  std::vector<SkRect> source_rects_;

  // The input images being sampled.
  std::vector<sk_sp<SkImage>> source_images_;

  // The computed size of the tiles and number of tiles.
  SkSize tile_size_;
  SkISize tile_count_;
};

}  // namespace skia

#endif  // SKIA_EXT_GEOMETRY_H_
