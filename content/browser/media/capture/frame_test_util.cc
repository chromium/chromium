// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/frame_test_util.h"

#include <stdint.h>

#include <array>
#include <cmath>

#include "base/compiler_specific.h"
#include "base/containers/auto_spanification_helper.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "skia/ext/rgba_to_yuva.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace content {

// static
SkBitmap FrameTestUtil::ConvertToBitmap(const media::VideoFrame& frame) {
  CHECK(frame.ColorSpace().IsValid());

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(frame.visible_rect().width(),
                                                frame.visible_rect().height(),
                                                SkColorSpace::MakeSRGB()));
  SkPixmap pm = bitmap.pixmap();
  skia::ConvertYUVAToRGBA(frame.GetVisibleSkYUVAInfo(), frame.BitDepth(),
                          frame.GetVisiblePlanesSkPixmaps(), pm);
  return bitmap;
}

// static
gfx::Rect FrameTestUtil::ToSafeIncludeRect(const gfx::RectF& rect_f,
                                           int fuzzy_border) {
  gfx::Rect result = gfx::ToEnclosedRect(rect_f);
  CHECK_GT(result.width(), 2 * fuzzy_border);
  CHECK_GT(result.height(), 2 * fuzzy_border);
  result.Inset(fuzzy_border);
  return result;
}

// static
gfx::Rect FrameTestUtil::ToSafeExcludeRect(const gfx::RectF& rect_f,
                                           int fuzzy_border) {
  gfx::Rect result = gfx::ToEnclosingRect(rect_f);
  result.Inset(-fuzzy_border);
  return result;
}

// static
FrameTestUtil::RGB FrameTestUtil::ComputeAverageColor(
    SkBitmap frame,
    const gfx::Rect& raw_include_rect,
    const gfx::Rect& raw_exclude_rect) {
  // Clip the rects to the valid region within |frame|. Also, only the subregion
  // of |exclude_rect| within |include_rect| is relevant.
  gfx::Rect include_rect = raw_include_rect;
  include_rect.Intersect(gfx::Rect(0, 0, frame.width(), frame.height()));
  gfx::Rect exclude_rect = raw_exclude_rect;
  exclude_rect.Intersect(include_rect);

  // Sum up the color values in each color channel for all pixels in
  // |include_rect| not contained by |exclude_rect|.
  std::array<int64_t, 3> include_sums = {};
  for (int y = include_rect.y(), bottom = include_rect.bottom(); y < bottom;
       ++y) {
    for (int x = include_rect.x(), right = include_rect.right(); x < right;
         ++x) {
      if (exclude_rect.Contains(x, y)) {
        continue;
      }

      const SkColor color = frame.getColor(x, y);
      include_sums[0] += SkColorGetR(color);
      include_sums[1] += SkColorGetG(color);
      include_sums[2] += SkColorGetB(color);
    }
  }

  // Divide the sums by the area to compute the average color.
  const int include_area =
      include_rect.size().GetArea() - exclude_rect.size().GetArea();
  if (include_area <= 0) {
    return RGB{NAN, NAN, NAN};
  } else {
    const auto include_area_f = static_cast<double>(include_area);
    return RGB{include_sums[0] / include_area_f,
               include_sums[1] / include_area_f,
               include_sums[2] / include_area_f};
  }
}

// static
bool FrameTestUtil::IsApproximatelySameColor(SkColor color,
                                             const RGB& rgb,
                                             int max_diff) {
  const double r_diff = std::abs(SkColorGetR(color) - rgb.r);
  const double g_diff = std::abs(SkColorGetG(color) - rgb.g);
  const double b_diff = std::abs(SkColorGetB(color) - rgb.b);
  return r_diff < max_diff && g_diff < max_diff && b_diff < max_diff;
}

// static
bool FrameTestUtil::IsApproximatelySameColor(SkColor color,
                                             SkColor expected_color,
                                             int max_diff) {
  const int r_diff = std::abs(static_cast<int>(SkColorGetR(color)) -
                              static_cast<int>(SkColorGetR(expected_color)));
  const int g_diff = std::abs(static_cast<int>(SkColorGetG(color)) -
                              static_cast<int>(SkColorGetG(expected_color)));
  const int b_diff = std::abs(static_cast<int>(SkColorGetB(color)) -
                              static_cast<int>(SkColorGetB(expected_color)));
  return r_diff < max_diff && g_diff < max_diff && b_diff < max_diff;
}

bool FrameTestUtil::IsApproximatelySameColor(SkBitmap frame,
                                             const gfx::Rect& raw_include_rect,
                                             const gfx::Rect& raw_exclude_rect,
                                             SkColor expected_color,
                                             int max_diff) {
  // Clip the rects to the valid region within |frame|. Also, only the subregion
  // of |exclude_rect| within |include_rect| is relevant.
  gfx::Rect include_rect = raw_include_rect;
  include_rect.Intersect(gfx::Rect(0, 0, frame.width(), frame.height()));
  gfx::Rect exclude_rect = raw_exclude_rect;
  exclude_rect.Intersect(include_rect);

  for (int y = include_rect.y(), bottom = include_rect.bottom(); y < bottom;
       ++y) {
    for (int x = include_rect.x(), right = include_rect.right(); x < right;
         ++x) {
      if (exclude_rect.Contains(x, y)) {
        continue;
      }

      const SkColor color = frame.getColor(x, y);
      if (!IsApproximatelySameColor(color, expected_color, max_diff)) {
        return false;
      }
    }
  }

  return true;
}

// static
gfx::RectF FrameTestUtil::TransformSimilarly(const gfx::Rect& original,
                                             const gfx::RectF& transformed,
                                             const gfx::Rect& rect) {
  if (original.IsEmpty()) {
    return gfx::RectF(transformed.x() - original.x(),
                      transformed.y() - original.y(), 0.0f, 0.0f);
  }
  return gfx::MapRect(gfx::RectF(rect), gfx::RectF(original), transformed);
}

std::ostream& operator<<(std::ostream& out, const FrameTestUtil::RGB& rgb) {
  return (out << "{r=" << rgb.r << ",g=" << rgb.g << ",b=" << rgb.b << '}');
}

}  // namespace content
