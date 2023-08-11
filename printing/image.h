// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_IMAGE_H_
#define PRINTING_IMAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

class Metafile;

// Lightweight raw-bitmap management. The image, once initialized, is immutable.
// The only purpose is testing image contents.
class Image {
 public:
  // Creates the image from the metafile.  Deduces bounds based on bounds in
  // metafile.  If loading fails size().IsEmpty() will be true.
  explicit Image(const Metafile& metafile);

  // Copy constructor.
  explicit Image(const Image& image);

  ~Image();

  const gfx::Size& size() const { return size_; }

  // Return a checksum of the image (MD5 over the internal data structure).
  std::string checksum() const;

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
  // Loads the first page from `metafile`.
  bool LoadMetafile(const Metafile& metafile);

  // Pixel dimensions of the image.
  gfx::Size size_;

  // Length of a line in bytes.
  int row_length_;

  // Actual bitmap data in arrays of RGBAs (so when loaded as uint32_t, it's
  // 0xABGR).
  std::vector<unsigned char> data_;

  // Prevent operator= (this function has no implementation)
  Image& operator=(const Image& image);
};

}  // namespace printing

#endif  // PRINTING_IMAGE_H_
