// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef PRINTING_IMAGE_H_
#define PRINTING_IMAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/check.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

// Lightweight raw-bitmap management. The image, once initialized, is immutable.
// The only purpose is testing image contents.
class Image {
 public:
  // Creates an image from raw ARGB pixel data, 32 bits per pixel.
  Image(gfx::Size size, int line_stride, std::vector<unsigned char> buffer);

  Image(const Image& image);
  Image& operator=(const Image& image) = delete;

  ~Image();

  bool operator==(const Image& other) const;

  const gfx::Size& size() const { return size_; }

  // Returns the 0x0RGB value of the pixel at the given location.
  uint32_t Color(uint32_t color) const {
    return color & 0xFFFFFF;  // Strip out alpha channel.
  }

  uint32_t pixel_at(int x, int y) const {
    DCHECK(x >= 0 && x < size_.width());
    DCHECK(y >= 0 && y < size_.height());
    const uint32_t* data = reinterpret_cast<const uint32_t*>(&*data_.begin());
    const uint32_t* data_row = data + y * row_length_ / sizeof(uint32_t);
    return Color(data_row[x]);
  }

 private:
  // Pixel dimensions of the image.
  const gfx::Size size_;

  // Length of a line in bytes.
  const int row_length_;

  // Actual bitmap data in arrays of RGBAs (so when loaded as uint32_t, it's
  // 0xABGR).
  const std::vector<unsigned char> data_;
};

}  // namespace printing

#endif  // PRINTING_IMAGE_H_
