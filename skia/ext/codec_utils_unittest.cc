// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/codec_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkPngRustDecoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkStream.h"

namespace skia {
namespace {

TEST(SkiaCodecUtils, EncodeSkPixmapAsPngAsSkDataSmokeTest) {
  // Prepare a test `bitmap`.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorGREEN);

  // Encode the `bitmap` into `png`.
  sk_sp<SkData> png = EncodePngAsSkData(bitmap.pixmap());
  ASSERT_TRUE(png);

  // Decode the `png` into `roundtrip` bitmap.
  SkBitmap roundtrip;
  {
    SkCodec::Result result;
    std::unique_ptr<SkCodec> codec = SkPngRustDecoder::Decode(
        std::make_unique<SkMemoryStream>(std::move(png)), &result);
    ASSERT_TRUE(codec);
    ASSERT_EQ(result, SkCodec::kSuccess);
    roundtrip.allocN32Pixels(10, 10);
    ASSERT_EQ(SkCodec::kSuccess, codec->getPixels(roundtrip.pixmap()));
  }

  // Smoke-test that the `roundtrip` bitmap has, well, round-tripped.
  EXPECT_EQ(roundtrip.getColor(5, 5), SK_ColorGREEN);
}

TEST(SkiaCodecUtils, EncodeSkImageAsPngAsSkDataSmokeTest) {
  // Prepare a test `bitmap` / `image`.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorGREEN);
  sk_sp<SkImage> image = SkImages::RasterFromBitmap(bitmap);
  ASSERT_TRUE(image);

  // Encode the `image` into `png`.
  sk_sp<SkData> png = EncodePngAsSkData(nullptr, image.get());
  ASSERT_TRUE(png);

  // Decode the `png` into `roundtrip` bitmap.
  SkBitmap roundtrip;
  {
    SkCodec::Result result;
    std::unique_ptr<SkCodec> codec = SkPngRustDecoder::Decode(
        std::make_unique<SkMemoryStream>(std::move(png)), &result);
    ASSERT_TRUE(codec);
    ASSERT_EQ(result, SkCodec::kSuccess);
    roundtrip.allocN32Pixels(10, 10);
    ASSERT_EQ(SkCodec::kSuccess, codec->getPixels(roundtrip.pixmap()));
  }

  // Smoke-test that the `roundtrip` bitmap has, well, round-tripped.
  EXPECT_EQ(roundtrip.getColor(5, 5), SK_ColorGREEN);
}

TEST(SkiaCodecUtils, EncodePngAsDataUriSmokeTest) {
  // Prepare a test `bitmap`.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorGREEN);

  // Encode the `bitmap` into a data `uri`.
  std::string uri = EncodePngAsDataUri(bitmap.pixmap());

  // Smoke test the `uri`.  We don't verify exact equality to avoid depending in
  // internal implementation details of the encoder.
  EXPECT_THAT(uri, testing::StartsWith("data:image/png;"));
}

}  // namespace
}  // namespace skia
