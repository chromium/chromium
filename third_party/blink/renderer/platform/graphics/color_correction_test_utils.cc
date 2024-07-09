// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"

#include "base/containers/heap_array.h"
#include "base/notreached.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace blink {

bool ColorCorrectionTestUtils::IsNearlyTheSame(float expected,
                                               float actual,
                                               float tolerance) {
  EXPECT_LE(actual, expected + tolerance);
  EXPECT_GE(actual, expected - tolerance);
  return true;
}

sk_sp<SkColorSpace> ColorCorrectionTestUtils::ColorSpinSkColorSpace() {
  const unsigned char colorspin_profile_data[] = {
      0x00, 0x00, 0x01, 0xea, 0x54, 0x45, 0x53, 0x54, 0x00, 0x00, 0x00, 0x00,
      0x6d, 0x6e, 0x74, 0x72, 0x52, 0x47, 0x42, 0x20, 0x58, 0x59, 0x5a, 0x20,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x61, 0x63, 0x73, 0x70, 0x74, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00,
      0x74, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6, 0xd6,
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xd3, 0x2d, 0x74, 0x65, 0x73, 0x74,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09,
      0x63, 0x70, 0x72, 0x74, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x0d,
      0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x8c,
      0x77, 0x74, 0x70, 0x74, 0x00, 0x00, 0x01, 0x8c, 0x00, 0x00, 0x00, 0x14,
      0x72, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0xa0, 0x00, 0x00, 0x00, 0x14,
      0x67, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0xb4, 0x00, 0x00, 0x00, 0x14,
      0x62, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0xc8, 0x00, 0x00, 0x00, 0x14,
      0x72, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0xdc, 0x00, 0x00, 0x00, 0x0e,
      0x67, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0xdc, 0x00, 0x00, 0x00, 0x0e,
      0x62, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0xdc, 0x00, 0x00, 0x00, 0x0e,
      0x74, 0x65, 0x78, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x10, 0x77, 0x68, 0x61, 0x63, 0x6b, 0x65, 0x64, 0x2e,
      0x69, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x52,
      0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0xcc, 0x58, 0x59, 0x5a, 0x20,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x8d, 0x00, 0x00, 0xa0, 0x2c,
      0x00, 0x00, 0x0f, 0x95, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x26, 0x31, 0x00, 0x00, 0x10, 0x2f, 0x00, 0x00, 0xbe, 0x9b,
      0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x18,
      0x00, 0x00, 0x4f, 0xa5, 0x00, 0x00, 0x04, 0xfc, 0x63, 0x75, 0x72, 0x76,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x33};
  skcms_ICCProfile colorspin_profile;
  skcms_Parse(colorspin_profile_data, sizeof(colorspin_profile_data),
              &colorspin_profile);
  return SkColorSpace::Make(colorspin_profile);
}

void ColorCorrectionTestUtils::CompareColorCorrectedPixels(
    const void* actual_pixels,
    const void* expected_pixels,
    size_t num_pixels,
    PixelFormat pixel_format,
    PixelsAlphaMultiply alpha_multiplied,
    UnpremulRoundTripTolerance premul_unpremul_tolerance) {
  bool test_passed = true;
  int _8888_color_correction_tolerance = 3;
  int _16161616_color_correction_tolerance = 255;
  float floating_point_color_correction_tolerance = 0.01;
  if (premul_unpremul_tolerance == kNoUnpremulRoundTripTolerance)
    floating_point_color_correction_tolerance = 0;

  switch (pixel_format) {
    case kPixelFormat_8888: {
      if (premul_unpremul_tolerance == kUnpremulRoundTripTolerance) {
        // Premul->unpremul->premul round trip does not introduce any error when
        // rounding intermediate results. However, we still might see some error
        // introduced in consecutive color correction operations (error <= 3).
        // For unpremul->premul->unpremul round trip, we do premul and compare
        // the result.
        const uint8_t* actual_pixels_u8 =
            static_cast<const uint8_t*>(actual_pixels);
        const uint8_t* expected_pixels_u8 =
            static_cast<const uint8_t*>(expected_pixels);
        for (size_t i = 0; test_passed && i < num_pixels; i++) {
          test_passed &=
              (actual_pixels_u8[i * 4 + 3] == expected_pixels_u8[i * 4 + 3]);
          int alpha_multiplier =
              alpha_multiplied ? 1 : expected_pixels_u8[i * 4 + 3];
          for (size_t j = 0; j < 3; j++) {
            test_passed &= IsNearlyTheSame(
                actual_pixels_u8[i * 4 + j] * alpha_multiplier,
                expected_pixels_u8[i * 4 + j] * alpha_multiplier,
                _8888_color_correction_tolerance);
          }
        }
      } else {
        EXPECT_EQ(std::memcmp(actual_pixels, expected_pixels, num_pixels * 4),
                  0);
      }
      break;
    }

    case kPixelFormat_16161616: {
      const uint16_t* actual_pixels_u16 =
          static_cast<const uint16_t*>(actual_pixels);
      const uint16_t* expected_pixels_u16 =
          static_cast<const uint16_t*>(expected_pixels);
      for (size_t i = 0; test_passed && i < num_pixels * 4; i++) {
        test_passed &=
            IsNearlyTheSame(actual_pixels_u16[i], expected_pixels_u16[i],
                            _16161616_color_correction_tolerance);
      }
      break;
    }

    case kPixelFormat_hhhh: {
      auto actual_pixels_f32 = base::HeapArray<float>::Uninit(num_pixels * 4);
      auto expected_pixels_f32 = base::HeapArray<float>::Uninit(num_pixels * 4);
      EXPECT_TRUE(
          skcms_Transform(actual_pixels, skcms_PixelFormat_RGBA_hhhh,
                          skcms_AlphaFormat_Unpremul, nullptr,
                          actual_pixels_f32.data(), skcms_PixelFormat_BGRA_ffff,
                          skcms_AlphaFormat_Unpremul, nullptr, num_pixels));
      EXPECT_TRUE(
          skcms_Transform(expected_pixels, skcms_PixelFormat_RGBA_hhhh,
                          skcms_AlphaFormat_Unpremul, nullptr,
                          expected_pixels_f32.data(), skcms_PixelFormat_BGRA_ffff,
                          skcms_AlphaFormat_Unpremul, nullptr, num_pixels));

      for (size_t i = 0; test_passed && i < num_pixels * 4; i++) {
        test_passed &=
            IsNearlyTheSame(actual_pixels_f32[i], expected_pixels_f32[i],
                            floating_point_color_correction_tolerance);
      }
      break;
    }

    case kPixelFormat_ffff: {
      const float* actual_pixels_f32 = static_cast<const float*>(actual_pixels);
      const float* expected_pixels_f32 =
          static_cast<const float*>(expected_pixels);
      for (size_t i = 0; test_passed && i < num_pixels * 4; i++) {
        test_passed &=
            IsNearlyTheSame(actual_pixels_f32[i], expected_pixels_f32[i],
                            floating_point_color_correction_tolerance);
      }
      break;
    }

    default:
      NOTREACHED_IN_MIGRATION();
  }
  EXPECT_EQ(test_passed, true);
}

bool ColorCorrectionTestUtils::ConvertPixelsToColorSpaceAndPixelFormatForTest(
    void* src_data,
    size_t num_elements,
    PredefinedColorSpace src_color_space,
    ImageDataStorageFormat src_storage_format,
    PredefinedColorSpace dst_color_space,
    CanvasPixelFormat dst_canvas_pixel_format,
    std::unique_ptr<uint8_t[]>& converted_pixels,
    PixelFormat pixel_format_for_f16_canvas) {
  skcms_PixelFormat src_pixel_format = skcms_PixelFormat_RGBA_8888;
  if (src_storage_format == ImageDataStorageFormat::kUint16) {
    src_pixel_format = skcms_PixelFormat_RGBA_16161616LE;
  } else if (src_storage_format == ImageDataStorageFormat::kFloat32) {
    src_pixel_format = skcms_PixelFormat_RGBA_ffff;
  }

  skcms_PixelFormat dst_pixel_format = skcms_PixelFormat_RGBA_8888;
  if (dst_canvas_pixel_format == CanvasPixelFormat::kF16) {
    dst_pixel_format = (pixel_format_for_f16_canvas == kPixelFormat_hhhh)
                           ? skcms_PixelFormat_RGBA_hhhh
                           : skcms_PixelFormat_RGBA_ffff;
  }

  sk_sp<SkColorSpace> src_sk_color_space = nullptr;
  src_sk_color_space =
      CanvasColorParams(src_color_space,
                        (src_storage_format == ImageDataStorageFormat::kUint8)
                            ? CanvasPixelFormat::kUint8
                            : CanvasPixelFormat::kF16,
                        kNonOpaque)
          .GetSkColorSpace();
  if (!src_sk_color_space.get())
    src_sk_color_space = SkColorSpace::MakeSRGB();

  sk_sp<SkColorSpace> dst_sk_color_space =
      CanvasColorParams(dst_color_space, dst_canvas_pixel_format, kNonOpaque)
          .GetSkColorSpace();
  if (!dst_sk_color_space.get())
    dst_sk_color_space = SkColorSpace::MakeSRGB();

  skcms_ICCProfile* src_profile_ptr = nullptr;
  skcms_ICCProfile* dst_profile_ptr = nullptr;
  skcms_ICCProfile src_profile, dst_profile;
  src_sk_color_space->toProfile(&src_profile);
  dst_sk_color_space->toProfile(&dst_profile);
  // If the profiles are similar, we better leave them as nullptr, since
  // skcms_Transform() only checks for profile pointer equality for the fast
  // path.
  if (!skcms_ApproximatelyEqualProfiles(&src_profile, &dst_profile)) {
    src_profile_ptr = &src_profile;
    dst_profile_ptr = &dst_profile;
  }

  skcms_AlphaFormat alpha_format = skcms_AlphaFormat_Unpremul;
  bool conversion_result =
      skcms_Transform(src_data, src_pixel_format, alpha_format, src_profile_ptr,
                      converted_pixels.get(), dst_pixel_format, alpha_format,
                      dst_profile_ptr, num_elements / 4);

  return conversion_result;
}

bool ColorCorrectionTestUtils::MatchColorSpace(
    sk_sp<SkColorSpace> src_color_space,
    sk_sp<SkColorSpace> dst_color_space) {
  if ((!src_color_space && dst_color_space) ||
      (src_color_space && !dst_color_space))
    return false;
  if (!src_color_space && !dst_color_space)
    return true;
  skcms_ICCProfile src_profile, dst_profile;
  src_color_space->toProfile(&src_profile);
  dst_color_space->toProfile(&dst_profile);
  return skcms_ApproximatelyEqualProfiles(&src_profile, &dst_profile);
}

bool ColorCorrectionTestUtils::MatchSkImages(sk_sp<SkImage> src_image,
                                             sk_sp<SkImage> dst_image,
                                             unsigned uint8_tolerance,
                                             float f16_tolerance,
                                             bool compare_alpha) {
  if ((!src_image && dst_image) || (src_image && !dst_image))
    return false;
  if (!src_image)
    return true;
  if ((src_image->width() != dst_image->width()) ||
      (src_image->height() != dst_image->height())) {
    return false;
  }

  if (compare_alpha && src_image->alphaType() != dst_image->alphaType())
    return false;
  // Color type is not checked since the decoded image does not have a specific
  // color type, unless it is drawn onto a surface or readPixels() is called.
  // Only compare color spaces if both are non-null
  if (src_image->refColorSpace() && dst_image->refColorSpace()) {
    if (!MatchColorSpace(src_image->refColorSpace(),
                         dst_image->refColorSpace())) {
      return false;
    }
  }

  bool test_passed = true;
  int num_pixels = src_image->width() * src_image->height();
  int num_components = compare_alpha ? 4 : 3;

  SkImageInfo src_info = SkImageInfo::Make(
      src_image->width(), src_image->height(), kN32_SkColorType,
      src_image->alphaType(), src_image->refColorSpace());

  SkImageInfo dst_info = SkImageInfo::Make(
      dst_image->width(), dst_image->height(), kN32_SkColorType,
      src_image->alphaType(), dst_image->refColorSpace());

  if (src_image->colorType() != kRGBA_F16_SkColorType) {
    auto src_pixels = base::HeapArray<uint8_t>::Uninit(num_pixels * 4);
    auto dst_pixels = base::HeapArray<uint8_t>::Uninit(num_pixels * 4);

    src_image->readPixels(src_info, src_pixels.data(), src_info.minRowBytes(),
                          0, 0);
    dst_image->readPixels(dst_info, dst_pixels.data(), dst_info.minRowBytes(),
                          0, 0);

    for (size_t i = 0; test_passed && i < src_pixels.size(); i++) {
      for (int j = 0; j < num_components; j++) {
        test_passed &= IsNearlyTheSame(src_pixels[i * 4 + j],
                                       dst_pixels[i * 4 + j], uint8_tolerance);
      }
    }
    return test_passed;
  }

  auto src_pixels = base::HeapArray<float>::Uninit(num_pixels * 4);
  auto dst_pixels = base::HeapArray<float>::Uninit(num_pixels * 4);

  src_info = src_info.makeColorType(kRGBA_F32_SkColorType);
  dst_info = dst_info.makeColorType(kRGBA_F32_SkColorType);

  src_image->readPixels(src_info, src_pixels.data(), src_info.minRowBytes(), 0,
                        0);
  dst_image->readPixels(dst_info, dst_pixels.data(), dst_info.minRowBytes(), 0,
                        0);

  for (size_t i = 0; test_passed && i < src_pixels.size(); i++) {
    for (int j = 0; j < num_components; j++) {
      test_passed &= IsNearlyTheSame(src_pixels[i * 4 + j],
                                     dst_pixels[i * 4 + j], f16_tolerance);
    }
  }
  return test_passed;
}

}  // namespace blink
