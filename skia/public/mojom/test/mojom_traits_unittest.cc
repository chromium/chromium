// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include "mojo/public/cpp/test_support/test_utils.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "skia/public/mojom/bitmap.mojom.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "skia/public/mojom/image_info.mojom-shared.h"
#include "skia/public/mojom/image_info.mojom.h"
#include "skia/public/mojom/skcolorspace.mojom.h"
#include "skia/public/mojom/skcolorspace_mojom_traits.h"
#include "skia/public/mojom/skcolorspace_primaries.mojom.h"
#include "skia/public/mojom/skcolorspace_primaries_mojom_traits.h"
#include "skia/public/mojom/tile_mode.mojom.h"
#include "skia/public/mojom/tile_mode_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/skia_util.h"

namespace skia {
namespace {

// A helper to construct a skia.mojom.BitmapN32 without using StructTraits
// to bypass checks on the sending/serialization side.
mojo::StructPtr<skia::mojom::BitmapN32> ConstructBitmapN32(
    SkImageInfo info,
    std::vector<unsigned char> pixels) {
  auto mojom_bitmap = skia::mojom::BitmapN32::New();
  mojom_bitmap->image_info = std::move(info);
  mojom_bitmap->pixel_data = std::move(pixels);
  return mojom_bitmap;
}

// A helper to construct a skia.mojom.BitmapWithArbitraryBpp without using
// StructTraits to bypass checks on the sending/serialization side.
mojo::StructPtr<skia::mojom::BitmapWithArbitraryBpp>
ConstructBitmapWithArbitraryBpp(SkImageInfo info,
                                int row_bytes,
                                std::vector<unsigned char> pixels) {
  auto mojom_bitmap = skia::mojom::BitmapWithArbitraryBpp::New();
  mojom_bitmap->image_info = std::move(info);
  mojom_bitmap->UNUSED_row_bytes = row_bytes;
  mojom_bitmap->pixel_data = std::move(pixels);
  return mojom_bitmap;
}

// A helper to construct a skia.mojom.BitmapMappedFromTrustedProcess without
// using StructTraits to bypass checks on the sending/serialization side.
mojo::StructPtr<skia::mojom::BitmapMappedFromTrustedProcess>
ConstructBitmapMappedFromTrustedProcess(SkImageInfo info,
                                        int row_bytes,
                                        std::vector<unsigned char> pixels) {
  auto mojom_bitmap = skia::mojom::BitmapMappedFromTrustedProcess::New();
  mojom_bitmap->image_info = std::move(info);
  mojom_bitmap->UNUSED_row_bytes = row_bytes;
  mojom_bitmap->pixel_data = mojo_base::BigBuffer(std::move(pixels));
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

// A helper to construct a skia.mojom.ImageInfo without using StructTraits
// to bypass checks on the sending/serialization side.
mojo::StructPtr<skia::mojom::ImageInfo> ConstructImageInfo(
    SkColorType color_type,
    SkAlphaType alpha_type,
    uint32_t width,
    uint32_t height) {
  auto mojom_info = skia::mojom::ImageInfo::New();
  mojom_info->color_type = color_type;
  mojom_info->alpha_type = alpha_type;
  mojom_info->width = width;
  mojom_info->height = height;
  return mojom_info;
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

// We catch negative integers on the sending side and crash, when struct traits
// are used.
TEST(StructTraitsDeathTest, ImageInfoOverflowSizeWithStructTrait) {
  SkImageInfo input = SkImageInfo::Make(
      std::numeric_limits<uint32_t>::max(),
      std::numeric_limits<uint32_t>::max(), SkColorType::kGray_8_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB));
  SkImageInfo output;
  EXPECT_DEATH(skia::mojom::ImageInfo::SerializeAsMessage(&input), "");
}

// We must reject sizes that would cause integer overflow on the receiving side.
// The wire format is `uint32_t`, but Skia needs us to convert that to an `int`
// for the SkImageInfo type.
TEST(StructTraitsTest, ImageInfoOverflowSizeWithoutStructTrait) {
  SkImageInfo output;
  mojo::StructPtr<skia::mojom::ImageInfo> input = ConstructImageInfo(
      SkColorType::kGray_8_SkColorType, SkAlphaType::kUnpremul_SkAlphaType,
      std::numeric_limits<uint32_t>::max(),
      std::numeric_limits<uint32_t>::max());
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::ImageInfo>(
      input, output));
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

TEST(StructTraitsTest, SkColorSpace) {
  skcms_TransferFunction in_trfn{0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f};
  skcms_TransferFunction out_trfn;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<skia::mojom::SkcmsTransferFunction>(
          in_trfn, out_trfn));
  EXPECT_EQ(memcmp(&in_trfn, &out_trfn, sizeof(in_trfn)), 0);

  skcms_Matrix3x3 in_to_xyzd50{
      .vals = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}, {0.7f, 0.8f, 0.9f}}};
  skcms_Matrix3x3 out_to_xyzd50;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::SkcmsMatrix3x3>(
      in_to_xyzd50, out_to_xyzd50));
  EXPECT_EQ(memcmp(&in_to_xyzd50, &out_to_xyzd50, sizeof(in_to_xyzd50)), 0);

  sk_sp<SkColorSpace> in_cs = SkColorSpace::MakeRGB(in_trfn, in_to_xyzd50);
  sk_sp<SkColorSpace> out_cs;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::SkColorSpace>(
      in_cs, out_cs));
  EXPECT_TRUE(SkColorSpace::Equals(in_cs.get(), out_cs.get()));

  sk_sp<SkColorSpace> in_null_cs;
  sk_sp<SkColorSpace> out_null_cs;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::SkColorSpace>(
      in_null_cs, out_null_cs));
  EXPECT_EQ(out_null_cs.get(), nullptr);

  SkColorSpacePrimaries in_p = SkNamedPrimariesExt::kGenericFilm;
  SkColorSpacePrimaries out_p;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<skia::mojom::SkColorSpacePrimaries>(
          in_p, out_p));
  EXPECT_TRUE(in_p == out_p);
}

TEST(StructTraitsTest, TileMode) {
  SkTileMode input(SkTileMode::kClamp);
  SkTileMode output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::TileMode>(
      input, output));
  EXPECT_EQ(input, output);
  input = SkTileMode::kRepeat;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::TileMode>(
      input, output));
  EXPECT_EQ(input, output);
  input = SkTileMode::kMirror;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::TileMode>(
      input, output));
  EXPECT_EQ(input, output);
  input = SkTileMode::kDecal;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::TileMode>(
      input, output));
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

  auto BitmapsEqual = [](const SkBitmap& input, const SkBitmap& output) {
    EXPECT_EQ(input.info(), output.info());
    EXPECT_EQ(input.rowBytes(), output.rowBytes());
    EXPECT_TRUE(gfx::BitmapsAreEqual(input, output));
  };

  {
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::BitmapN32>(
        input, output));
    BitmapsEqual(input, output);
  }
  {
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                skia::mojom::BitmapWithArbitraryBpp>(input, output));
    BitmapsEqual(input, output);
  }
  {
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                skia::mojom::BitmapMappedFromTrustedProcess>(input, output));
    BitmapsEqual(input, output);
  }
  {
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
        input, output));
    BitmapsEqual(input, output);
  }
}

// Null input produces a default-initialized SkBitmap.
TEST(StructTraitsTest, BitmapNull) {
  SkBitmap input;
  input.setInfo(SkImageInfo::MakeN32Premul(
      10, 5,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear,
                            SkNamedGamut::kRec2020)));
  EXPECT_TRUE(input.isNull());

  auto IsDefaultInit = [](const SkBitmap& output) {
    EXPECT_EQ(output.info().alphaType(), kUnknown_SkAlphaType);
    EXPECT_EQ(output.info().colorType(), kUnknown_SkColorType);
    EXPECT_EQ(output.rowBytes(), 0u);
    EXPECT_TRUE(output.isNull());
  };

  SkBitmap output;
  {
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::BitmapN32>(
        input, output));
    IsDefaultInit(output);
  }
  {
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                skia::mojom::BitmapWithArbitraryBpp>(input, output));
    IsDefaultInit(output);
  }
  {
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                skia::mojom::BitmapMappedFromTrustedProcess>(input, output));
    IsDefaultInit(output);
  }
  {
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
        input, output));
    IsDefaultInit(output);
  }
}

// Serialize to string works, we only need this verify this for InlineBitmap,
// as the other Bitmap types should not be used for this purpose.
TEST(StructTraitsTest, InlineBitmapSerializeToString) {
  SkBitmap input;
  input.allocPixels(SkImageInfo::MakeN32Premul(10, 5));
  input.eraseColor(SK_ColorYELLOW);

  auto serialized = skia::mojom::InlineBitmap::Serialize(&input);
  SkBitmap output;
  ASSERT_TRUE(
      skia::mojom::InlineBitmap::Deserialize(std::move(serialized), &output));
  EXPECT_EQ(input.info(), output.info());
  EXPECT_EQ(input.rowBytes(), output.rowBytes());
  EXPECT_TRUE(gfx::BitmapsAreEqual(input, output));
}

// Verify that we can manually construct a valid skia.mojom object and
// deserialize it successfully.
TEST(StructTraitsTest, VerifyMojomConstruction) {
  SkBitmap output;

  {
    mojo::StructPtr<skia::mojom::BitmapN32> input =
        ConstructBitmapN32(SkImageInfo::MakeN32Premul(1, 1), {1, 2, 3, 4});
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::BitmapN32>(
        input, output));
  }
  {
    mojo::StructPtr<skia::mojom::BitmapWithArbitraryBpp> input =
        ConstructBitmapWithArbitraryBpp(SkImageInfo::MakeN32Premul(1, 1), 0,
                                        {1, 2, 3, 4});
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                skia::mojom::BitmapWithArbitraryBpp>(input, output));
  }
  {
    mojo::StructPtr<skia::mojom::BitmapMappedFromTrustedProcess> input =
        ConstructBitmapMappedFromTrustedProcess(
            SkImageInfo::MakeN32Premul(1, 1), 0, {1, 2, 3, 4});
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                skia::mojom::BitmapMappedFromTrustedProcess>(input, output));
  }
  {
    mojo::StructPtr<skia::mojom::InlineBitmap> input =
        ConstructInlineBitmap(SkImageInfo::MakeN32Premul(1, 1), {1, 2, 3, 4});
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
        input, output));
  }
}

// We only allow 64 * 1024 as the max width.
TEST(StructTraitsTest, BitmapTooWideToSerialize) {
  constexpr int kTooWide = 64 * 1024 + 1;
  SkBitmap input;
  input.allocPixels(
      SkImageInfo::MakeN32(kTooWide, 1, SkAlphaType::kUnpremul_SkAlphaType));
  input.eraseColor(SK_ColorYELLOW);
  SkBitmap output;
  {
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::BitmapN32>(
        input, output));
  }
  {
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
                 skia::mojom::BitmapWithArbitraryBpp>(input, output));
  }
  {
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
                 skia::mojom::BitmapMappedFromTrustedProcess>(input, output));
  }
  {
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
        input, output));
  }
}

// We only allow 64 * 1024 as the max height.
TEST(StructTraitsTest, BitmapTooTallToSerialize) {
  constexpr int kTooTall = 64 * 1024 + 1;
  SkBitmap input;
  input.allocPixels(
      SkImageInfo::MakeN32(1, kTooTall, SkAlphaType::kUnpremul_SkAlphaType));
  input.eraseColor(SK_ColorYELLOW);
  SkBitmap output;
  {
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::BitmapN32>(
        input, output));
  }
  {
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
                 skia::mojom::BitmapWithArbitraryBpp>(input, output));
  }
  {
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
                 skia::mojom::BitmapMappedFromTrustedProcess>(input, output));
  }
  {
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
        input, output));
  }
}

template <typename MojomType>
static void BadRowBytes() {
  SkImageInfo info =
      SkImageInfo::MakeN32(8, 5, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
  const size_t row_bytes = info.minRowBytes() + info.bytesPerPixel();
  SkBitmap input;
  EXPECT_TRUE(input.tryAllocPixels(info, row_bytes));
  // This will crash.
  EXPECT_DEATH(MojomType::SerializeAsMessage(&input), "");
}

// We do not allow sending rowBytes() other than the minRowBytes().
TEST(StructTraitsTest, BitmapSerializeInvalidRowBytes_BitmapN32) {
  BadRowBytes<skia::mojom::BitmapN32>();
}
TEST(StructTraitsTest, BitmapSerializeInvalidRowBytes_BitmapWithArbitraryBpp) {
  BadRowBytes<skia::mojom::BitmapWithArbitraryBpp>();
}
TEST(StructTraitsTest,
     BitmapSerializeInvalidRowBytes_BitmapMappedFromTrustedProcess) {
  BadRowBytes<skia::mojom::BitmapMappedFromTrustedProcess>();
}
TEST(StructTraitsTest, BitmapSerializeInvalidRowBytes_InlineBitmap) {
  BadRowBytes<skia::mojom::InlineBitmap>();
}

template <typename MojomType>
static void BadColor(bool expect_crash) {
  SkImageInfo info = SkImageInfo::MakeA8(10, 5);
  SkBitmap input;
  EXPECT_TRUE(input.tryAllocPixels(info));
  if (expect_crash) {
    // This will crash.
    EXPECT_DEATH(MojomType::SerializeAsMessage(&input), "");
  } else {
    // This won't as the mojom allows arbitrary color formats.
    MojomType::SerializeAsMessage(&input);
  }
}

TEST(StructTraitsTest, BitmapSerializeInvalidColorType_BitmapN32) {
  BadColor<skia::mojom::BitmapN32>(/*expect_crash=*/true);
}
TEST(StructTraitsTest, BitmapSerializeInvalidColorType_BitmapWithArbitraryBpp) {
  BadColor<skia::mojom::BitmapWithArbitraryBpp>(/*expect_crash=*/false);
}
TEST(StructTraitsTest,
     BitmapSerializeInvalidColorType_BitmapMappedFromTrustedProcess) {
  BadColor<skia::mojom::BitmapMappedFromTrustedProcess>(/*expect_crash=*/false);
}
TEST(StructTraitsTest, BitmapSerializeInvalidColorType_InlineBitmap) {
  BadColor<skia::mojom::InlineBitmap>(/*expect_crash=*/true);
}

// The row_bytes field is ignored, and the minRowBytes() is always used.
TEST(StructTraitsTest, BitmapDeserializeIgnoresRowBytes) {
  SkBitmap output;

  size_t ignored_row_bytes = 8;
  size_t expected_row_bytes = 4;
  {
    mojo::StructPtr<skia::mojom::BitmapWithArbitraryBpp> input =
        ConstructBitmapWithArbitraryBpp(SkImageInfo::MakeN32Premul(1, 1),
                                        ignored_row_bytes, {1, 2, 3, 4});
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                skia::mojom::BitmapWithArbitraryBpp>(input, output));
    EXPECT_EQ(expected_row_bytes, output.rowBytes());
  }
  {
    mojo::StructPtr<skia::mojom::BitmapMappedFromTrustedProcess> input =
        ConstructBitmapMappedFromTrustedProcess(
            SkImageInfo::MakeN32Premul(1, 1), ignored_row_bytes, {1, 2, 3, 4});
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                skia::mojom::BitmapMappedFromTrustedProcess>(input, output));
    EXPECT_EQ(expected_row_bytes, output.rowBytes());
  }
  {
    // Neither skia::mojom::BitmapN32 nor skia::mojom::InlineBitmap have a
    // row_bytes field to test.
  }
}

// The SkImageInfo claims 8 bytes, but the pixel vector has 4.
TEST(StructTraitsTest, InlineBitmapDeserializeTooFewBytes) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(2, 1);
  std::vector<unsigned char> pixels = {1, 2, 3, 4};
  SkBitmap output;
  {
    mojo::StructPtr<skia::mojom::BitmapN32> input =
        ConstructBitmapN32(info, pixels);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::BitmapN32>(
        input, output));
  }
  {
    mojo::StructPtr<skia::mojom::BitmapWithArbitraryBpp> input =
        ConstructBitmapWithArbitraryBpp(info, 0, pixels);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
                 skia::mojom::BitmapWithArbitraryBpp>(input, output));
  }
  {
    mojo::StructPtr<skia::mojom::BitmapMappedFromTrustedProcess> input =
        ConstructBitmapMappedFromTrustedProcess(info, 0, pixels);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
                 skia::mojom::BitmapMappedFromTrustedProcess>(input, output));
  }
  {
    mojo::StructPtr<skia::mojom::InlineBitmap> input =
        ConstructInlineBitmap(info, pixels);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
        input, output));
  }
}

// The SkImageInfo claims 4 bytes, but the pixel vector has 8.
TEST(StructTraitsTest, InlineBitmapDeserializeTooManyBytes) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(1, 1);
  std::vector<unsigned char> pixels = {1, 2, 3, 4, 5, 6, 7, 8};
  SkBitmap output;
  {
    mojo::StructPtr<skia::mojom::BitmapN32> input =
        ConstructBitmapN32(info, pixels);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::BitmapN32>(
        input, output));
  }
  {
    mojo::StructPtr<skia::mojom::BitmapWithArbitraryBpp> input =
        ConstructBitmapWithArbitraryBpp(info, 0, pixels);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
                 skia::mojom::BitmapWithArbitraryBpp>(input, output));
  }
  {
    mojo::StructPtr<skia::mojom::BitmapMappedFromTrustedProcess> input =
        ConstructBitmapMappedFromTrustedProcess(info, 0, pixels);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
                 skia::mojom::BitmapMappedFromTrustedProcess>(input, output));
  }
  {
    mojo::StructPtr<skia::mojom::InlineBitmap> input =
        ConstructInlineBitmap(info, pixels);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<skia::mojom::InlineBitmap>(
        input, output));
  }
}

}  // namespace
}  // namespace skia
