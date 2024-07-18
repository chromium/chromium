// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_UI_THUMBNAIL_H_
#define PDF_UI_THUMBNAIL_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

class Thumbnail;

using SendThumbnailCallback = base::OnceCallback<void(Thumbnail)>;

class Thumbnail final {
 public:
  Thumbnail(const gfx::Size& page_size, float device_pixel_ratio);
  Thumbnail(Thumbnail&& other) noexcept;
  Thumbnail& operator=(Thumbnail&& other) noexcept;
  ~Thumbnail();

  float device_pixel_ratio() const { return device_pixel_ratio_; }

  int stride() const { return stride_; }

  const gfx::Size& image_size() const { return image_size_; }

  // Note that <canvas> can only hold data in RGBA format. It is the
  // responsibility of the thumbnail's renderer to fill the data with RGBA data.
  base::Value::BlobStorage& GetImageData();

  // Transfers the internal image data to the caller. After calling TakeData(),
  // this Thumbnail instance should not be used.
  base::Value::BlobStorage TakeData();

 private:
  // Intended resolution of the thumbnail image. The dimensions of `bitmap_`
  // are the dimensions of the thumbnail in CSS pixels multiplied by
  // `device_pixel_ratio_`.
  // Only values between 0.25 and 2 are supported.
  float device_pixel_ratio_;

  gfx::Size image_size_;  // In pixels.
  int stride_;
  base::Value::BlobStorage image_data_;
};

}  // namespace chrome_pdf

#endif  // PDF_UI_THUMBNAIL_H_
