// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/image.h"

#include "ppapi/cpp/image_data.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace chrome_pdf {

Image::Image(const pp::ImageData& pepper_image) : image_(pepper_image) {}

Image::Image(const SkBitmap& skia_image) : image_(skia_image) {}

Image::Image(const Image& other) = default;

Image& Image::operator=(const Image& other) = default;

Image::~Image() = default;

}  // namespace chrome_pdf
