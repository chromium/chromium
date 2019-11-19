// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DRAW_UTILS_SHADOW_H_
#define PDF_DRAW_UTILS_SHADOW_H_

#include <stdint.h>

#include <vector>

namespace pp {
class ImageData;
class Rect;
}  // namespace pp

namespace chrome_pdf {
namespace draw_utils {

// Shadow Matrix contains matrix for shadow rendering. To reduce amount of
// calculations user may choose to cache matrix and reuse it if nothing changed.
class ShadowMatrix {
 public:
  // Matrix parameters.
  // depth - how big matrix should be. Shadow will go smoothly across the
  // entire matrix from black to background color.
  // If factor == 1, smoothing will be linear from 0 to the end (depth),
  // if 0 < factor < 1, smoothing will drop faster near 0.
  // if factor > 1, smoothing will drop faster near the end (depth).
  ShadowMatrix(uint32_t depth, double factor, uint32_t background);

  ~ShadowMatrix();

  uint32_t GetValue(int32_t x, int32_t y) const {
    return matrix_[y * depth_ + x];
  }

  uint32_t depth() const { return depth_; }

 private:
  const uint32_t depth_;
  std::vector<uint32_t> matrix_;
};

// Draw shadow on the image using provided ShadowMatrix.
// shadow_rc - rectangle occupied by shadow
// object_rc - rectangle that drops the shadow
// clip_rc - clipping region
void DrawShadow(pp::ImageData* image,
                const pp::Rect& shadow_rc,
                const pp::Rect& object_rc,
                const pp::Rect& clip_rc,
                const ShadowMatrix& matrix);

}  // namespace draw_utils
}  // namespace chrome_pdf

#endif  // PDF_DRAW_UTILS_SHADOW_H_
