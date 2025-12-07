// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"

// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers

template <typename T>
static bool read(const uint8_t** data, size_t* size, T* value) {
  if (*size < sizeof(T)) {
    return false;
  }

  *value = *reinterpret_cast<const T*>(*data);
  *data += sizeof(T);
  *size -= sizeof(T);
  return true;
}

#define READ_INT(output)                 \
  if (!read<int>(&data, &size, &output)) \
    return 0;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  int iMethod, srcW, srcH, dstW, dstH, subsetX, subsetY, subsetR, subsetB;
  READ_INT(iMethod)
  READ_INT(srcW)
  READ_INT(srcH)
  READ_INT(dstW)
  READ_INT(dstH)
  READ_INT(subsetX)
  READ_INT(subsetY)
  READ_INT(subsetR)
  READ_INT(subsetB)

  skia::ImageOperations::ResizeMethod method =
      skia::ImageOperations::RESIZE_GOOD;
  switch (iMethod) {
    case 1:
      method = skia::ImageOperations::RESIZE_BETTER;
      break;
    case 2:
      method = skia::ImageOperations::RESIZE_BEST;
      break;
    case 3:
      method = skia::ImageOperations::RESIZE_BOX;
      break;
    case 4:
      method = skia::ImageOperations::RESIZE_HAMMING1;
      break;
    case 5:
      method = skia::ImageOperations::RESIZE_LANCZOS3;
      break;
    default:
      break;  // RESIZE_GOOD
  }
  if (srcW < 0 || srcH < 0 || srcW > 300 || srcH > 300) {
    return 0;
  }
  if (dstW <= 0 || dstH <= 0 || dstW > srcW || dstH > srcH) {
    return 0;
  }
  if (dstW == 0 && dstH == 0) {
    return 0;
  }
  SkIRect subset = SkIRect{subsetX, subsetY, subsetR, subsetB};
  if (subset.isEmpty()) {
    return 0;
  }
  SkIRect dest = {0, 0, dstW, dstH};
  if (!dest.contains(subset)) {
    return 0;
  }
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32(srcW, srcH, kOpaque_SkAlphaType));
  if (!surface) {
    return 0;
  }
  SkPixmap input;
  if (!surface->peekPixels(&input)) {
    return 0;
  }

  SkBitmap bitmap =
      skia::ImageOperations::Resize(input, method, dstW, dstH, subset);
  return 0;
}
