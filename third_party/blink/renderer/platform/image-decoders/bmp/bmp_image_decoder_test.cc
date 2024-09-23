// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"

#include <memory>
#include <string>
#include <tuple>

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
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
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
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
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
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
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
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  // Test when not all data is received.
  decoder->SetData(data.get(), false);
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->Failed());
}

// Verify that decoding this image does not crash.
TEST(BMPImageDecoderTest, crbug752898) {
  static constexpr char kBmpFile[] = "/images/resources/crbug752898.bmp";
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  decoder->DecodeFrameBufferAtIndex(0);
}

// Verify that decoding this image does not crash.
TEST(BMPImageDecoderTest, invalidBitmapOffset) {
  static constexpr char kBmpFile[] =
      "/images/resources/invalid-bitmap-offset.bmp";
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

// Verify that decoding an image with an unnecessary EOF marker does not crash.
TEST(BMPImageDecoderTest, allowEOFWhenPastEndOfImage) {
  static constexpr char kBmpFile[] = "/images/resources/unnecessary-eof.bmp";
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

using BMPSuiteEntry = std::tuple<std::string, std::string>;
class BMPImageDecoderTest : public testing::TestWithParam<BMPSuiteEntry> {};

TEST_P(BMPImageDecoderTest, VerifyBMPSuiteImage) {
  // Load the BMP file under test.
  const auto& [entry_dir, entry_bmp] = GetParam();
  std::string bmp_path = base::StringPrintf(
      "/images/bmp-suite/%s/%s.bmp", entry_dir.c_str(), entry_bmp.c_str());
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(bmp_path.c_str());
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
    EXPECT_NE(entry_dir, "good");
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
      base::StringPrintf("%s_%s.rev0", entry_dir.c_str(), entry_bmp.c_str()));
  EXPECT_TRUE(skia_gold->CompareScreenshot(golden_name, *result_image,
                                           &positive_if_exact_image_only))
      << bmp_path;
#endif
}

INSTANTIATE_TEST_SUITE_P(
    BMPSuite,
    BMPImageDecoderTest,
    testing::Values(
        BMPSuiteEntry{"good", "pal1"},
        BMPSuiteEntry{"good", "pal1wb"},
        BMPSuiteEntry{"good", "pal1bg"},
        BMPSuiteEntry{"good", "pal4"},
        BMPSuiteEntry{"good", "pal4gs"},
        BMPSuiteEntry{"good", "pal4rle"},
        BMPSuiteEntry{"good", "pal8"},
        BMPSuiteEntry{"good", "pal8-0"},
        BMPSuiteEntry{"good", "pal8gs"},
        BMPSuiteEntry{"good", "pal8rle"},
        BMPSuiteEntry{"good", "pal8w126"},
        BMPSuiteEntry{"good", "pal8w125"},
        BMPSuiteEntry{"good", "pal8w124"},
        BMPSuiteEntry{"good", "pal8topdown"},
        BMPSuiteEntry{"good", "pal8nonsquare"},
        BMPSuiteEntry{"good", "pal8os2"},
        BMPSuiteEntry{"good", "pal8v4"},
        BMPSuiteEntry{"good", "pal8v5"},
        BMPSuiteEntry{"good", "rgb16"},
        BMPSuiteEntry{"good", "rgb16bfdef"},
        BMPSuiteEntry{"good", "rgb16-565"},
        BMPSuiteEntry{"good", "rgb16-565pal"},
        BMPSuiteEntry{"good", "rgb24"},
        BMPSuiteEntry{"good", "rgb24pal"},
        BMPSuiteEntry{"good", "rgb32"},
        BMPSuiteEntry{"good", "rgb32bfdef"},
        BMPSuiteEntry{"good", "rgb32bf"},

        BMPSuiteEntry{"questionable", "pal1p1"},
        BMPSuiteEntry{"questionable", "pal2"},
        BMPSuiteEntry{"questionable", "pal2color"},
        BMPSuiteEntry{"questionable", "pal4rletrns"},
        BMPSuiteEntry{"questionable", "pal4rlecut"},
        BMPSuiteEntry{"questionable", "pal8rletrns"},
        BMPSuiteEntry{"questionable", "pal8rlecut"},
        BMPSuiteEntry{"questionable", "pal8offs"},
        BMPSuiteEntry{"questionable", "pal8oversizepal"},
        BMPSuiteEntry{"questionable", "pal8os2-sz"},
        BMPSuiteEntry{"questionable", "pal8os2-hs"},
        BMPSuiteEntry{"questionable", "pal8os2sp"},
        BMPSuiteEntry{"questionable", "pal8os2v2"},
        BMPSuiteEntry{"questionable", "pal8os2v2-16"},
        BMPSuiteEntry{"questionable", "pal8os2v2-sz"},
        BMPSuiteEntry{"questionable", "pal8os2v2-40sz"},
        BMPSuiteEntry{"questionable", "rgb24rle24"},
        BMPSuiteEntry{"questionable", "pal1huffmsb"},  // Unsupported encoding.
        BMPSuiteEntry{"questionable", "rgb16faketrns"},
        BMPSuiteEntry{"questionable", "rgb16-231"},
        BMPSuiteEntry{"questionable", "rgb16-3103"},
        BMPSuiteEntry{"questionable", "rgba16-4444"},
        BMPSuiteEntry{"questionable", "rgba16-5551"},
        BMPSuiteEntry{"questionable", "rgba16-1924"},
        BMPSuiteEntry{"questionable", "rgb24largepal"},
        //           {"questionable", "rgb24prof"},  Omitted--not public domain.
        //           {"questionable", "rgb24prof2"},    "       "    "      "
        //           {"questionable", "rgb24lprof"},    "       "    "      "
        BMPSuiteEntry{"questionable", "rgb24jpeg"},
        BMPSuiteEntry{"questionable", "rgb24png"},
        BMPSuiteEntry{"questionable", "rgb32h52"},
        BMPSuiteEntry{"questionable", "rgb32-xbgr"},
        BMPSuiteEntry{"questionable", "rgb32fakealpha"},
        BMPSuiteEntry{"questionable", "rgb32-111110"},
        BMPSuiteEntry{"questionable", "rgb32-7187"},
        BMPSuiteEntry{"questionable", "rgba32-1"},
        BMPSuiteEntry{"questionable", "rgba32-1010102"},
        BMPSuiteEntry{"questionable", "rgba32-81284"},
        BMPSuiteEntry{"questionable", "rgba32-61754"},
        BMPSuiteEntry{"questionable", "rgba32abf"},
        BMPSuiteEntry{"questionable", "rgba32h56"},
        // TODO: crbug.com/40244265 - a bitcount of 64 is not yet supported.
        BMPSuiteEntry{"questionable", "rgba64"},

        BMPSuiteEntry{"bad", "badbitcount"},
        BMPSuiteEntry{"bad", "badbitssize"},
        BMPSuiteEntry{"bad", "baddens1"},
        BMPSuiteEntry{"bad", "baddens2"},
        BMPSuiteEntry{"bad", "badfilesize"},
        BMPSuiteEntry{"bad", "badheadersize"},
        BMPSuiteEntry{"bad", "badpalettesize"},
        BMPSuiteEntry{"bad", "badplanes"},
        BMPSuiteEntry{"bad", "badrle"},
        BMPSuiteEntry{"bad", "badrle4"},
        BMPSuiteEntry{"bad", "badrle4bis"},
        BMPSuiteEntry{"bad", "badrle4ter"},
        BMPSuiteEntry{"bad", "badrlebis"},
        BMPSuiteEntry{"bad", "badrleter"},
        BMPSuiteEntry{"bad", "badwidth"},
        BMPSuiteEntry{"bad", "pal8badindex"},
        BMPSuiteEntry{"bad", "reallybig"},
        BMPSuiteEntry{"bad", "rgb16-880"},
        BMPSuiteEntry{"bad", "rletopdown"},
        BMPSuiteEntry{"bad", "shortfile"}));

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
