// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/bitmap.h"

#include <stdint.h>

#include <memory>

#include "base/check_op.h"
#include "ppapi/cpp/image_data.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSize.h"

namespace chrome_pdf {

namespace {

// Releases pp::ImageData associated with the SkPixelRef. The pp::ImageData acts
// like a shared pointer to memory provided by Pepper, and must be retained for
// the life of the SkPixelRef.
void ReleaseImageData(void* addr, void* context) {
  pp::ImageData* image_data = static_cast<pp::ImageData*>(context);
  DCHECK_EQ(addr, image_data->data());
  delete image_data;
}

}  // namespace

SkBitmap CreateN32PremulSkBitmap(const SkISize& size) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size));
  return bitmap;
}

SkBitmap SkBitmapFromPPImageData(std::unique_ptr<pp::ImageData> image_data) {
  if (image_data->is_null()) {
    return SkBitmap();
  }

  // Note that we unconditionally use BGRA_PREMUL with PDFium.
  DCHECK_EQ(image_data->format(), PP_IMAGEDATAFORMAT_BGRA_PREMUL);
  SkImageInfo info =
      SkImageInfo::Make(image_data->size().width(), image_data->size().height(),
                        kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  void* data = image_data->data();
  int32_t stride = image_data->stride();

  SkBitmap bitmap;
  bool success = bitmap.installPixels(info, data, stride, ReleaseImageData,
                                      image_data.release());
  DCHECK(success);
  return bitmap;
}

}  // namespace chrome_pdf
