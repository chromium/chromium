// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "v8/include/v8.h"

namespace blink {

class StaticBitmapImageTest : public testing::Test {};

// This test verifies if requesting a large ImageData that cannot be handled by
// V8 is denied by StaticBitmapImage. This prevents V8 from crashing the
// renderer if the user asks to get back the ImageData.
TEST_F(StaticBitmapImageTest,
       ConvertArrayBufferContentsTooBigToAllocateDoesNotCrash) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(1, 1);
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
  EXPECT_TRUE(!!surface);

  scoped_refptr<StaticBitmapImage> image =
      StaticBitmapImage::Create(surface->makeImageSnapshot());

  IntRect too_big_rect(IntPoint(0, 0),
                       IntSize(1, (v8::TypedArray::kMaxLength / 4) + 1));
  EXPECT_GT(
      StaticBitmapImage::GetSizeInBytes(too_big_rect, CanvasColorParams()),
      v8::TypedArray::kMaxLength);
}

}  // namespace blink
