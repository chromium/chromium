// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/shadow.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/rect.h"

namespace chrome_pdf {
namespace draw_utils {

constexpr uint8_t kOpaqueAlpha = 0xFF;
constexpr uint8_t kTransparentAlpha = 0x00;

inline uint8_t GetBlue(const uint32_t& pixel) {
  return static_cast<uint8_t>(pixel & 0xFF);
}

inline uint8_t GetGreen(const uint32_t& pixel) {
  return static_cast<uint8_t>((pixel >> 8) & 0xFF);
}

inline uint8_t GetRed(const uint32_t& pixel) {
  return static_cast<uint8_t>((pixel >> 16) & 0xFF);
}

inline uint8_t GetAlpha(const uint32_t& pixel) {
  return static_cast<uint8_t>((pixel >> 24) & 0xFF);
}

inline uint32_t MakePixel(uint8_t red,
                          uint8_t green,
                          uint8_t blue,
                          uint8_t alpha) {
  return (static_cast<uint32_t>(alpha) << 24) |
         (static_cast<uint32_t>(red) << 16) |
         (static_cast<uint32_t>(green) << 8) | static_cast<uint32_t>(blue);
}

inline uint8_t ProcessColor(uint8_t src_color,
                            uint8_t dest_color,
                            uint8_t alpha) {
  uint32_t processed = static_cast<uint32_t>(src_color) * alpha +
                       static_cast<uint32_t>(dest_color) * (0xFF - alpha);
  return static_cast<uint8_t>((processed / 0xFF) & 0xFF);
}

ShadowMatrix::ShadowMatrix(uint32_t depth, double factor, uint32_t background)
    : depth_(depth) {
  DCHECK_GT(depth_, 0U);
  matrix_.resize(depth_ * depth_);

  // pv - is a rounding power factor for smoothing corners.
  // pv = 2.0 will make corners completely round.
  constexpr double pv = 4.0;
  // pow_pv - cache to avoid recalculating pow(x, pv) every time.
  std::vector<double> pow_pv(depth_, 0.0);

  double r = static_cast<double>(depth_);
  double coef = 256.0 / pow(r, factor);

  for (uint32_t y = 0; y < depth_; y++) {
    // Since matrix is symmetrical, we can reduce the number of calculations
    // by mirroring results.
    for (uint32_t x = 0; x <= y; x++) {
      // Fill cache if needed.
      if (pow_pv[x] == 0.0)
        pow_pv[x] = pow(x, pv);
      if (pow_pv[y] == 0.0)
        pow_pv[y] = pow(y, pv);

      // v - is a value for the smoothing function.
      // If x == 0 simplify calculations.
      double v = (x == 0) ? y : pow(pow_pv[x] + pow_pv[y], 1 / pv);

      // Smoothing function.
      // If factor == 1, smoothing will be linear from 0 to the end,
      // if 0 < factor < 1, smoothing will drop faster near 0.
      // if factor > 1, smoothing will drop faster near the end (depth).
      double f = 256.0 - coef * pow(v, factor);

      uint8_t alpha = 0;
      if (f > kOpaqueAlpha)
        alpha = kOpaqueAlpha;
      else if (f < kTransparentAlpha)
        alpha = kTransparentAlpha;
      else
        alpha = static_cast<uint8_t>(f);

      uint8_t red = ProcessColor(0, GetRed(background), alpha);
      uint8_t green = ProcessColor(0, GetGreen(background), alpha);
      uint8_t blue = ProcessColor(0, GetBlue(background), alpha);
      uint32_t pixel = MakePixel(red, green, blue, GetAlpha(background));

      // Mirror matrix.
      matrix_[y * depth_ + x] = pixel;
      matrix_[x * depth_ + y] = pixel;
    }
  }
}

ShadowMatrix::~ShadowMatrix() = default;

namespace {

void PaintShadow(pp::ImageData* image,
                 const pp::Rect& clip_rc,
                 const pp::Rect& shadow_rc,
                 const ShadowMatrix& matrix) {
  pp::Rect draw_rc = shadow_rc.Intersect(clip_rc);
  if (draw_rc.IsEmpty())
    return;

  int32_t depth = static_cast<int32_t>(matrix.depth());
  for (int32_t y = draw_rc.y(); y < draw_rc.bottom(); y++) {
    for (int32_t x = draw_rc.x(); x < draw_rc.right(); x++) {
      int32_t matrix_x = std::max(depth + shadow_rc.x() - x - 1,
                                  depth - shadow_rc.right() + x);
      int32_t matrix_y = std::max(depth + shadow_rc.y() - y - 1,
                                  depth - shadow_rc.bottom() + y);
      uint32_t* pixel = image->GetAddr32(pp::Point(x, y));

      if (matrix_x < 0)
        matrix_x = 0;
      else if (matrix_x >= static_cast<int32_t>(depth))
        matrix_x = depth - 1;

      if (matrix_y < 0)
        matrix_y = 0;
      else if (matrix_y >= static_cast<int32_t>(depth))
        matrix_y = depth - 1;

      *pixel = matrix.GetValue(matrix_x, matrix_y);
    }
  }
}

}  // namespace

void DrawShadow(pp::ImageData* image,
                const pp::Rect& shadow_rc,
                const pp::Rect& object_rc,
                const pp::Rect& clip_rc,
                const ShadowMatrix& matrix) {
  if (shadow_rc == object_rc)
    return;  // Nothing to paint.

  // Fill top part.
  pp::Rect rc(shadow_rc.point(),
              pp::Size(shadow_rc.width(), object_rc.y() - shadow_rc.y()));
  PaintShadow(image, rc.Intersect(clip_rc), shadow_rc, matrix);

  // Fill bottom part.
  rc = pp::Rect(shadow_rc.x(), object_rc.bottom(), shadow_rc.width(),
                shadow_rc.bottom() - object_rc.bottom());
  PaintShadow(image, rc.Intersect(clip_rc), shadow_rc, matrix);

  // Fill left part.
  rc = pp::Rect(shadow_rc.x(), object_rc.y(), object_rc.x() - shadow_rc.x(),
                object_rc.height());
  PaintShadow(image, rc.Intersect(clip_rc), shadow_rc, matrix);

  // Fill right part.
  rc = pp::Rect(object_rc.right(), object_rc.y(),
                shadow_rc.right() - object_rc.right(), object_rc.height());
  PaintShadow(image, rc.Intersect(clip_rc), shadow_rc, matrix);
}

}  // namespace draw_utils
}  // namespace chrome_pdf
