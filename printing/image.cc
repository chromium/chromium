// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"

namespace printing {

Image::Image(gfx::Size size, int line_stride, std::vector<unsigned char> buffer)
    : size_(size), row_length_(line_stride), data_(std::move(buffer)) {}

Image::Image(const Image& image) = default;

Image::~Image() = default;

bool Image::operator==(const Image& other) const {
  return size_ == other.size_ && row_length_ == other.row_length_ &&
         data_ == other.data_;
}

uint32_t Image::pixel_at(int x, int y) const {
  CHECK_GE(x, 0);
  CHECK_LT(x, size_.width());
  CHECK_GE(y, 0);
  CHECK_LT(y, size_.height());
  const uint32_t* data = reinterpret_cast<const uint32_t*>(&*data_.begin());
  UNSAFE_TODO({
    const uint32_t* data_row = data + y * row_length_ / sizeof(uint32_t);
    return Color(data_row[x]);
  });
}

}  // namespace printing
