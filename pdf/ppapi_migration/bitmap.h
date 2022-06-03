// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_BITMAP_H_
#define PDF_PPAPI_MIGRATION_BITMAP_H_

#include <memory>

#include "third_party/skia/include/core/SkSize.h"

class SkBitmap;

namespace pp {
class ImageData;
}  // namespace pp

namespace chrome_pdf {

// Creates an SkBitmap of a given `size`.
SkBitmap CreateN32PremulSkBitmap(const SkISize& size);

// Creates an SkBitmap from a pp::ImageData. The SkBitmap takes ownership of the
// pp::ImageData, and shares ownership of the underlying pixel memory. (Note
// that it's easy to make a shallow copy of a pp::ImageData.)
//
// In case of an error, returns an empty SkBitmap.
//
// TODO(kmoon): Skia is trying to get rid of SkBitmap in favor of immutable
// types like SkImage, so we should migrate once PDFium is ready for Skia.
SkBitmap SkBitmapFromPPImageData(std::unique_ptr<pp::ImageData> image_data);

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_BITMAP_H_
