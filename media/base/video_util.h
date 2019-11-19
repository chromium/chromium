// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_UTIL_H_
#define MEDIA_BASE_VIDEO_UTIL_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoFrame;

// Computes the pixel aspect ratio of a given |visible_rect| from its
// |natural_size|.
//
// See https://en.wikipedia.org/wiki/Pixel_aspect_ratio for a detailed
// definition.
//
// Returns NaN or Infinity if |visible_rect| or |natural_size| are empty.
//
// Note: Something has probably gone wrong if you need to call this function;
// pixel aspect ratios should be the source of truth.
//
// TODO(crbug.com/837337): Decide how to encode 'not provided' for pixel aspect
// ratios, and return that if one of the inputs is empty.
MEDIA_EXPORT double GetPixelAspectRatio(const gfx::Rect& visible_rect,
                                        const gfx::Size& natural_size);

// Increases (at most) one of the dimensions of |visible_rect| to produce
// a |natural_size| with the given pixel aspect ratio.
//
// Returns gfx::Size() if |pixel_aspect_ratio| is not finite and positive.
MEDIA_EXPORT gfx::Size GetNaturalSize(const gfx::Rect& visible_rect,
                                      double pixel_aspect_ratio);

// Overload that takes the pixel aspect ratio as an integer fraction (and
// |visible_size| instead of |visible_rect|).
//
// Returns gfx::Size() if numerator or denominator are not positive.
MEDIA_EXPORT gfx::Size GetNaturalSize(const gfx::Size& visible_size,
                                      int aspect_ratio_numerator,
                                      int aspect_ratio_denominator);

// Fills |frame| containing YUV data to the given color values.
MEDIA_EXPORT void FillYUV(VideoFrame* frame, uint8_t y, uint8_t u, uint8_t v);

// Fills |frame| containing YUVA data with the given color values.
MEDIA_EXPORT void FillYUVA(VideoFrame* frame,
                           uint8_t y,
                           uint8_t u,
                           uint8_t v,
                           uint8_t a);

// Creates a border in |frame| such that all pixels outside of |view_area| are
// black. Only YV12 and ARGB format video frames are currently supported. If
// format is YV12, the size and position of |view_area| must be even to align
// correctly with the color planes.
MEDIA_EXPORT void LetterboxVideoFrame(VideoFrame* frame,
                                      const gfx::Rect& view_area);

// Rotates |src| plane by |rotation| degree with possible flipping vertically
// and horizontally.
// |rotation| is limited to {0, 90, 180, 270}.
// |width| and |height| are expected to be even numbers.
// Both |src| and |dest| planes are packed and have same |width| and |height|.
// When |width| != |height| and rotated by 90/270, only the maximum square
// portion located in the center is rotated. For example, for width=640 and
// height=480, the rotated area is 480x480 located from row 0 through 479 and
// from column 80 through 559. The leftmost and rightmost 80 columns are
// ignored for both |src| and |dest|.
// The caller is responsible for blanking out the margin area.
MEDIA_EXPORT void RotatePlaneByPixels(const uint8_t* src,
                                      uint8_t* dest,
                                      int width,
                                      int height,
                                      int rotation,  // Clockwise.
                                      bool flip_vert,
                                      bool flip_horiz);

// Return the largest centered rectangle with the same aspect ratio of |content|
// that fits entirely inside of |bounds|.  If |content| is empty, its aspect
// ratio would be undefined; and in this case an empty Rect would be returned.
MEDIA_EXPORT gfx::Rect ComputeLetterboxRegion(const gfx::Rect& bounds,
                                              const gfx::Size& content);

// Same as ComputeLetterboxRegion(), except ensure the result has even-numbered
// x, y, width, and height. |bounds| must already have even-numbered
// coordinates, but the |content| size can be anything.
//
// This is useful for ensuring content scaled and converted to I420 does not
// have color distortions around the edges in a letterboxed video frame. Note
// that, in cases where ComputeLetterboxRegion() would return a 1x1-sized Rect,
// this function could return either a 0x0-sized Rect or a 2x2-sized Rect.
MEDIA_EXPORT gfx::Rect ComputeLetterboxRegionForI420(const gfx::Rect& bounds,
                                                     const gfx::Size& content);

// Return a scaled |size| whose area is less than or equal to |target|, where
// one of its dimensions is equal to |target|'s.  The aspect ratio of |size| is
// preserved as closely as possible.  If |size| is empty, the result will be
// empty.
MEDIA_EXPORT gfx::Size ScaleSizeToFitWithinTarget(const gfx::Size& size,
                                                  const gfx::Size& target);

// Return a scaled |size| whose area is greater than or equal to |target|, where
// one of its dimensions is equal to |target|'s.  The aspect ratio of |size| is
// preserved as closely as possible.  If |size| is empty, the result will be
// empty.
MEDIA_EXPORT gfx::Size ScaleSizeToEncompassTarget(const gfx::Size& size,
                                                  const gfx::Size& target);

// Returns |size| with only one of its dimensions increased such that the result
// matches the aspect ratio of |target|.  This is different from
// ScaleSizeToEncompassTarget() in two ways: 1) The goal is to match the aspect
// ratio of |target| rather than that of |size|.  2) Only one of the dimensions
// of |size| may change, and it may only be increased (padded).  If either
// |size| or |target| is empty, the result will be empty.
MEDIA_EXPORT gfx::Size PadToMatchAspectRatio(const gfx::Size& size,
                                             const gfx::Size& target);

// Copy an RGB bitmap into the specified |region_in_frame| of a YUV video frame.
// Fills the regions outside |region_in_frame| with black.
MEDIA_EXPORT void CopyRGBToVideoFrame(const uint8_t* source,
                                      int stride,
                                      const gfx::Rect& region_in_frame,
                                      VideoFrame* frame);

// Converts a frame with YV12A format into I420 by dropping alpha channel.
MEDIA_EXPORT scoped_refptr<VideoFrame> WrapAsI420VideoFrame(
    scoped_refptr<VideoFrame> frame);

// Copy I420 video frame to match the required coded size and pad the region
// outside the visible rect repeatly with the last column / row up to the coded
// size of |dst_frame|. Return false when |dst_frame| is empty or visible rect
// is empty.
// One application is content mirroring using HW encoder. As the required coded
// size for encoder is unknown before capturing, memory copy is needed when the
// coded size does not match the requirement. Padding can improve the encoding
// efficiency in this case, as the encoder will encode the whole coded region.
// Performance-wise, this function could be expensive as it does memory copy of
// the whole visible rect.
// Note:
// 1. |src_frame| and |dst_frame| should have same size of visible rect.
// 2. The visible rect's origin of |dst_frame| should be (0,0).
// 3. |dst_frame|'s coded size (both width and height) should be larger than or
// equal to the visible size, since the visible region in both frames should be
// identical.
MEDIA_EXPORT bool I420CopyWithPadding(const VideoFrame& src_frame,
                                      VideoFrame* dst_frame) WARN_UNUSED_RESULT;

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_UTIL_H_
