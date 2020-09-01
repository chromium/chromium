// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/test_utils.h"
#include "skia/public/mojom/bitmap.mojom.h"
#include "skia/public/mojom/blur_image_filter_tile_mode.mojom.h"
#include "skia/public/mojom/blur_image_filter_tile_mode_mojom_traits.h"
#include "skia/public/mojom/image_info.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"
#include "ui/gfx/skia_util.h"

namespace skia {

namespace {

// mojo::test::SerializeAndDeserialize() doesn't work for a raw enum, so roll
// our own.
bool SerializeAndDeserialize(SkBlurImageFilter::TileMode* input,
                             SkBlurImageFilter::TileMode* output) {
  skia::mojom::BlurTileMode mode =
      mojo::EnumTraits<skia::mojom::BlurTileMode,
                       SkBlurImageFilter::TileMode>::ToMojom(*input);
  return mojo::EnumTraits<skia::mojom::BlurTileMode,
                          SkBlurImageFilter::TileMode>::FromMojom(mode, output);
}

}  // namespace

TEST(StructTraitsTest, ImageInfo) {
  SkImageInfo input = SkImageInfo::Make(
      34, 56, SkColorType::kGray_8_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB));
  SkImageInfo output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::ImageInfo>(
      &input, &output));
  EXPECT_EQ(input, output);

  SkImageInfo another_input_with_null_color_space =
      SkImageInfo::Make(54, 43, SkColorType::kRGBA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType, nullptr);
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::ImageInfo>(
      &another_input_with_null_color_space, &output));
  EXPECT_FALSE(output.colorSpace());
  EXPECT_EQ(another_input_with_null_color_space, output);
}

TEST(StructTraitsTest, ImageInfoCustomColorSpace) {
  skcms_TransferFunction transfer{0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f};
  skcms_Matrix3x3 gamut{
      .vals = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}, {0.7f, 0.8f, 0.9f}}};
  sk_sp<SkColorSpace> color_space = SkColorSpace::MakeRGB(transfer, gamut);
  SkImageInfo input =
      SkImageInfo::Make(12, 34, SkColorType::kRGBA_8888_SkColorType,
                        kUnpremul_SkAlphaType, color_space);
  SkImageInfo output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::ImageInfo>(
      &input, &output));
  EXPECT_TRUE(output.colorSpace());
  EXPECT_EQ(input, output);
}

TEST(StructTraitsTest, Bitmap) {
  SkBitmap input;
  input.allocPixels(SkImageInfo::MakeN32Premul(
      10, 5,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear,
                            SkNamedGamut::kRec2020)));
  input.eraseColor(SK_ColorYELLOW);
  input.erase(SK_ColorTRANSPARENT, SkIRect::MakeXYWH(0, 1, 2, 3));
  SkBitmap output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(
      &input, &output));
  EXPECT_EQ(input.info(), output.info());
  EXPECT_EQ(input.rowBytes(), output.rowBytes());
  EXPECT_TRUE(gfx::BitmapsAreEqual(input, output));
}

TEST(StructTraitsTest, BitmapTooWideToSerialize) {
  SkBitmap input;
  constexpr int kTooWide = 32 * 1024 + 1;
  input.allocPixels(
      SkImageInfo::MakeN32(kTooWide, 1, SkAlphaType::kUnpremul_SkAlphaType));
  input.eraseColor(SK_ColorYELLOW);
  SkBitmap output;
  ASSERT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(
      &input, &output));
}

TEST(StructTraitsTest, BitmapTooTallToSerialize) {
  SkBitmap input;
  constexpr int kTooTall = 32 * 1024 + 1;
  input.allocPixels(
      SkImageInfo::MakeN32(1, kTooTall, SkAlphaType::kUnpremul_SkAlphaType));
  input.eraseColor(SK_ColorYELLOW);
  SkBitmap output;
  ASSERT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(
      &input, &output));
}

TEST(StructTraitsTest, BitmapWithExtraRowBytes) {
  SkBitmap input;
  // Ensure traits work with bitmaps containing additional bytes between rows.
  SkImageInfo info =
      SkImageInfo::MakeN32(8, 5, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
  // Any extra bytes on each row must be a multiple of the row's pixel size to
  // keep every row's pixels aligned.
  size_t extra = info.bytesPerPixel();
  input.allocPixels(info, info.minRowBytes() + extra);
  input.eraseColor(SK_ColorRED);
  input.erase(SK_ColorTRANSPARENT, SkIRect::MakeXYWH(0, 1, 2, 3));
  SkBitmap output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(
      &input, &output));
  EXPECT_EQ(input.info(), output.info());
  EXPECT_EQ(input.rowBytes(), output.rowBytes());
  EXPECT_TRUE(gfx::BitmapsAreEqual(input, output));
}

TEST(StructTraitsTest, BlurImageFilterTileMode) {
  SkBlurImageFilter::TileMode input(SkBlurImageFilter::kClamp_TileMode);
  SkBlurImageFilter::TileMode output;
  ASSERT_TRUE(SerializeAndDeserialize(&input, &output));
  EXPECT_EQ(input, output);
  input = SkBlurImageFilter::kRepeat_TileMode;
  ASSERT_TRUE(SerializeAndDeserialize(&input, &output));
  EXPECT_EQ(input, output);
  input = SkBlurImageFilter::kClampToBlack_TileMode;
  ASSERT_TRUE(SerializeAndDeserialize(&input, &output));
  EXPECT_EQ(input, output);
}

}  // namespace skia
