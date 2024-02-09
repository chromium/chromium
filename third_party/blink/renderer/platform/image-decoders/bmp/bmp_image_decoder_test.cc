// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_base_test.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkAlphaType.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS))
// GN deps checking doesn't understand #if guards, so we need to use nogncheck
// here: https://gn.googlesource.com/gn/+/main/docs/reference.md#nogncheck
#include "ui/base/test/skia_gold_matching_algorithm.h"  // nogncheck
#include "ui/base/test/skia_gold_pixel_diff.h"          // nogncheck
#endif

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateBMPDecoder() {
  return std::make_unique<BMPImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::kTransformToSRGB,
      ImageDecoder::kNoDecodedImageByteLimit);
}

}  // anonymous namespace

TEST(BMPImageDecoderTest, isSizeAvailable) {
  // This image is 256x256.
  static constexpr char kBmpFile[] = "/images/resources/gracehopper.bmp";
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(256, decoder->Size().width());
  EXPECT_EQ(256, decoder->Size().height());
}

TEST(BMPImageDecoderTest, parseAndDecode) {
  // This image is 256x256.
  static constexpr char kBmpFile[] = "/images/resources/gracehopper.bmp";
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(256, frame->Bitmap().width());
  EXPECT_EQ(256, frame->Bitmap().height());
  EXPECT_FALSE(decoder->Failed());
}

// Test if a BMP decoder returns a proper error while decoding an empty image.
TEST(BMPImageDecoderTest, emptyImage) {
  static constexpr char kBmpFile[] = "/images/resources/0x0.bmp";  // 0x0
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameEmpty, frame->GetStatus());
  EXPECT_TRUE(decoder->Failed());
}

TEST(BMPImageDecoderTest, int32MinHeight) {
  static constexpr char kBmpFile[] =
      "/images/resources/1xint32_min.bmp";  // 0xINT32_MIN
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  // Test when not all data is received.
  decoder->SetData(data.get(), false);
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->Failed());
}

// This test verifies that calling SharedBuffer::MergeSegmentsIntoBuffer() does
// not break BMP decoding at a critical point: in between a call to decode the
// size (when BMPImageDecoder stops while it may still have input data to
// read) and a call to do a full decode.
TEST(BMPImageDecoderTest, mergeBuffer) {
  static constexpr char kBmpFile[] = "/images/resources/gracehopper.bmp";
  TestMergeBuffer(&CreateBMPDecoder, kBmpFile);
}

// Verify that decoding this image does not crash.
TEST(BMPImageDecoderTest, crbug752898) {
  static constexpr char kBmpFile[] = "/images/resources/crbug752898.bmp";
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  decoder->DecodeFrameBufferAtIndex(0);
}

// Verify that decoding this image does not crash.
TEST(BMPImageDecoderTest, invalidBitmapOffset) {
  static constexpr char kBmpFile[] =
      "/images/resources/invalid-bitmap-offset.bmp";
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

// Verify that decoding an image with an unnecessary EOF marker does not crash.
TEST(BMPImageDecoderTest, allowEOFWhenPastEndOfImage) {
  static constexpr char kBmpFile[] = "/images/resources/unnecessary-eof.bmp";
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

TEST(BMPImageDecoderTest, VerifyBMPSuite) {
  struct BMPSuiteEntry {
    const char* dir;
    const char* bmp;
  };
  static constexpr BMPSuiteEntry kBMPSuiteEntries[] = {
      {"good", "pal1"},
      {"good", "pal1wb"},
      {"good", "pal1bg"},
      {"good", "pal4"},
      {"good", "pal4gs"},
      {"good", "pal4rle"},
      {"good", "pal8"},
      {"good", "pal8-0"},
      {"good", "pal8gs"},
      {"good", "pal8rle"},
      {"good", "pal8w126"},
      {"good", "pal8w125"},
      {"good", "pal8w124"},
      {"good", "pal8topdown"},
      {"good", "pal8nonsquare"},
      {"good", "pal8os2"},
      {"good", "pal8v4"},
      {"good", "pal8v5"},
      {"good", "rgb16"},
      {"good", "rgb16bfdef"},
      {"good", "rgb16-565"},
      {"good", "rgb16-565pal"},
      {"good", "rgb24"},
      {"good", "rgb24pal"},
      {"good", "rgb32"},
      {"good", "rgb32bfdef"},
      {"good", "rgb32bf"},

      {"questionable", "pal1p1"},
      {"questionable", "pal2"},
      {"questionable", "pal2color"},
      {"questionable", "pal4rletrns"},
      {"questionable", "pal4rlecut"},
      {"questionable", "pal8rletrns"},
      {"questionable", "pal8rlecut"},
      {"questionable", "pal8offs"},
      {"questionable", "pal8oversizepal"},
      {"questionable", "pal8os2-sz"},
      {"questionable", "pal8os2-hs"},
      {"questionable", "pal8os2sp"},
      {"questionable", "pal8os2v2"},
      {"questionable", "pal8os2v2-16"},
      {"questionable", "pal8os2v2-sz"},
      {"questionable", "pal8os2v2-40sz"},
      {"questionable", "rgb24rle24"},
      {"questionable", "pal1huffmsb"},  // We reject this encoding.
      {"questionable", "rgb16faketrns"},
      {"questionable", "rgb16-231"},
      {"questionable", "rgb16-3103"},
      {"questionable", "rgba16-4444"},
      {"questionable", "rgba16-5551"},
      {"questionable", "rgba16-1924"},
      {"questionable", "rgb24largepal"},
      // {"questionable", "rgb24prof", "rgb24"},  // Omitted--not public domain.
      // {"questionable", "rgb24prof2", "rgb24"},  //  "       "    "      "
      // {"questionable", "rgb24lprof", "rgb24"},  //  "       "    "      "
      {"questionable", "rgb24jpeg"},
      {"questionable", "rgb24png"},
      {"questionable", "rgb32h52"},
      {"questionable", "rgb32-xbgr"},
      {"questionable", "rgb32fakealpha"},
      {"questionable", "rgb32-111110"},
      {"questionable", "rgb32-7187"},
      {"questionable", "rgba32-1"},
      {"questionable", "rgba32-1010102"},
      {"questionable", "rgba32-81284"},
      {"questionable", "rgba32-61754"},
      {"questionable", "rgba32abf"},
      {"questionable", "rgba32h56"},
      // TODO: crbug.com/40244265 - a bitcount of 64 is not yet supported.
      {"questionable", "rgba64"},

      {"bad", "badbitcount"},
      {"bad", "badbitssize"},
      {"bad", "baddens1"},
      {"bad", "baddens2"},
      {"bad", "badfilesize"},
      {"bad", "badheadersize"},
      {"bad", "badpalettesize"},
      {"bad", "badplanes"},
      {"bad", "badrle"},
      {"bad", "badrle4"},
      {"bad", "badrle4bis"},
      {"bad", "badrle4ter"},
      {"bad", "badrlebis"},
      {"bad", "badrleter"},
      {"bad", "badwidth"},
      {"bad", "pal8badindex"},
      {"bad", "reallybig"},
      {"bad", "rgb16-880"},
      {"bad", "rletopdown"},
      {"bad", "shortfile"},
  };

  for (const BMPSuiteEntry& entry : kBMPSuiteEntries) {
    // Load the BMP file under test.
    std::string bmp_path =
        base::StringPrintf("/images/bmp-suite/%s/%s.bmp", entry.dir, entry.bmp);
    scoped_refptr<SharedBuffer> data = ReadFile(bmp_path.c_str());
    ASSERT_NE(data.get(), nullptr) << "unable to load '" << bmp_path << "'";
    ASSERT_FALSE(data->empty());

    std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
    decoder->SetData(data, /*all_data_received=*/true);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);

    // Some entries in BMP Suite are intentionally invalid. These could draw
    // nonsense, or generate an error. We only need to verify that they don't
    // crash, and treat them as if they generated a 1x1 transparent bitmap.
    [[maybe_unused]] const SkBitmap* result_image;
    SkBitmap empty_bitmap;
    if (frame->GetStatus() == ImageFrame::kFrameComplete) {
      EXPECT_FALSE(decoder->Failed());
      result_image = &frame->Bitmap();
    } else {
      // Images in the "good" directory should always decode successfully.
      EXPECT_NE(entry.dir, "good");
      // Represent failures as a 1x1 transparent black pixel in Skia Gold.
      EXPECT_TRUE(decoder->Failed());
      empty_bitmap.allocPixels(SkImageInfo::MakeN32(1, 1, kPremul_SkAlphaType));
      empty_bitmap.eraseColor(SK_ColorTRANSPARENT);
      result_image = &empty_bitmap;
    }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS))
    // Verify image contents via go/chrome-engprod-skia-gold on platforms where
    // it is properly supported. On other platforms, decoding without a crash
    // counts as a pass.
    raw_ptr<ui::test::SkiaGoldPixelDiff> skia_gold =
        ui::test::SkiaGoldPixelDiff::GetSession();
    ui::test::PositiveIfOnlyImageAlgorithm positive_if_exact_image_only;
    std::string golden_name = ui::test::SkiaGoldPixelDiff::GetGoldenImageName(
        "BMPImageDecoderTest", "VerifyBMPSuite",
        base::StringPrintf("%s_%s.rev0", entry.dir, entry.bmp));
    EXPECT_TRUE(skia_gold->CompareScreenshot(golden_name, *result_image,
                                             &positive_if_exact_image_only))
        << bmp_path;
#endif
  }
}

class BMPImageDecoderCorpusTest : public ImageDecoderBaseTest {
 public:
  BMPImageDecoderCorpusTest() : ImageDecoderBaseTest("bmp") {}

 protected:
  std::unique_ptr<ImageDecoder> CreateImageDecoder() const override {
    return std::make_unique<BMPImageDecoder>(
        ImageDecoder::kAlphaPremultiplied, ColorBehavior::kTransformToSRGB,
        ImageDecoder::kNoDecodedImageByteLimit);
  }

  // The BMPImageDecoderCorpusTest tests are really slow under Valgrind.
  // Thus it is split into fast and slow versions. The threshold is
  // set to 10KB because the fast test can finish under Valgrind in
  // less than 30 seconds.
  static const int64_t kThresholdSize = 10240;
};

TEST_F(BMPImageDecoderCorpusTest, DecodingFast) {
  TestDecoding(FileSelection::kSmaller, kThresholdSize);
}

#if defined(THREAD_SANITIZER)
// BMPImageDecoderCorpusTest.DecodingSlow always times out under ThreadSanitizer
// v2.
#define MAYBE_DecodingSlow DISABLED_DecodingSlow
#else
#define MAYBE_DecodingSlow DecodingSlow
#endif
TEST_F(BMPImageDecoderCorpusTest, MAYBE_DecodingSlow) {
  TestDecoding(FileSelection::kBigger, kThresholdSize);
}

}  // namespace blink
