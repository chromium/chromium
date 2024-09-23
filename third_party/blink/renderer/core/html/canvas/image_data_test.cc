// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/image_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

class ImageDataTest : public testing::Test {};

// This test passes if it does not crash. If the required memory is not
// allocated to the ImageData, then an exception must raise.
TEST_F(ImageDataTest, CreateImageDataTooBig) {
  DummyExceptionStateForTesting exception_state;
  ImageData* too_big_image_data = ImageData::Create(
      32767, 32767, ImageDataSettings::Create(), exception_state);
  if (!too_big_image_data) {
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<ESErrorType>(), ESErrorType::kRangeError);
  }
}

TEST_F(ImageDataTest, ImageDataTooBigToAllocateDoesNotCrash) {
  constexpr size_t kBytesPerPixel = 4;
  constexpr size_t kMaxSize = v8::TypedArray::kMaxByteLength / kBytesPerPixel;

  // Statically compute a width and height such that the product is above
  // kMaxSize.
  constexpr int kWidth = 1 << 30;
  constexpr int kHeight = (kMaxSize / kWidth) + 1;
  static_assert(size_t{kWidth} * (kHeight - 1) <= kMaxSize);
  static_assert(size_t{kWidth} * kHeight > kMaxSize);

  gfx::Size too_big_size(kWidth, kHeight);
  ImageData* image_data = ImageData::CreateForTest(too_big_size);
  EXPECT_EQ(image_data, nullptr);
}

}  // namespace
}  // namespace blink
