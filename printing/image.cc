// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include <utility>

namespace printing {

Image::Image(gfx::Size size, int line_stride, std::vector<unsigned char> buffer)
    : size_(size), row_length_(line_stride), data_(std::move(buffer)) {}

Image::Image(const Image& image) = default;

Image::~Image() = default;

bool Image::operator==(const Image& other) const {
  return size_ == other.size_ && row_length_ == other.row_length_ &&
         data_ == other.data_;
}

}  // namespace printing
