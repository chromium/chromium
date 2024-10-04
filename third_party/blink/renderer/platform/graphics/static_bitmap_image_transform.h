// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STATIC_BITMAP_IMAGE_TRANSFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STATIC_BITMAP_IMAGE_TRANSFORM_H_

#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// A helper class for transformations of a StaticBitmapImage that can
// potentially be accomplished by a blit. This includes the transforms caused
// by ImageBitmapOptions and cloning, but will be expanded to include
// changing the alpha type, color space, and pixel format.
class PLATFORM_EXPORT StaticBitmapImageTransform {
 public:
  // The parameters to the most generic Apply function that all other functions
  // are built upon.
  struct Params {
    // If true, then the source image must be copied (even if the transform
    // is a no-op, or can be accomplished without allocating a new backing).
    bool force_copy = false;

    // If true, then the final result should be flipped vertically. This happens
    // in the space after `source_orientation` has been applied.
    bool flip_y = false;
    bool premultiply_alpha = true;
    bool has_color_space_conversion = false;
    bool source_is_unpremul = false;
    bool orientation_from_image = true;

    // The sampling options to use. This will be set to nearest-neighbor if no
    // resampling is performed.
    SkSamplingOptions sampling;

    // The orientation of the source.
    class ImageOrientation source_orientation;

    // The `source_size`, `source_rect`, and `dest_size` parameters are all in
    // the space after the `source_orientation` has been applied.
    gfx::Size source_size;
    gfx::Rect source_rect;
    gfx::Size dest_size;

    // Compute the parameters for creating and then resizing a subset of the
    // source image. In the underlying PaintImage, `source_skrect` corresponds
    // to `source_rect`, `source_skrect_valid` corresponds to the intersection
    // of that with the PaintImage size, and `dest_sksize` corresponds to the
    // output size.
    void ComputeSubsetParameters(SkIRect& source_skrect,
                                 SkIRect& source_skrect_valid,
                                 SkISize& dest_sksize) const;

    bool MustPreserveUnpremulValues() const {
      return source_is_unpremul && !premultiply_alpha;
    }
  };

  // Apply the specified transform to the indcated image.
  static scoped_refptr<StaticBitmapImage> Apply(
      scoped_refptr<StaticBitmapImage> image,
      const Params& params);

  // Create a copy of the input image, with a newly created backing.
  static scoped_refptr<StaticBitmapImage> Clone(
      scoped_refptr<StaticBitmapImage> image);

 private:
  // Apply the specified transform by manipulating SkPixmaps in software. This
  // (SkPixmap::scalePixels) is the only path that allows resampling of images
  // while preserving unpremultiplied alpha values.
  static scoped_refptr<StaticBitmapImage> ApplyUsingPixmap(
      scoped_refptr<StaticBitmapImage> image,
      const Params& params);

  // Apply the specified transform by using a blit. The blit may be done on the
  // GPU or may be done in software. The result is always premultiplied.
  static scoped_refptr<StaticBitmapImage> ApplyWithBlit(
      scoped_refptr<StaticBitmapImage> image,
      const Params& params);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STATIC_BITMAP_IMAGE_TRANSFORM_H_
