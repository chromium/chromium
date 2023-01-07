// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace skia {

SkBitmap ReadPixels(SkCanvas* canvas) {
  SkBitmap bitmap;
  bitmap.allocPixels(canvas->imageInfo());
  if (!canvas->readPixels(bitmap, 0, 0))
    bitmap.reset();
  return bitmap;
}

bool GetWritablePixels(SkCanvas* canvas, SkPixmap* result) {
  if (!canvas || !result) {
    return false;
  }

  SkImageInfo info;
  size_t row_bytes;
  void* pixels = canvas->accessTopLayerPixels(&info, &row_bytes);
  if (!pixels) {
    result->reset();
    return false;
  }

  result->reset(info, pixels, row_bytes);
  return true;
}

#if !defined(WIN32)

std::unique_ptr<SkCanvas> CreatePlatformCanvasWithPixels(
    int width,
    int height,
    bool is_opaque,
    uint8_t* data,
    OnFailureType failureType) {

  SkBitmap bitmap;
  bitmap.setInfo(SkImageInfo::MakeN32(width, height,
      is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType));

  if (data) {
    bitmap.setPixels(data);
  } else {
      if (!bitmap.tryAllocPixels()) {
        CHECK(failureType != CRASH_ON_FAILURE);
        return nullptr;
      }

      // Follow the logic in SkCanvas::createDevice(), initialize the bitmap if
      // it is not opaque.
      if (!is_opaque)
        bitmap.eraseARGB(0, 0, 0, 0);
  }

  return std::make_unique<SkCanvas>(bitmap);
}

#endif

}  // namespace skia
