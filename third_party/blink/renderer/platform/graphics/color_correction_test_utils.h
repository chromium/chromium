// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_CORRECTION_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_CORRECTION_TEST_UTILS_H_

#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

enum PixelFormat {
  kPixelFormat_8888,      // 8 bit color components
  kPixelFormat_16161616,  // 16 bit unsigned color components
  kPixelFormat_hhhh,      // half float color components
  kPixelFormat_ffff,      // float 32 color components
};

enum PixelsAlphaMultiply {
  kAlphaMultiplied,
  kAlphaUnmultiplied,
};

enum UnpremulRoundTripTolerance {
  kNoUnpremulRoundTripTolerance,
  kUnpremulRoundTripTolerance,
};

class ColorCorrectionTestUtils {
  STATIC_ONLY(ColorCorrectionTestUtils);

 public:
  // ImageBitmap color space conversion test utils
  static sk_sp<SkColorSpace> ColorSpinSkColorSpace();

  static void CompareColorCorrectedPixels(
      const void* actual_pixels,
      const void* expected_pixels,
      size_t num_pixels,
      PixelFormat pixel_format,
      PixelsAlphaMultiply alpha_multiplied = kAlphaUnmultiplied,
      UnpremulRoundTripTolerance premul_unpremul_tolerance =
          kUnpremulRoundTripTolerance);

  static bool ConvertPixelsToColorSpaceAndPixelFormatForTest(
      void* src_data,
      size_t num_elements,
      PredefinedColorSpace src_color_space,
      ImageDataStorageFormat src_storage_format,
      PredefinedColorSpace dst_color_space,
      CanvasPixelFormat dst_canvas_pixel_format,
      std::unique_ptr<uint8_t[]>& converted_pixels,
      PixelFormat pixel_format_for_f16_canvas);

  static bool MatchColorSpace(sk_sp<SkColorSpace> src_color_space,
                              sk_sp<SkColorSpace> dst_color_space);

  // Compares size, colorspace and pixel values of two images
  // If the colorspace of either image is null the colorspaces are assumed
  // to be equal
  static bool MatchSkImages(sk_sp<SkImage> src_image,
                            sk_sp<SkImage> dst_image,
                            unsigned uint8_tolerance,
                            float f16_tolerance,
                            bool compare_alpha);

 private:
  static bool IsNearlyTheSame(float expected, float actual, float tolerance);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_CORRECTION_TEST_UTILS_H_
