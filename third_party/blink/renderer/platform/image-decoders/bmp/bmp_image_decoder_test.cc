// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_base_test.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

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
    const char* png;
  };
  static constexpr BMPSuiteEntry kBMPSuiteEntries[] = {
      {"good", "pal1", "pal1"},
      {"good", "pal1wb", "pal1"},
      {"good", "pal1bg", "pal1bg"},
      {"good", "pal4", "pal4"},
      {"good", "pal4gs", "pal4gs"},
      {"good", "pal4rle", "pal4"},
      {"good", "pal8", "pal8"},
      {"good", "pal8-0", "pal8"},
      {"good", "pal8gs", "pal8gs"},
      {"good", "pal8rle", "pal8"},
      {"good", "pal8w126", "pal8w126"},
      {"good", "pal8w125", "pal8w125"},
      {"good", "pal8w124", "pal8w124"},
      {"good", "pal8topdown", "pal8"},
      {"good", "pal8nonsquare", "pal8nonsquare-e"},
      {"good", "pal8os2", "pal8"},
      {"good", "pal8v5", "pal8"},
      {"good", "rgb16", "rgb16"},
      {"good", "rgb16bfdef", "rgb16"},
      {"good", "rgb16-565", "rgb16-565"},
      {"good", "rgb16-565pal", "rgb16-565"},
      {"good", "rgb24", "rgb24"},
      {"good", "rgb24pal", "rgb24"},
      {"good", "rgb32", "rgb24"},
      {"good", "rgb32bfdef", "rgb24"},
      {"good", "rgb32bf", "rgb24"},

      // The following "good" image is not included because our decoder puts a
      // slight color tinge in the result. This isn't visible to the naked eye,
      // but it is not pixel-perfect to the reference. Despite being in the
      // "good" category, the test description states "not sure that the gamma
      // and chromaticity values in this file are sensible, because I can’t find
      // any detailed documentation of them" so a slight deviation seems fine.
      // {"good", "pal8v4", "pal8"},

      // Questionable images have a reference; however, the standard is not
      // clear on the expected output, so it may be reasonable for our results
      // to change if the implementation is updated.
      {"questionable", "pal1p1", "pal1p1"},
      {"questionable", "pal2", "pal2"},
      {"questionable", "pal2color", "pal2color"},
      {"questionable", "pal4rletrns", "pal4rletrns"},
      {"questionable", "pal4rlecut", "pal4rlecut"},
      {"questionable", "pal8rletrns", "pal8rletrns"},
      {"questionable", "pal8rlecut", "pal8rlecut"},
      {"questionable", "pal8offs", "pal8"},
      {"questionable", "pal8oversizepal", "pal8"},
      {"questionable", "pal8os2-sz", "pal8"},
      {"questionable", "pal8os2-hs", "pal8"},
      {"questionable", "pal8os2sp", "pal8"},
      {"questionable", "pal8os2v2", "pal8"},
      {"questionable", "pal8os2v2-16", "pal8"},
      {"questionable", "pal8os2v2-sz", "pal8"},
      {"questionable", "pal8os2v2-40sz", "pal8"},
      {"questionable", "rgb24rle24", "pal8"},
      {"questionable", "pal1huffmsb", nullptr},  // We reject this encoding.
      {"questionable", "rgb16faketrns", "rgb16"},
      {"questionable", "rgb16-231", "rgb16-231"},
      {"questionable", "rgb16-3103", nullptr},  // We have a low-bit difference.
      {"questionable", "rgba16-4444", "rgba16-4444"},
      {"questionable", "rgba16-5551", "rgba16-5551"},
      {"questionable", "rgba16-1924", "rgba16-1924"},
      {"questionable", "rgb24largepal", "rgb24"},
      // {"questionable", "rgb24prof", "rgb24"},  // Omitted--not public domain.
      // {"questionable", "rgb24prof2", "rgb24"},  //  "       "    "      "
      // {"questionable", "rgb24lprof", "rgb24"},  //  "       "    "      "
      {"questionable", "rgb24jpeg", nullptr},  // Reference isn't PNG-encoded.
      {"questionable", "rgb24png", "rgb24"},
      {"questionable", "rgb32h52", "rgb24"},
      {"questionable", "rgb32-xbgr", "rgb24"},
      {"questionable", "rgb32fakealpha", "rgb24"},
      {"questionable", "rgb32-111110", nullptr},  // We have a low-bit diff.
      {"questionable", "rgb32-7187", nullptr},    //  "  "   "     "     "
      {"questionable", "rgba32-1", "rgba32"},
      {"questionable", "rgba32-1010102", nullptr},  // We have a low-bit diff.
      {"questionable", "rgba32-81284", "rgba32-81284"},
      {"questionable", "rgba32-61754", nullptr},  // We have a low-bit diff.
      {"questionable", "rgba32abf", "rgba32"},
      {"questionable", "rgba32h56", "rgba32"},
      // {"questionable", "rgba64", "rgba32"},  // We don't support HDR BMPs.

      // Bad images do not have a reference; we just need to verify that we can
      // parse them without crashing.
      {"bad", "badbitcount", nullptr},
      {"bad", "badbitssize", nullptr},
      {"bad", "baddens1", nullptr},
      {"bad", "baddens2", nullptr},
      {"bad", "badfilesize", nullptr},
      {"bad", "badheadersize", nullptr},
      {"bad", "badpalettesize", nullptr},
      {"bad", "badplanes", nullptr},
      {"bad", "badrle", nullptr},
      {"bad", "badrle4", nullptr},
      {"bad", "badrle4bis", nullptr},
      {"bad", "badrle4ter", nullptr},
      {"bad", "badrlebis", nullptr},
      {"bad", "badrleter", nullptr},
      {"bad", "badwidth", nullptr},
      {"bad", "pal8badindex", nullptr},
      {"bad", "reallybig", nullptr},
      {"bad", "rgb16-880", nullptr},
      {"bad", "rletopdown", nullptr},
      {"bad", "shortfile", nullptr},
  };

  for (const BMPSuiteEntry& entry : kBMPSuiteEntries) {
    // Load the BMP file under test.
    std::string bmp_path =
        base::StringPrintf("/images/bmp-suite/%s/%s.bmp", entry.dir, entry.bmp);
    scoped_refptr<SharedBuffer> data = ReadFile(bmp_path.c_str());
    ASSERT_NE(data.get(), nullptr) << "unable to load '" << bmp_path << "'";
    ASSERT_FALSE(data->empty());

    std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
    decoder->SetData(data.get(), /*all_data_received=*/true);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    if (!entry.png) {
      // If there is no reference image, decoding the image without a crash is
      // considered a success.
      continue;
    }

    // Verify that the BMP decoded successfully.
    ASSERT_EQ(ImageFrame::kFrameComplete, frame->GetStatus()) << bmp_path;
    ASSERT_FALSE(decoder->Failed()) << bmp_path;

    // Load the PNG reference for this file.
    std::string png_path =
        base::StringPrintf("/images/bmp-suite/reference/%s.png", entry.png);
    scoped_refptr<SharedBuffer> reference_data = ReadFile(png_path.c_str());
    ASSERT_NE(reference_data.get(), nullptr)
        << "unable to load '" << png_path << "'";
    ASSERT_FALSE(reference_data->empty());

    PNGImageDecoder png_decoder{ImageDecoder::kAlphaNotPremultiplied,
                                ImageDecoder::kDefaultBitDepth,
                                ColorBehavior::kTransformToSRGB,
                                ImageDecoder::kNoDecodedImageByteLimit};
    png_decoder.SetData(reference_data.get(), /*all_data_received=*/true);
    ImageFrame* reference_frame = png_decoder.DecodeFrameBufferAtIndex(0);
    ASSERT_EQ(ImageFrame::kFrameComplete, reference_frame->GetStatus());
    ASSERT_FALSE(png_decoder.Failed());

    // Compare the ImageFrames.
    // TODO: https://crbug.com/1524420 - use Skia Gold for pixel diffing
    ASSERT_EQ(frame->GetPixelFormat(), ImageFrame::kN32);
    ASSERT_EQ(reference_frame->GetPixelFormat(), ImageFrame::kN32);
    ASSERT_EQ(frame->Bitmap().width(), reference_frame->Bitmap().width());
    ASSERT_EQ(frame->Bitmap().height(), reference_frame->Bitmap().height());

    for (int y = frame->Bitmap().height() - 1; y >= 0; --y) {
      for (int x = frame->Bitmap().width() - 1; x >= 0; --x) {
        ImageFrame::PixelData* pixel_rgba = frame->GetAddr(x, y);
        ImageFrame::PixelData* reference_rgba = reference_frame->GetAddr(x, y);
        // If the alpha channel is zero on both sides, the RGB channels don't
        // matter. (Some of the reference images contain colors in zero-alpha
        // positions, but our BMP decoder emits zero across all channels.)
        if (SkGetPackedA32(*pixel_rgba) == 0 &&
            SkGetPackedA32(*reference_rgba) == 0) {
          continue;
        }
        // If the alpha channels are non-zero, the RGB colors must match.
        if (*pixel_rgba != *reference_rgba) {
          ADD_FAILURE() << base::StringPrintf(
              "%s: pixel mismatch at %d, %d - RGBA in reference "
              "[%02X%02X%02X%02X] vs actual [%02X%02X%02X%02X]",
              bmp_path.c_str(), x, y, SkGetPackedR32(*reference_rgba),
              SkGetPackedG32(*reference_rgba), SkGetPackedB32(*reference_rgba),
              SkGetPackedA32(*reference_rgba), SkGetPackedR32(*pixel_rgba),
              SkGetPackedG32(*pixel_rgba), SkGetPackedB32(*pixel_rgba),
              SkGetPackedA32(*pixel_rgba));
          x = y = 0;  // Only report the first pixel mismatch.
        }
      }
    }
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
