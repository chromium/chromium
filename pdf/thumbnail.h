// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_THUMBNAIL_H_
#define PDF_THUMBNAIL_H_

#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace chrome_pdf {

class Thumbnail final {
 public:
  Thumbnail();
  Thumbnail(const gfx::Size& page_size, float device_pixel_ratio);
  Thumbnail(Thumbnail&& other);
  Thumbnail& operator=(Thumbnail&& other);
  ~Thumbnail();

  SkBitmap& bitmap() { return bitmap_; }

  float device_pixel_ratio() const { return device_pixel_ratio_; }

 private:
  // Raw image data of the thumbnail.
  SkBitmap bitmap_;

  // Intended resolution of the thumbnail image. The dimensions of `bitmap_`
  // are the dimensions of the thumbnail in CSS pixels multiplied by
  // `device_pixel_ratio_`.
  // Only values between 0.25 and 2 are supported.
  float device_pixel_ratio_ = 1;
};

}  // namespace chrome_pdf

#endif  // PDF_THUMBNAIL_H_
