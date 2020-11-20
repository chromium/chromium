// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/test_utils.h"
#include "skia/public/mojom/bitmap.mojom.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
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

// A helper to construct a skia.mojom.Bitmap without using StructTraits
// to bypass checks on the sending/serialization side.
mojo::StructPtr<skia::mojom::Bitmap> ConstructBitmap(
    SkImageInfo info,
    int row_bytes,
    std::vector<unsigned char> pixels) {
  auto mojom_bitmap = skia::mojom::Bitmap::New();
  mojom_bitmap->image_info = std::move(info);
  mojom_bitmap->UNUSED_row_bytes = row_bytes;
  mojom_bitmap->pixel_data = std::move(pixels);
  return mojom_bitmap;
}

// A helper to construct a skia.mojom.InlineBitmap without using StructTraits
// to bypass checks on the sending/serialization side.
mojo::StructPtr<skia::mojom::InlineBitmap> ConstructInlineBitmap(
    SkImageInfo info,
    std::vector<unsigned char> pixels) {
  DCHECK_EQ(info.colorType(), kN32_SkColorType);
  auto mojom_bitmap = skia::mojom::InlineBitmap::New();
  mojom_bitmap->image_info = std::move(info);
  mojom_bitmap->pixel_data = std::move(pixels);
  return mojom_bitmap;
}

TEST(StructTraitsTest, ImageInfo) {
  SkImageInfo input = SkImageInfo::Make(
      34, 56, SkColorType::kGray_8_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB));
  SkImageInfo output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::ImageInfo>(
      input, output));
  EXPECT_EQ(input, output);

  SkImageInfo another_input_with_null_color_space =
      SkImageInfo::Make(54, 43, SkColorType::kRGBA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType, nullptr);
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::ImageInfo>(
      another_input_with_null_color_space, output));
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
      input, output));
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
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(input, output));
  EXPECT_EQ(input.info(), output.info());
  EXPECT_EQ(input.rowBytes(), output.rowBytes());
  EXPECT_TRUE(gfx::BitmapsAreEqual(input, output));
}

TEST(StructTraitsTest, BitmapNull) {
  SkBitmap input;
  input.setInfo(SkImageInfo::MakeN32Premul(
      10, 5,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear,
                            SkNamedGamut::kRec2020)));
  EXPECT_TRUE(input.isNull());

  // Null input produces a default-initialized SkBitmap.
  SkBitmap output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(input, output));
  EXPECT_EQ(output.info().alphaType(), kUnknown_SkAlphaType);
  EXPECT_EQ(output.info().colorType(), kUnknown_SkColorType);
  EXPECT_EQ(output.rowBytes(), 0u);
  EXPECT_TRUE(output.isNull());
}

TEST(StructTraitsTest, BitmapTooWideToSerialize) {
  SkBitmap input;
  constexpr int kTooWide = 64 * 1024 + 1;
  input.allocPixels(
      SkImageInfo::MakeN32(kTooWide, 1, SkAlphaType::kUnpremul_SkAlphaType));
  input.eraseColor(SK_ColorYELLOW);
  SkBitmap output;
  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(input, output));
}

TEST(StructTraitsTest, BitmapTooTallToSerialize) {
  SkBitmap input;
  constexpr int kTooTall = 64 * 1024 + 1;
  input.allocPixels(
      SkImageInfo::MakeN32(1, kTooTall, SkAlphaType::kUnpremul_SkAlphaType));
  input.eraseColor(SK_ColorYELLOW);
  SkBitmap output;
  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(input, output));
}

TEST(StructTraitsTest, BitmapSerializeInvalidRowBytes) {
  SkBitmap input;
  SkImageInfo info =
      SkImageInfo::MakeN32(8, 5, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
  EXPECT_TRUE(
      input.tryAllocPixels(info, info.minRowBytes() + info.bytesPerPixel()));

  // We do not allow sending rowBytes() other than the minRowBytes().
  EXPECT_DEATH(skia::mojom::Bitmap::SerializeAsMessage(&input), "");
}

TEST(StructTraitsTest, VerifyBitmapConstruction) {
  // Verify that we can manually construct a valid skia.mojom.Bitmap and
  // deserialize it successfully.
  mojo::StructPtr<skia::mojom::Bitmap> input =
      ConstructBitmap(SkImageInfo::MakeN32Premul(1, 1), 0, {1, 2, 3, 4});

  SkBitmap output;
  bool ok =
      mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(input, output);
  EXPECT_TRUE(ok);
}

TEST(StructTraitsTest, BitmapDeserializeIgnoresRowBytes) {
  mojo::StructPtr<skia::mojom::Bitmap> input =
      ConstructBitmap(SkImageInfo::MakeN32Premul(1, 1), 8, {1, 2, 3, 4});

  SkBitmap output;
  bool ok =
      mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(input, output);
  EXPECT_TRUE(ok);
  // The row_bytes field is ignored, and the minRowBytes() is always used.
  EXPECT_EQ(4u, output.rowBytes());
}

TEST(StructTraitsTest, BitmapDeserializeMismatchFormatAndPixels) {
  mojo::StructPtr<skia::mojom::Bitmap> input =
      ConstructBitmap(SkImageInfo::MakeA8(1, 1), 0, {1, 2, 3, 4});

  SkBitmap output;
  bool ok =
      mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(input, output);
  EXPECT_FALSE(ok);
}

TEST(StructTraitsTest, BitmapDeserializeTooFewPixels) {
  mojo::StructPtr<skia::mojom::Bitmap> input =
      ConstructBitmap(SkImageInfo::MakeN32Premul(2, 1), 0, {1, 2, 3, 4});

  SkBitmap output;
  bool ok =
      mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(input, output);
  EXPECT_FALSE(ok);
}

TEST(StructTraitsTest, BitmapDeserializeTooManyPixels) {
  mojo::StructPtr<skia::mojom::Bitmap> input = ConstructBitmap(
      SkImageInfo::MakeN32Premul(1, 1), 0, {1, 2, 3, 4, 5, 6, 7, 8});

  SkBitmap output;
  bool ok =
      mojo::test::SerializeAndDeserialize<skia::mojom::Bitmap>(input, output);
  EXPECT_FALSE(ok);
}

TEST(StructTraitsTest, BlurImageFilterTileMode) {
  SkBlurImageFilter::TileMode input(SkBlurImageFilter::kClamp_TileMode);
  SkBlurImageFilter::TileMode output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::BlurTileMode>(
      input, output));
  EXPECT_EQ(input, output);
  input = SkBlurImageFilter::kRepeat_TileMode;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::BlurTileMode>(
      input, output));
  EXPECT_EQ(input, output);
  input = SkBlurImageFilter::kClampToBlack_TileMode;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::BlurTileMode>(
      input, output));
  EXPECT_EQ(input, output);
}

TEST(StructTraitsTest, InlineBitmap) {
  SkBitmap input;
  input.allocPixels(SkImageInfo::MakeN32Premul(
      10, 5,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear,
                            SkNamedGamut::kRec2020)));
  input.eraseColor(SK_ColorYELLOW);
  input.erase(SK_ColorTRANSPARENT, SkIRect::MakeXYWH(0, 1, 2, 3));
  SkBitmap output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
      input, output));
  EXPECT_EQ(input.info(), output.info());
  EXPECT_EQ(input.rowBytes(), output.rowBytes());
  EXPECT_TRUE(gfx::BitmapsAreEqual(input, output));
}

TEST(StructTraitsTest, InlineBitmapNull) {
  SkBitmap input;
  input.setInfo(SkImageInfo::MakeN32Premul(
      10, 5,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear,
                            SkNamedGamut::kRec2020)));
  EXPECT_TRUE(input.isNull());

  // Null input produces a default-initialized SkBitmap.
  SkBitmap output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
      input, output));
  EXPECT_EQ(output.info().alphaType(), kUnknown_SkAlphaType);
  EXPECT_EQ(output.info().colorType(), kUnknown_SkColorType);
  EXPECT_EQ(output.rowBytes(), 0u);
  EXPECT_TRUE(output.isNull());
}

TEST(StructTraitsTest, InlineBitmapSerializeToString) {
  SkBitmap input;
  input.allocPixels(SkImageInfo::MakeN32Premul(10, 5));
  input.eraseColor(SK_ColorYELLOW);

  // Serialize to string works.
  auto serialized = skia::mojom::InlineBitmap::Serialize(&input);
  SkBitmap output;
  ASSERT_TRUE(
      skia::mojom::InlineBitmap::Deserialize(std::move(serialized), &output));
  EXPECT_EQ(input.info(), output.info());
  EXPECT_EQ(input.rowBytes(), output.rowBytes());
  EXPECT_TRUE(gfx::BitmapsAreEqual(input, output));
}

TEST(StructTraitsTest, InlineBitmapSerializeInvalidRowBytes) {
  SkBitmap input;
  input.allocPixels(SkImageInfo::MakeN32Premul(10, 5), 11 * 4);

  // We do not allow sending rowBytes() other than the minRowBytes().
  EXPECT_DEATH(skia::mojom::InlineBitmap::SerializeAsMessage(&input), "");
}

TEST(StructTraitsTest, InlineBitmapSerializeInvalidColorType) {
  SkBitmap input;
  input.allocPixels(SkImageInfo::MakeA8(10, 5));

  // We do not allow sending colorType() other than the kN32_SkColorType.
  EXPECT_DEATH(skia::mojom::InlineBitmap::SerializeAsMessage(&input), "");
}

TEST(StructTraitsTest, VerifyInlineBitmapConstruction) {
  // Verify that we can manually construct a valid skia.mojom.InlineBitmap and
  // deserialize it successfully.
  mojo::StructPtr<skia::mojom::InlineBitmap> input =
      ConstructInlineBitmap(SkImageInfo::MakeN32Premul(1, 1), {1, 2, 3, 4});

  SkBitmap output;
  bool ok = mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
      input, output);
  EXPECT_TRUE(ok);
}

TEST(StructTraitsTest, InlineBitmapDeserializeTooFewBytes) {
  mojo::StructPtr<skia::mojom::InlineBitmap> input =
      ConstructInlineBitmap(SkImageInfo::MakeN32Premul(2, 1), {1, 2, 3, 4});

  SkBitmap output;
  bool ok = mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
      input, output);
  EXPECT_FALSE(ok);
}

TEST(StructTraitsTest, InlineBitmapDeserializeTooManyBytes) {
  mojo::StructPtr<skia::mojom::InlineBitmap> input = ConstructInlineBitmap(
      SkImageInfo::MakeN32Premul(1, 1), {1, 2, 3, 4, 5, 6, 7, 8});

  SkBitmap output;
  bool ok = mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
      input, output);
  EXPECT_FALSE(ok);
}

}  // namespace
}  // namespace skia
