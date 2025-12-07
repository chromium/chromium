// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_rect.h"

#include <algorithm>
#include <utility>

#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

gfx::RectF PdfRect::AsGfxRectF() const {
  return {/*x=*/left(), /*y=*/bottom(), /*width=*/width(), /*height=*/height()};
}

void PdfRect::Offset(float horizontal, float vertical) {
  left_ += horizontal;
  top_ += vertical;
  right_ += horizontal;
  bottom_ += vertical;
}

void PdfRect::Normalize() {
  if (top_ < bottom_) {
    std::swap(top_, bottom_);
  }
  if (right_ < left_) {
    std::swap(right_, left_);
  }
}

void PdfRect::Scale(float scale_factor) {
  left_ *= scale_factor;
  top_ *= scale_factor;
  right_ *= scale_factor;
  bottom_ *= scale_factor;
}

void PdfRect::Intersect(const PdfRect& rect) {
  if (IsEmpty() || rect.IsEmpty()) {
    *this = PdfRect();
    return;
  }

  float max_left = std::max(left_, rect.left());
  float min_top = std::min(top_, rect.top());
  float min_right = std::min(right_, rect.right());
  float max_bottom = std::max(bottom_, rect.bottom());
  if (max_left >= min_right || max_bottom >= min_top) {
    *this = PdfRect();
    return;
  }

  left_ = max_left;
  top_ = min_top;
  right_ = min_right;
  bottom_ = max_bottom;
}

void PdfRect::Union(const PdfRect& rect) {
  if (rect.IsEmpty()) {
    return;
  }

  if (IsEmpty()) {
    *this = rect;
    return;
  }

  left_ = std::min(left_, rect.left());
  top_ = std::max(top_, rect.top());
  right_ = std::max(right_, rect.right());
  bottom_ = std::min(bottom_, rect.bottom());
}

}  // namespace chrome_pdf
