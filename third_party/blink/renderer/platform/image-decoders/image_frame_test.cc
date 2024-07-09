// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace blink {
namespace {

// Needed for ImageFrame::SetMemoryAllocator, but still does the default
// allocation.
class TestAllocator final : public SkBitmap::Allocator {
  bool allocPixelRef(SkBitmap* dst) override { return dst->tryAllocPixels(); }
};

class ImageFrameTest : public testing::Test {
 public:
  void SetUp() override {
    src_8888_a = 0x80;
    src_8888_r = 0x40;
    src_8888_g = 0x50;
    src_8888_b = 0x60;
    src_8888 = SkPackARGB32(src_8888_a, src_8888_r, src_8888_g, src_8888_b);
    dst_8888 = SkPackARGB32(0xA0, 0x60, 0x70, 0x80);

#if SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
    pixel_format_n32 = skcms_PixelFormat_BGRA_8888;
#else
    pixel_format_n32 = skcms_PixelFormat_RGBA_8888;
#endif

    skcms_Transform(&src_8888, pixel_format_n32, skcms_AlphaFormat_Unpremul,
                    nullptr, &src_f16, skcms_PixelFormat_RGBA_hhhh,
                    skcms_AlphaFormat_Unpremul, nullptr, 1);
    skcms_Transform(&dst_8888, pixel_format_n32, skcms_AlphaFormat_Unpremul,
                    nullptr, &dst_f16, skcms_PixelFormat_RGBA_hhhh,
                    skcms_AlphaFormat_Unpremul, nullptr, 1);
  }

 protected:
  const float color_compoenent_tolerance = 0.01;
  unsigned src_8888_a, src_8888_r, src_8888_g, src_8888_b;
  ImageFrame::PixelData src_8888, dst_8888;
  ImageFrame::PixelDataF16 src_f16, dst_f16;
  skcms_PixelFormat pixel_format_n32;

  void ConvertN32ToF32(float* dst, ImageFrame::PixelData src) {
    skcms_Transform(&src, pixel_format_n32, skcms_AlphaFormat_Unpremul, nullptr,
                    dst, skcms_PixelFormat_RGBA_ffff,
                    skcms_AlphaFormat_Unpremul, nullptr, 1);
  }

  void ConvertF16ToF32(float* dst, ImageFrame::PixelDataF16 src) {
    skcms_Transform(&src, skcms_PixelFormat_RGBA_hhhh,
                    skcms_AlphaFormat_Unpremul, nullptr, dst,
                    skcms_PixelFormat_RGBA_ffff, skcms_AlphaFormat_Unpremul,
                    nullptr, 1);
  }
};

TEST_F(ImageFrameTest, BlendRGBARawF16Buffer) {
  ImageFrame::PixelData blended_8888(dst_8888);
  ImageFrame::BlendRGBARaw(&blended_8888, src_8888_r, src_8888_g, src_8888_b,
                           src_8888_a);

  ImageFrame::PixelDataF16 blended_f16 = dst_f16;
  ImageFrame::BlendRGBARawF16Buffer(&blended_f16, &src_f16, 1);

  float f32_from_blended_8888[4];
  ConvertN32ToF32(f32_from_blended_8888, blended_8888);

  float f32_from_blended_f16[4];
  ConvertF16ToF32(f32_from_blended_f16, blended_f16);

  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(fabs(f32_from_blended_8888[i] - f32_from_blended_f16[i]) <
                color_compoenent_tolerance);
  }
}

TEST_F(ImageFrameTest, BlendRGBAPremultipliedF16Buffer) {
  ImageFrame::PixelData blended_8888(dst_8888);
  ImageFrame::BlendRGBAPremultiplied(&blended_8888, src_8888_r, src_8888_g,
                                     src_8888_b, src_8888_a);

  ImageFrame::PixelDataF16 blended_f16 = dst_f16;
  ImageFrame::BlendRGBAPremultipliedF16Buffer(&blended_f16, &src_f16, 1);

  float f32_from_blended_8888[4];
  ConvertN32ToF32(f32_from_blended_8888, blended_8888);

  float f32_from_blended_f16[4];
  ConvertF16ToF32(f32_from_blended_f16, blended_f16);

  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(fabs(f32_from_blended_8888[i] - f32_from_blended_f16[i]) <
                color_compoenent_tolerance);
  }
}

}  // namespace
}  // namespace blink
