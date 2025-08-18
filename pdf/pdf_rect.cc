// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_rect.h"

#include <algorithm>
#include <utility>

namespace chrome_pdf {

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
  left_ = std::max(left_, rect.left_);
  top_ = std::min(top_, rect.top_);
  right_ = std::min(right_, rect.right_);
  bottom_ = std::max(bottom_, rect.bottom_);
}

}  // namespace chrome_pdf
