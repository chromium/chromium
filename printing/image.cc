// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"

namespace printing {

Image::Image(gfx::Size size, int line_stride, std::vector<unsigned char> buffer)
    : size_(size), row_length_(line_stride), data_(std::move(buffer)) {}

Image::Image(const Image& image) = default;

Image::~Image() = default;

uint32_t Image::pixel_at(int x, int y) const {
  CHECK_GE(x, 0);
  CHECK_LT(x, size_.width());
  CHECK_GE(y, 0);
  CHECK_LT(y, size_.height());
  const auto row_bytes = base::span(data_).subspan(
      static_cast<size_t>(y) * row_length_, static_cast<size_t>(row_length_));
  const auto row = base::subtle::reinterpret_span<const uint32_t>(row_bytes);
  return Color(row[static_cast<size_t>(x)]);
}

}  // namespace printing
