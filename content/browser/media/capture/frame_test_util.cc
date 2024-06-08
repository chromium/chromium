// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/media/capture/frame_test_util.h"

#include <stdint.h>

#include <cmath>

#include "base/numerics/safe_conversions.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace content {

namespace {

using TriStim = gfx::ColorTransform::TriStim;

// Copies YUV row data into an array of TriStims, mapping [0,255]⇒[0.0,1.0]. The
// chroma planes are assumed to be half-width.
void LoadStimsFromYUV(const uint8_t y_src[],
                      const uint8_t u_src[],
                      const uint8_t v_src[],
                      int width,
                      TriStim stims[]) {
  for (int i = 0; i < width; ++i) {
    stims[i].SetPoint(y_src[i] / 255.0f, u_src[i / 2] / 255.0f,
                      v_src[i / 2] / 255.0f);
  }
}

void LoadStimsFromYUV(const uint8_t y_src[],
                      const uint16_t uv_src[],
                      int width,
                      TriStim stims[]) {
// https://docs.microsoft.com/en-us/windows/win32/medfound/recommended-8-bit-yuv-formats-for-video-rendering#nv12
// "All of the Y samples appear first in memory as an array of unsigned char
// values with an even number of lines. The Y plane is followed immediately by
// an array of unsigned char values that contains packed U (Cb) and V (Cr)
// samples. When the combined U-V array is addressed as an array of
// little-endian WORD values, the LSBs contain the U values, and the MSBs
// contain the V values."
#if defined(SK_CPU_BENDIAN)
  for (int i = 0; i < width; ++i) {
    stims[i].SetPoint(
        y_src[i] / 255.0f,
        (uv_src[i / 2] >> 8) / 255.0f,  // MSB contains U values on LE
        (uv_src[i / 2] & 0xFF) / 255.0f);
  }
#else
  for (int i = 0; i < width; ++i) {
    stims[i].SetPoint(
        y_src[i] / 255.0f,
        (uv_src[i / 2] & 0xFF) / 255.0f,  // LSB contains U values on LE
        (uv_src[i / 2] >> 8) / 255.0f);
  }
#endif
}

// Maps [0.0,1.0]⇒[0,255], rounding to the nearest integer.
uint8_t QuantizeAndClamp(float value) {
  return base::saturated_cast<uint8_t>(
      std::fma(value, 255.0f, 0.5f /* rounding */));
}

// Copies the array of TriStims to the BGRA/RGBA output, mapping
// [0.0,1.0]⇒[0,255].
void StimsToN32Row(const TriStim row[], int width, uint8_t bgra_out[]) {
  for (int i = 0; i < width; ++i) {
    bgra_out[(i * 4) + (SK_R32_SHIFT / 8)] = QuantizeAndClamp(row[i].x());
    bgra_out[(i * 4) + (SK_G32_SHIFT / 8)] = QuantizeAndClamp(row[i].y());
    bgra_out[(i * 4) + (SK_B32_SHIFT / 8)] = QuantizeAndClamp(row[i].z());
    bgra_out[(i * 4) + (SK_A32_SHIFT / 8)] = 255;
  }
}

}  // namespace

// static
SkBitmap FrameTestUtil::ConvertToBitmap(const media::VideoFrame& frame) {
  CHECK(frame.ColorSpace().IsValid());

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(frame.visible_rect().width(),
                                                frame.visible_rect().height(),
                                                SkColorSpace::MakeSRGB()));

  // Note: The hand-optimized libyuv::H420ToARGB() would be more-desirable for
  // runtime performance. However, while it claims to convert from the REC709
  // color space to sRGB, as of this writing, it does not do so accurately. For
  // example, a YUV triplet that should become almost exactly yellow (0xffff01)
  // is converted as 0xfeff0c (the blue channel has a difference of 11!). Since
  // one goal of these tests is to confirm color space correctness, the
  // following color transformation code is provided:

  // Construct the ColorTransform.
  const auto transform = gfx::ColorTransform::NewColorTransform(
      frame.ColorSpace(), gfx::ColorSpace::CreateSRGB());
  CHECK(transform);

  // Convert one row at a time.
  std::vector<gfx::ColorTransform::TriStim> stims(bitmap.width());
  for (int row = 0; row < bitmap.height(); ++row) {
    if (frame.format() == media::VideoPixelFormat::PIXEL_FORMAT_I420) {
      LoadStimsFromYUV(
          frame.visible_data(media::VideoFrame::Plane::kY) +
              row * frame.stride(media::VideoFrame::Plane::kY),
          frame.visible_data(media::VideoFrame::Plane::kU) +
              (row / 2) * frame.stride(media::VideoFrame::Plane::kU),
          frame.visible_data(media::VideoFrame::Plane::kV) +
              (row / 2) * frame.stride(media::VideoFrame::Plane::kV),
          bitmap.width(), stims.data());
    } else {
      CHECK_EQ(frame.format(), media::VideoPixelFormat::PIXEL_FORMAT_NV12);
      LoadStimsFromYUV(
          frame.visible_data(media::VideoFrame::Plane::kY) +
              row * frame.stride(media::VideoFrame::Plane::kY),
          reinterpret_cast<const uint16_t*>(
              frame.visible_data(media::VideoFrame::Plane::kUV) +
              (row / 2) * frame.stride(media::VideoFrame::Plane::kUV)),
          bitmap.width(), stims.data());
    }
    transform->Transform(stims.data(), stims.size());
    StimsToN32Row(stims.data(), bitmap.width(),
                  reinterpret_cast<uint8_t*>(bitmap.getAddr32(0, row)));
  }

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
  int64_t include_sums[3] = {0};
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
