// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {
namespace {

sk_sp<SkImage> CreateFrameAtIndex(DeferredImageDecoder* decoder, size_t index) {
  return SkImages::DeferredFromGenerator(
      std::make_unique<SkiaPaintImageGenerator>(
          decoder->CreateGenerator(), index,
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
  base::test::SingleThreadTaskEnvironment task_environment;
  const Vector<char> file = ReadFile(file_name);

  scoped_refptr<SharedBuffer> partial_file =
      SharedBuffer::Create(base::span(file).first(bytes_for_first_frame));
  std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::Create(
      partial_file, false, ImageDecoder::kAlphaPremultiplied,
      ColorBehavior::kIgnore);
  ASSERT_NE(decoder, nullptr);
  sk_sp<SkImage> partial_image = CreateFrameAtIndex(decoder.get(), 0);

  scoped_refptr<SharedBuffer> almost_complete_file =
      SharedBuffer::Create(base::span(file).first(file.size() - 1));
  decoder->SetData(almost_complete_file, false);
  sk_sp<SkImage> image_with_more_data =
      CreateFrameAtIndex(decoder.get(), later_frame);

  // we now want to ensure we don't crash if we access these in this order
  SkImageInfo info = SkImageInfo::MakeN32Premul(10, 10);
  sk_sp<SkSurface> surf = SkSurfaces::Raster(info);
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
  MixImages("/images/resources/gracehopper.bmp", 122u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesIco) {
  MixImages("/images/resources/wrong-frame-dimensions.ico", 1376u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform,
     rustIcoMixedAspectEntriesClearsFullCanvas) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(features::kRustyIcoFeature);

  // Two-entry ICO: entry 0 is a 4x2 opaque red RGB PNG (frame 0) and entry 1
  // is a 1x4 opaque blue RGB PNG (frame 1). The default image is 4x2.
  scoped_refptr<SharedBuffer> buffer =
      ReadFileToSharedBuffer("/images/resources/rust-ico-mixed-aspect-4x2.ico");
  std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::Create(
      buffer, true, ImageDecoder::kAlphaPremultiplied, ColorBehavior::kIgnore);
  ASSERT_NE(decoder, nullptr);
  ASSERT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(gfx::Size(4, 2), decoder->Size());
  EXPECT_EQ(gfx::Size(4, 2), decoder->FrameSizeAtIndex(0));

  sk_sp<PaintImageGenerator> generator = decoder->CreateGenerator();
  ASSERT_NE(generator, nullptr);
  const SkImageInfo& info = generator->GetSkImageInfo();
  ASSERT_EQ(SkISize::Make(4, 2), info.dimensions());

  SkBitmap dst_bitmap;
  ASSERT_TRUE(dst_bitmap.tryAllocPixels(info));
  dst_bitmap.eraseColor(SK_ColorMAGENTA);

  // Frame 1 has dimensions 1x4, which does not match the 4x2 generator
  // pixmap; decoding it through the generator must fail without overwriting
  // the buffer or poisoning subsequent frame 0 decodes.
  EXPECT_FALSE(generator->GetPixels(dst_bitmap.pixmap(), /*frame_index=*/1,
                                    cc::PaintImage::GetNextGeneratorClientId(),
                                    /*lazy_pixel_ref=*/0));
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 4; ++x) {
      EXPECT_EQ(SK_ColorMAGENTA, dst_bitmap.getColor(x, y));
    }
  }

  // Frame zero must initialize every pixel in the published default image.
  dst_bitmap.eraseColor(SK_ColorMAGENTA);
  EXPECT_TRUE(generator->GetPixels(dst_bitmap.pixmap(), /*frame_index=*/0,
                                   cc::PaintImage::GetNextGeneratorClientId(),
                                   /*lazy_pixel_ref=*/0));
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 4; ++x) {
      EXPECT_EQ(SK_ColorRED, dst_bitmap.getColor(x, y));
    }
  }
}

TEST(DeferredImageDecoderTestWoPlatform,
     RustIcoReportedInputsInitializeFullPixmap) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(features::kRustyIcoFeature);
  const struct {
    const char* path;
    int payload_height;
    SkColor color;
  } cases[] = {
      {"/images/resources/rust-ico-mixed-aspect-256x2.ico", 2, SK_ColorRED},
      {"/images/resources/rust-ico-mixed-aspect-256x64.ico", 64,
       SkColorSetRGB(0x20, 0x60, 0xe0)},
      {"/images/resources/rust-ico-full-canvas.ico", 256,
       SkColorSetRGB(0x20, 0x60, 0xe0)},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.path);
    scoped_refptr<SharedBuffer> buffer = ReadFileToSharedBuffer(test.path);
    ASSERT_TRUE(buffer);
    std::unique_ptr<DeferredImageDecoder> decoder =
        DeferredImageDecoder::Create(buffer, /*data_complete=*/true,
                                     ImageDecoder::kAlphaPremultiplied,
                                     ColorBehavior::kIgnore);
    ASSERT_TRUE(decoder);
    EXPECT_EQ(gfx::Size(256, test.payload_height), decoder->Size());
    EXPECT_EQ(decoder->Size(), decoder->FrameSizeAtIndex(0));
    sk_sp<PaintImageGenerator> generator = decoder->CreateGenerator();
    ASSERT_TRUE(generator);
    ASSERT_EQ(SkISize::Make(256, test.payload_height),
              generator->GetSkImageInfo().dimensions());
    SkBitmap bitmap;
    ASSERT_TRUE(bitmap.tryAllocPixels(generator->GetSkImageInfo()));
    bitmap.eraseColor(SK_ColorMAGENTA);
    ASSERT_TRUE(generator->GetPixels(bitmap.pixmap(), 0,
                                     cc::PaintImage::GetNextGeneratorClientId(),
                                     /*lazy_pixel_ref=*/0));
    for (int y = 0; y < test.payload_height; ++y) {
      for (int x = 0; x < 256; ++x) {
        EXPECT_EQ(test.color, bitmap.getColor(x, y));
      }
    }
  }
}

TEST(DeferredImageDecoderTestWoPlatform,
     RustIcoSkippedAndOversizedEntriesInitializeFullPixmap) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(features::kRustyIcoFeature);
  const Vector<char> file =
      ReadFile("/images/resources/rust-ico-mixed-aspect-256x2.ico");
  const auto source = base::as_byte_span(file);
  const uint32_t first_image_offset = IcoImageDataOffset(base::span(file), 0);
  const uint32_t larger_image_offset = IcoImageDataOffset(base::span(file), 1);
  ASSERT_LT(first_image_offset, larger_image_offset);
  ASSERT_LT(larger_image_offset, file.size());
  constexpr wtf_size_t kSecondEntryOffset =
      kIcoHeaderSize + kIcoDirectoryEntrySize;
  constexpr uint8_t kOversizedEntryHeight = 128;

  for (bool skip_larger_entry : {false, true}) {
    SCOPED_TRACE(skip_larger_entry);
    Vector<uint8_t> bytes;
    if (skip_larger_entry) {
      // Duplicate the valid entry, then corrupt the larger entry's payload.
      // Three directory entries still produce two surviving codecs, keeping
      // the decode-then-copy path used by the reports.
      bytes.append_range(source.first(kSecondEntryOffset));
      bytes.append_range(
          source.subspan(kIcoHeaderSize, kIcoDirectoryEntrySize));
      bytes.append_range(source.subspan(kSecondEntryOffset));
      const std::array<uint32_t, 3> shifted_offsets = {
          first_image_offset + kIcoDirectoryEntrySize,
          first_image_offset + kIcoDirectoryEntrySize,
          larger_image_offset + kIcoDirectoryEntrySize};
      for (wtf_size_t index = 0; index < shifted_offsets.size(); ++index) {
        const auto encoded_offset =
            base::U32ToLittleEndian(shifted_offsets[index]);
        base::span(bytes)
            .subspan(kIcoHeaderSize + index * kIcoDirectoryEntrySize +
                         kIcoEntryImageOffset,
                     sizeof(uint32_t))
            .copy_from(encoded_offset);
      }
      const auto encoded_count = base::U16ToLittleEndian(
          base::checked_cast<uint16_t>(shifted_offsets.size()));
      base::span(bytes)
          .subspan<kIcoImageCountOffset, sizeof(uint16_t)>()
          .copy_from(encoded_count);
      bytes[shifted_offsets.back()] = 0;
    } else {
      bytes.append_range(source);
      // The 1x256 payload exceeds its declared 1x128 frame but still raises
      // the container height above the valid 256x2 frame.
      bytes[kSecondEntryOffset + kIcoEntryHeightOffset] = kOversizedEntryHeight;
    }

    scoped_refptr<SharedBuffer> buffer =
        SharedBuffer::Create(base::span(bytes));
    std::unique_ptr<DeferredImageDecoder> decoder =
        DeferredImageDecoder::Create(buffer, /*data_complete=*/true,
                                     ImageDecoder::kAlphaPremultiplied,
                                     ColorBehavior::kIgnore);
    ASSERT_TRUE(decoder);
    ASSERT_EQ(2u, decoder->FrameCount());
    EXPECT_EQ(gfx::Size(256, 2), decoder->Size());
    EXPECT_EQ(decoder->Size(), decoder->FrameSizeAtIndex(0));
    sk_sp<PaintImageGenerator> generator = decoder->CreateGenerator();
    ASSERT_TRUE(generator);
    ASSERT_EQ(SkISize::Make(256, 2), generator->GetSkImageInfo().dimensions());
    SkBitmap bitmap;
    ASSERT_TRUE(bitmap.tryAllocPixels(generator->GetSkImageInfo()));
    bitmap.eraseColor(SK_ColorMAGENTA);
    ASSERT_TRUE(generator->GetPixels(bitmap.pixmap(), 0,
                                     cc::PaintImage::GetNextGeneratorClientId(),
                                     /*lazy_pixel_ref=*/0));
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 256; ++x) {
        EXPECT_EQ(SK_ColorRED, bitmap.getColor(x, y));
      }
    }
  }
}

TEST(DeferredImageDecoderTestWoPlatform,
     rustIcoProgressiveDecodeClearsFullCanvas) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(features::kRustyIcoFeature);

  const Vector<char> file = ReadFile("/images/resources/2entries.ico");
  const uint32_t partial_size = IcoImageDataOffset(base::span(file), 1);
  ASSERT_GT(file.size(), partial_size);

  // Only the 16x16 entry is complete; the 32x32 entry is missing.
  scoped_refptr<SharedBuffer> partial_buffer =
      SharedBuffer::Create(base::span(file).first(partial_size));
  std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::Create(
      partial_buffer, /*data_complete=*/false,
      ImageDecoder::kAlphaPremultiplied, ColorBehavior::kIgnore);
  ASSERT_NE(decoder, nullptr);
  ASSERT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(gfx::Size(32, 32), decoder->Size());
  EXPECT_EQ(gfx::Size(32, 32), decoder->FrameSizeAtIndex(0));

  sk_sp<PaintImageGenerator> partial_generator = decoder->CreateGenerator();
  ASSERT_NE(partial_generator, nullptr);
  const SkImageInfo& partial_info = partial_generator->GetSkImageInfo();
  ASSERT_EQ(SkISize::Make(32, 32), partial_info.dimensions());

  const auto client_id = cc::PaintImage::GetNextGeneratorClientId();
  SkBitmap partial_bitmap;
  ASSERT_TRUE(partial_bitmap.tryAllocPixels(partial_info));
  partial_bitmap.eraseColor(SK_ColorMAGENTA);
  ASSERT_TRUE(partial_generator->GetPixels(partial_bitmap.pixmap(), 0,
                                           client_id, /*lazy_pixel_ref=*/0));
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      if (x >= 16 || y >= 16) {
        EXPECT_EQ(SK_ColorTRANSPARENT, partial_bitmap.getColor(x, y));
      }
    }
  }

  scoped_refptr<SharedBuffer> full_buffer =
      SharedBuffer::Create(base::span(file));
  decoder->SetData(full_buffer, /*all_data_received=*/true);
  sk_sp<PaintImageGenerator> full_generator = decoder->CreateGenerator();
  ASSERT_NE(full_generator, nullptr);
  ASSERT_EQ(SkISize::Make(32, 32),
            full_generator->GetSkImageInfo().dimensions());
  SkBitmap full_bitmap;
  ASSERT_TRUE(full_bitmap.tryAllocPixels(full_generator->GetSkImageInfo()));
  full_bitmap.eraseColor(SK_ColorMAGENTA);
  ASSERT_TRUE(full_generator->GetPixels(full_bitmap.pixmap(), 0, client_id,
                                        /*lazy_pixel_ref=*/0));
  // Compare every pixel against a fresh decode of the complete input.
  std::unique_ptr<DeferredImageDecoder> reference_decoder =
      DeferredImageDecoder::Create(full_buffer, /*data_complete=*/true,
                                   ImageDecoder::kAlphaPremultiplied,
                                   ColorBehavior::kIgnore);
  ASSERT_TRUE(reference_decoder);
  sk_sp<PaintImageGenerator> reference_generator =
      reference_decoder->CreateGenerator();
  ASSERT_TRUE(reference_generator);
  ASSERT_EQ(full_generator->GetSkImageInfo().dimensions(),
            reference_generator->GetSkImageInfo().dimensions());
  SkBitmap reference_bitmap;
  ASSERT_TRUE(
      reference_bitmap.tryAllocPixels(reference_generator->GetSkImageInfo()));
  reference_bitmap.eraseColor(SK_ColorTRANSPARENT);
  ASSERT_TRUE(reference_generator->GetPixels(
      reference_bitmap.pixmap(), 0, cc::PaintImage::GetNextGeneratorClientId(),
      /*lazy_pixel_ref=*/0));
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      EXPECT_EQ(reference_bitmap.getColor(x, y), full_bitmap.getColor(x, y));
    }
  }
}

TEST(DeferredImageDecoderTestWoPlatform, fragmentedSignature) {
  base::test::SingleThreadTaskEnvironment task_environment;
  constexpr auto test_files = std::to_array<const char*>({
      "/images/resources/animated.gif",
      "/images/resources/mu.png",
      "/images/resources/2-dht.jpg",
      "/images/resources/webp-animated.webp",
      "/images/resources/gracehopper.bmp",
      "/images/resources/wrong-frame-dimensions.ico",
  });

  for (const auto* test_file : test_files) {
    Vector<char> file_data = ReadFile(test_file);
    auto [first_byte, rest_of_data] = base::span(file_data).split_at(1u);

    // Truncated signature (only 1 byte).  Decoder instantiation should fail.
    scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create(first_byte);
    EXPECT_FALSE(ImageDecoder::HasSufficientDataToSniffMimeType(
        *buffer, /*all_data_received=*/false));
    EXPECT_EQ(nullptr, DeferredImageDecoder::Create(
                           buffer, false, ImageDecoder::kAlphaPremultiplied,
                           ColorBehavior::kIgnore));

    // Append the rest of the data.  We should be able to sniff the signature
    // now, even if segmented. Keep all_data_received false to exercise the
    // incremental data path.
    buffer->Append(rest_of_data);
    EXPECT_TRUE(ImageDecoder::HasSufficientDataToSniffMimeType(
        *buffer, /*all_data_received=*/false));
    std::unique_ptr<DeferredImageDecoder> decoder =
        DeferredImageDecoder::Create(buffer, false,
                                     ImageDecoder::kAlphaPremultiplied,
                                     ColorBehavior::kIgnore);
    ASSERT_NE(decoder, nullptr);
    EXPECT_TRUE(String(test_file).ends_with(decoder->FilenameExtension()));
  }
}

#if BUILDFLAG(ENABLE_JXL_DECODER)
// A complete image can be shorter than the longest signature used for MIME
// sniffing: the smallest valid naked JPEG XL codestream is 12 bytes. Decoder
// instantiation must succeed once all data is received, even if the data is
// too short for regular signature sniffing. See crbug.com/507903802.
TEST(DeferredImageDecoderTestWoPlatform, completeDataShorterThanSniffLength) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(features::kJXLImageFormat);

  // base64: /wrfBwiDBAwASyAY, a 256x128 all-black image.
  static constexpr std::array<uint8_t, 12> kSmallJxl = {
      0xFF, 0x0A, 0xDF, 0x07, 0x08, 0x83, 0x04, 0x0C, 0x00, 0x4B, 0x20, 0x18};
  scoped_refptr<SharedBuffer> buffer =
      SharedBuffer::Create(base::span(kSmallJxl));

  // With incomplete data, there is not enough to sniff the MIME type.
  EXPECT_FALSE(ImageDecoder::HasSufficientDataToSniffMimeType(
      *buffer, /*all_data_received=*/false));
  EXPECT_EQ(nullptr, DeferredImageDecoder::Create(
                         buffer, false, ImageDecoder::kAlphaPremultiplied,
                         ColorBehavior::kIgnore));

  // With complete data, sniffing must proceed with the available bytes.
  EXPECT_TRUE(ImageDecoder::HasSufficientDataToSniffMimeType(
      *buffer, /*all_data_received=*/true));
  std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::Create(
      buffer, true, ImageDecoder::kAlphaPremultiplied, ColorBehavior::kIgnore);
  ASSERT_NE(decoder, nullptr);
  EXPECT_EQ("jxl", decoder->FilenameExtension());
  ASSERT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(256, decoder->Size().width());
  EXPECT_EQ(128, decoder->Size().height());

  sk_sp<SkImage> image = CreateFrameAtIndex(decoder.get(), 0);
  ASSERT_NE(image, nullptr);

  // Force a lazy decode by reading pixels.
  SkImageInfo info = SkImageInfo::MakeN32Premul(256, 128);
  Vector<char> storage(
      base::checked_cast<wtf_size_t>(info.computeMinByteSize()));
  SkPixmap pixmap(info, storage.data(), info.minRowBytes());
  EXPECT_TRUE(image->readPixels(pixmap, 0, 0));
}

// A high-bit-depth image is decoded into the F16 buffer that the generator's
// SkImageInfo describes. The worker decoder must tag its output with the same
// color space as the metadata decoder, or the one-shot decode into external
// memory fails and ImageFrameGenerator latches a permanent decode failure.
// A 16-bit lossy (XYB-encoded) JPEG XL file with an embedded ICC profile hits
// this: jxl-rs outputs linear sRGB for float formats but sRGB for integer
// formats. See crbug.com/574339645.
TEST(DeferredImageDecoderTestWoPlatform, highBitDepthDecodeFromCompleteData) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(features::kJXLImageFormat);

  const Vector<char> file = ReadFile("/images/resources/lossy_16bit_icc.jxl");
  scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create(base::span(file));

  std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::Create(
      buffer, true, ImageDecoder::kAlphaPremultiplied, ColorBehavior::kTag);
  ASSERT_NE(decoder, nullptr);
  ASSERT_TRUE(decoder->IsSizeAvailable());

  sk_sp<PaintImageGenerator> generator = decoder->CreateGenerator();
  ASSERT_NE(generator, nullptr);
  const SkImageInfo& info = generator->GetSkImageInfo();
  ASSERT_EQ(kRGBA_F16_SkColorType, info.colorType());

  Vector<char> storage(
      base::checked_cast<wtf_size_t>(info.computeMinByteSize()));
  SkPixmap pixmap(info, storage.data(), info.minRowBytes());
  EXPECT_TRUE(generator->GetPixels(pixmap, 0,
                                   cc::PaintImage::GetNextGeneratorClientId(),
                                   /*lazy_pixel_ref=*/0));
}
#endif  // BUILDFLAG(ENABLE_JXL_DECODER)

}  // namespace blink
