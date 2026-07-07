// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "skia/ext/image_operations.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace {

void ResizeFuzzTest(skia::ImageOperations::ResizeMethod method,
                    int src_w,
                    int src_h,
                    int dst_w,
                    int dst_h,
                    int subset_x,
                    int subset_y,
                    int subset_r,
                    int subset_b) {
  // Relational guards. Simple bounds are enforced by FuzzTest domains.
  if (dst_w > src_w || dst_h > src_h) {
    return;
  }
  SkIRect subset = SkIRect{subset_x, subset_y, subset_r, subset_b};
  if (subset.isEmpty()) {
    return;
  }
  SkIRect dest = {0, 0, dst_w, dst_h};
  if (!dest.contains(subset)) {
    return;
  }

  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32(src_w, src_h, kOpaque_SkAlphaType));
  if (!surface) {
    return;
  }
  SkPixmap input;
  if (!surface->peekPixels(&input)) {
    return;
  }

  SkBitmap bitmap =
      skia::ImageOperations::Resize(input, method, dst_w, dst_h, subset);
}

}  // namespace

FUZZ_TEST(ImageOperationsResize, ResizeFuzzTest)
    .WithDomains(
        /*method=*/fuzztest::ElementOf<skia::ImageOperations::ResizeMethod>({
            skia::ImageOperations::RESIZE_GOOD,
            skia::ImageOperations::RESIZE_BETTER,
            skia::ImageOperations::RESIZE_BEST,
            skia::ImageOperations::RESIZE_BOX,
            skia::ImageOperations::RESIZE_HAMMING1,
            skia::ImageOperations::RESIZE_LANCZOS3,
        }),
        /*src_w=*/fuzztest::InRange(0, 300),
        /*src_h=*/fuzztest::InRange(0, 300),
        /*dst_w=*/fuzztest::InRange(1, 300),
        /*dst_h=*/fuzztest::InRange(1, 300),
        /*subset_x=*/fuzztest::InRange(0, 300),
        /*subset_y=*/fuzztest::InRange(0, 300),
        /*subset_r=*/fuzztest::InRange(0, 300),
        /*subset_b=*/fuzztest::InRange(0, 300));
