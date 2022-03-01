// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_IMAGE_H_
#define PDF_PPAPI_MIGRATION_IMAGE_H_

#include "third_party/skia/include/core/SkBitmap.h"

namespace chrome_pdf {

// Holder for an image in either Pepper or Skia format.
//
// Note that both Pepper and Skia images retain shared ownership of any
// underlying pixel memory, so this class may be copied freely.
class Image final {
 public:
  explicit Image(const SkBitmap& skia_image);
  Image(const Image& other);
  Image& operator=(const Image& other);
  ~Image();

  const SkBitmap& skia_image() const { return image_; }

 private:
  SkBitmap image_;
};

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_IMAGE_H_
