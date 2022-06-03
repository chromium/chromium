// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_IMAGE_H_
#define PDF_PPAPI_MIGRATION_IMAGE_H_

#include "ppapi/cpp/image_data.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace chrome_pdf {

// Holder for an image in either Pepper or Skia format.
//
// Note that both Pepper and Skia images retain shared ownership of any
// underlying pixel memory, so this class may be copied freely.
class Image final {
 public:
  explicit Image(const pp::ImageData& pepper_image);
  explicit Image(const SkBitmap& skia_image);
  Image(const Image& other);
  Image& operator=(const Image& other);
  ~Image();

  const pp::ImageData& pepper_image() const {
    return absl::get<pp::ImageData>(image_);
  }

  const SkBitmap& skia_image() const { return absl::get<SkBitmap>(image_); }

 private:
  absl::variant<pp::ImageData, SkBitmap> image_;
};

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_IMAGE_H_
