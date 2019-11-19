// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {
namespace {

sk_sp<SkImage> CreateFrameAtIndex(DeferredImageDecoder* decoder, size_t index) {
  return SkImage::MakeFromGenerator(std::make_unique<SkiaPaintImageGenerator>(
      decoder->CreateGenerator(index), index,
      cc::PaintImage::kDefaultGeneratorClientId));
}

}  // namespace

/**
 *  Used to test decoding SkImages out of order.
 *  e.g.
 *  SkImage* imageA = decoder.createFrameAtIndex(0);
 *  // supply more (but not all) data to the decoder
 *  SkImage* imageB = decoder.createFrameAtIndex(laterFrame);
 *  draw(imageB);
 *  draw(imageA);
 *
 *  This results in using the same ImageDecoder (in the ImageDecodingStore) to
 *  decode less data the second time. This test ensures that it is safe to do
 *  so.
 *
 *  @param fileName File to decode
 *  @param bytesForFirstFrame Number of bytes needed to return an SkImage
 *  @param laterFrame Frame to decode with almost complete data. Can be 0.
 */
static void MixImages(const char* file_name,
                      size_t bytes_for_first_frame,
                      size_t later_frame) {
  const Vector<char> file = ReadFile(file_name)->CopyAs<Vector<char>>();

  scoped_refptr<SharedBuffer> partial_file =
      SharedBuffer::Create(file.data(), bytes_for_first_frame);
  std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::Create(
      partial_file, false, ImageDecoder::kAlphaPremultiplied,
      ColorBehavior::Ignore());
  ASSERT_NE(decoder, nullptr);
  sk_sp<SkImage> partial_image = CreateFrameAtIndex(decoder.get(), 0);

  scoped_refptr<SharedBuffer> almost_complete_file =
      SharedBuffer::Create(file.data(), file.size() - 1);
  decoder->SetData(almost_complete_file, false);
  sk_sp<SkImage> image_with_more_data =
      CreateFrameAtIndex(decoder.get(), later_frame);

  // we now want to ensure we don't crash if we access these in this order
  SkImageInfo info = SkImageInfo::MakeN32Premul(10, 10);
  sk_sp<SkSurface> surf = SkSurface::MakeRaster(info);
  surf->getCanvas()->drawImage(image_with_more_data, 0, 0);
  surf->getCanvas()->drawImage(partial_image, 0, 0);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesGif) {
  MixImages("/images/resources/animated.gif", 818u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesPng) {
  MixImages("/images/resources/mu.png", 910u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesJpg) {
  MixImages("/images/resources/2-dht.jpg", 177u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesWebp) {
  MixImages("/images/resources/webp-animated.webp", 142u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesBmp) {
  MixImages("/images/resources/lenna.bmp", 122u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesIco) {
  MixImages("/images/resources/wrong-frame-dimensions.ico", 1376u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform, fragmentedSignature) {
  const char* test_files[] = {
      "/images/resources/animated.gif",
      "/images/resources/mu.png",
      "/images/resources/2-dht.jpg",
      "/images/resources/webp-animated.webp",
      "/images/resources/lenna.bmp",
      "/images/resources/wrong-frame-dimensions.ico",
  };

  for (size_t i = 0; i < SK_ARRAY_COUNT(test_files); ++i) {
    scoped_refptr<SharedBuffer> file_buffer = ReadFile(test_files[i]);
    ASSERT_NE(file_buffer, nullptr);
    // We need contiguous data, which SharedBuffer doesn't guarantee.
    Vector<char> contiguous = file_buffer->CopyAs<Vector<char>>();
    EXPECT_EQ(contiguous.size(), file_buffer->size());
    const char* data = contiguous.data();

    // Truncated signature (only 1 byte).  Decoder instantiation should fail.
    scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create<size_t>(data, 1u);
    EXPECT_FALSE(ImageDecoder::HasSufficientDataToSniffImageType(*buffer));
    EXPECT_EQ(nullptr, DeferredImageDecoder::Create(
                           buffer, false, ImageDecoder::kAlphaPremultiplied,
                           ColorBehavior::Ignore()));

    // Append the rest of the data.  We should be able to sniff the signature
    // now, even if segmented.
    buffer->Append<size_t>(data + 1, contiguous.size() - 1);
    EXPECT_TRUE(ImageDecoder::HasSufficientDataToSniffImageType(*buffer));
    std::unique_ptr<DeferredImageDecoder> decoder =
        DeferredImageDecoder::Create(buffer, false,
                                     ImageDecoder::kAlphaPremultiplied,
                                     ColorBehavior::Ignore());
    ASSERT_NE(decoder, nullptr);
    EXPECT_TRUE(String(test_files[i]).EndsWith(decoder->FilenameExtension()));
  }
}

}  // namespace blink
