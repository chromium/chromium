// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_FRAME_TEST_UTIL_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_FRAME_TEST_UTIL_H_

#include <ostream>

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Rect;
class RectF;
}  // namespace gfx

namespace media {
class VideoFrame;
}  // namespace media

namespace content {

class FrameTestUtil {
 public:
  struct RGB {
    double r;
    double g;
    double b;
  };

  // Converts the image data in the given |frame| to a new SkBitmap. The result
  // uses the sRGB color space.
  static SkBitmap ConvertToBitmap(const media::VideoFrame& frame);

  // Helper functions to adjust the rects to account for the "fuzzy border"
  // between regions. These are typically used to generate the |include_rect|
  // and |exclude_rect| arguments passed to ComputeAverageColor() below. This
  // improves test accuracy, since the capturer's scaling can cause blended
  // colors at the border between regions. ToSafeIncludeRect() is used to
  // crop-out the outer fuzzy pixels of a region, while ToSafeExcludeRect()
  // is used to include an extra border of possible fuzziness around a region
  // that should be omitted.
  static gfx::Rect ToSafeIncludeRect(const gfx::RectF& rect_f,
                                     int fuzzy_border = 4);
  static gfx::Rect ToSafeExcludeRect(const gfx::RectF& rect_f,
                                     int fuzzy_border = 4);

  // Returns the average RGB color in |include_rect| except for pixels also in
  // |exclude_rect|.
  static RGB ComputeAverageColor(SkBitmap frame,
                                 const gfx::Rect& include_rect,
                                 const gfx::Rect& exclude_rect);

  // Returns true if the red, green, and blue components are all within
  // |max_diff| of each other.
  static bool IsApproximatelySameColor(SkColor color,
                                       const RGB& rgb,
                                       int max_diff = kMaxColorDifference);

  // Determines how |original| has been scaled and translated to become
  // |transformed|, and then applies the same transform on |rect| and returns
  // the result.
  static gfx::RectF TransformSimilarly(const gfx::Rect& original,
                                       const gfx::RectF& transformed,
                                       const gfx::Rect& rect);

  // The default maximum color value difference.
  static constexpr int kMaxColorDifference = 4;
  // Only use this for platform/use-case combinations where a trade-off has been
  // made explicitly for performance reasons.
  static constexpr int kVeryLooseMaxColorDifference = 64;
};

// A convenience for logging and gtest expectations output.
std::ostream& operator<<(std::ostream& out, const FrameTestUtil::RGB& rgb);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_FRAME_TEST_UTIL_H_
