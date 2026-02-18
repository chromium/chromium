// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/containers/span.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_decoder_factory.h"
#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_features.h"
#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_rust_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_base_test.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
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
  return CreateBmpImageDecoder(
      ImageDecoder::kAlphaNotPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::kTransformToSRGB, ImageDecoder::kNoDecodedImageByteLimit);
}

enum class RustFeatureState { kRustEnabled, kRustDisabled };

class BMPImageDecoderTest : public testing::TestWithParam<RustFeatureState> {
 public:
  BMPImageDecoderTest() {
    switch (GetParam()) {
      case RustFeatureState::kRustEnabled:
        features_.InitAndEnableFeature(kRustyBmpFeature);
        break;
      case RustFeatureState::kRustDisabled:
        features_.InitAndDisableFeature(kRustyBmpFeature);
        break;
    }
  }

 protected:
  base::test::ScopedFeatureList features_;
};

}  // anonymous namespace

TEST_P(BMPImageDecoderTest, isSizeAvailable) {
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

TEST_P(BMPImageDecoderTest, parseAndDecode) {
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
TEST_P(BMPImageDecoderTest, emptyImage) {
  static constexpr char kBmpFile[] = "/images/resources/0x0.bmp";  // 0x0
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);

  // SkBmpRustCodec decoder returns decoder error with no frame created for
  // empty image.
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  if (GetParam() == RustFeatureState::kRustDisabled) {
    ASSERT_TRUE(frame);
    EXPECT_EQ(ImageFrame::kFrameEmpty, frame->GetStatus());
  } else {
    ASSERT_FALSE(frame);
  }

  EXPECT_TRUE(decoder->Failed());
}

TEST_P(BMPImageDecoderTest, int32MinHeight) {
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
TEST_P(BMPImageDecoderTest, crbug752898) {
  static constexpr char kBmpFile[] = "/images/resources/crbug752898.bmp";
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  decoder->DecodeFrameBufferAtIndex(0);
}

// Verify that decoding this image does not crash.
TEST_P(BMPImageDecoderTest, invalidBitmapOffset) {
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
TEST_P(BMPImageDecoderTest, allowEOFWhenPastEndOfImage) {
  static constexpr char kBmpFile[] = "/images/resources/unnecessary-eof.bmp";
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

class BMPSuiteEntry {
 public:
  // `entry_dir` and `entry_bmp` are primarily used to locate the input test
  // file (and also to construct Skia Gold test name) - the file will be read
  // from:
  // `third_party/blink/web_tests/images/bmp-suite/<entry_dir>/<entry_bmp>.bmp`
  //
  // `revision` is a Skia Gold revision number, which needs to be increased
  // every time test expectations change - see also documentation of
  // `PositiveIfOnlyImageAlgorithm` used by `BMPImageDecoderTest`:
  // https://source.chromium.org/chromium/chromium/src/+/main:ui/base/test/skia_gold_matching_algorithm.h;l=97-133;drc=31a129ff9b513950f7f96f7fba885e8341f52158
  BMPSuiteEntry(std::string entry_dir,
                std::string entry_bmp,
                std::string revision = "rev0")
      : entry_dir_(std::move(entry_dir)),
        entry_bmp_(std::move(entry_bmp)),
        revision_(std::move(revision)) {}

  const std::string& entry_dir() const { return entry_dir_; }
  const std::string& entry_bmp() const { return entry_bmp_; }
  const std::string& revision() const { return revision_; }

 private:
  std::string entry_dir_;
  std::string entry_bmp_;
  std::string revision_;
};

class BMPImageDecoderSuiteTest : public testing::TestWithParam<BMPSuiteEntry> {
 public:
  BMPImageDecoderSuiteTest() {
    scoped_feature_list_.InitAndDisableFeature(kRustyBmpFeature);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_LINUX)
#define MAYBE_VerifyBMPSuiteImage DISABLED_VerifyBMPSuiteImage
#else
#define MAYBE_VerifyBMPSuiteImage VerifyBMPSuiteImage
#endif
// TODO crbug.com/422362214): Re-enable once flakiness is addressed.
TEST_P(BMPImageDecoderSuiteTest, MAYBE_VerifyBMPSuiteImage) {
  // Load the BMP file under test.
  const BMPSuiteEntry& entry = GetParam();
  std::string bmp_path =
      base::StringPrintf("/images/bmp-suite/%s/%s.bmp",
                         entry.entry_dir().c_str(), entry.entry_bmp().c_str());
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
  if (frame && frame->GetStatus() == ImageFrame::kFrameComplete) {
    EXPECT_FALSE(decoder->Failed());
    result_image = &frame->Bitmap();
  } else {
    // Images in the "good" directory should always decode successfully.
    EXPECT_NE(entry.entry_dir(), "good");
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
      base::StringPrintf("%s_%s.%s", entry.entry_dir().c_str(),
                         entry.entry_bmp().c_str(), entry.revision().c_str()));
  EXPECT_TRUE(skia_gold->CompareScreenshot(golden_name, *result_image,
                                           &positive_if_exact_image_only))
      << bmp_path;
#endif
}

INSTANTIATE_TEST_SUITE_P(All,
                         BMPImageDecoderTest,
                         testing::Values(RustFeatureState::kRustEnabled,
                                         RustFeatureState::kRustDisabled));

INSTANTIATE_TEST_SUITE_P(
    BMPSuite,
    BMPImageDecoderSuiteTest,
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
        //           {"questionable", "rgb24prof"},  Omitted--not public
        //           domain.
        //           {"questionable", "rgb24prof2"},    "       "    " "
        //           {"questionable", "rgb24lprof"},    "       "    " "
        BMPSuiteEntry{"questionable", "rgb24jpeg", "rev1"},
        BMPSuiteEntry{"questionable", "rgb24png", "rev1"},
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
        BMPSuiteEntry{"bad", "rletopdown"},
        BMPSuiteEntry{"bad", "shortfile"}));

// Describes the type of expected difference between decoders
enum class DifferenceType {
  kNone,      // Decoders should produce identical results
  kPixelData  // Both decode successfully but produce different pixels
};

// Describes an expected difference between BMPImageDecoder and
// BmpRustImageDecoder
struct ExpectedDifference {
  DifferenceType type;
  std::string description;

  ExpectedDifference() : type(DifferenceType::kNone), description("") {}
  ExpectedDifference(DifferenceType t, std::string desc)
      : type(t), description(std::move(desc)) {}

  bool HasDifference() const { return type != DifferenceType::kNone; }
  bool AllowsPixelDifference() const {
    return type == DifferenceType::kPixelData;
  }
};

// Test class that compares BMPImageDecoder vs BmpRustImageDecoder
// to verify they produce identical results for all BMP suite files.
//
// Test parameter is a tuple of:
// - BMPSuiteEntry: identifies the test file
// - ExpectedDifference: documents the type and description of expected
// differences
class BMPDecoderComparisonTest
    : public testing::TestWithParam<
          std::tuple<BMPSuiteEntry, ExpectedDifference>> {};

TEST_P(BMPDecoderComparisonTest, CompareDecoders) {
  const auto& [entry, expected_diff] = GetParam();
  std::string bmp_path =
      base::StringPrintf("/images/bmp-suite/%s/%s.bmp",
                         entry.entry_dir().c_str(), entry.entry_bmp().c_str());

  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(bmp_path.c_str());
  ASSERT_NE(data.get(), nullptr) << "Unable to load: " << bmp_path;
  ASSERT_FALSE(data->empty());

  // If there's an expected difference, log it for documentation
  if (expected_diff.HasDifference()) {
    SCOPED_TRACE("Expected difference (pixel): " + expected_diff.description);
  }

  // Create BMPImageDecoder
  auto cpp_decoder = std::make_unique<BMPImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::kTransformToSRGB,
      ImageDecoder::kNoDecodedImageByteLimit);

  // Create BmpRustImageDecoder
  auto rust_decoder = std::make_unique<BmpRustImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::kTransformToSRGB,
      ImageDecoder::kNoDecodedImageByteLimit);

  // Set data for both decoders
  cpp_decoder->SetData(data.get(), true);
  rust_decoder->SetData(data.get(), true);

  // Check IsSizeAvailable
  bool cpp_size_available = cpp_decoder->IsSizeAvailable();
  bool rust_size_available = rust_decoder->IsSizeAvailable();

  // Check Failed state
  bool cpp_failed = cpp_decoder->Failed();
  bool rust_failed = rust_decoder->Failed();

  // Compare size availability
  EXPECT_EQ(cpp_size_available, rust_size_available)
      << bmp_path << ": IsSizeAvailable mismatch"
      << " (BMPImageDecoder: " << cpp_size_available
      << ", BmpRustImageDecoder: " << rust_size_available << ")";

  // If either failed to get size, stop comparison here
  if (!cpp_size_available || !rust_size_available) {
    return;
  }

  // Compare dimensions
  EXPECT_EQ(cpp_decoder->Size().width(), rust_decoder->Size().width())
      << bmp_path << ": Width mismatch";
  EXPECT_EQ(cpp_decoder->Size().height(), rust_decoder->Size().height())
      << bmp_path << ": Height mismatch";

  // Try to decode frame 0
  ImageFrame* cpp_frame = cpp_decoder->DecodeFrameBufferAtIndex(0);
  ImageFrame* rust_frame = rust_decoder->DecodeFrameBufferAtIndex(0);

  // Check failure state after decode attempt
  cpp_failed = cpp_decoder->Failed();
  rust_failed = rust_decoder->Failed();

  // Compare failure states
  EXPECT_EQ(cpp_failed, rust_failed)
      << bmp_path << ": Failed state mismatch after decode"
      << " (BMPImageDecoder: " << cpp_failed
      << ", BmpRustImageDecoder: " << rust_failed << ")";

  // If either decoder failed, we can't compare pixels
  if (cpp_failed || rust_failed) {
    return;
  }

  // Check frame validity
  if (!cpp_frame || !rust_frame) {
    EXPECT_EQ(cpp_frame == nullptr, rust_frame == nullptr)
        << bmp_path << ": Frame null mismatch";
    return;
  }

  // Compare frame status
  EXPECT_EQ(cpp_frame->GetStatus(), rust_frame->GetStatus())
      << bmp_path << ": Frame status mismatch";

  // If frames are not complete, skip pixel comparison
  if (cpp_frame->GetStatus() != ImageFrame::kFrameComplete ||
      rust_frame->GetStatus() != ImageFrame::kFrameComplete) {
    return;
  }

  // Compare bitmap dimensions - strict unless pixel difference is expected
  const SkBitmap& cpp_bitmap = cpp_frame->Bitmap();
  const SkBitmap& rust_bitmap = rust_frame->Bitmap();

  if (!expected_diff.AllowsPixelDifference()) {
    EXPECT_EQ(cpp_bitmap.width(), rust_bitmap.width())
        << bmp_path << ": Bitmap width mismatch";
    EXPECT_EQ(cpp_bitmap.height(), rust_bitmap.height())
        << bmp_path << ": Bitmap height mismatch";
    EXPECT_EQ(cpp_bitmap.colorType(), rust_bitmap.colorType())
        << bmp_path << ": Bitmap color type mismatch";
    EXPECT_EQ(cpp_bitmap.alphaType(), rust_bitmap.alphaType())
        << bmp_path << ": Bitmap alpha type mismatch";
  }

  // Compare pixel data - strict unless pixel difference is expected
  if (!expected_diff.AllowsPixelDifference() &&
      cpp_bitmap.width() == rust_bitmap.width() &&
      cpp_bitmap.height() == rust_bitmap.height() &&
      cpp_bitmap.colorType() == rust_bitmap.colorType()) {
    size_t cpp_size = cpp_bitmap.computeByteSize();
    size_t rust_size = rust_bitmap.computeByteSize();
    EXPECT_EQ(cpp_size, rust_size) << bmp_path << ": Bitmap byte size mismatch";

    if (cpp_size == rust_size && cpp_size > 0) {
      // SAFETY: SkBitmap::getPixels() returns a valid pointer to pixel data
      // with size verified by computeByteSize(). The span is used for safe
      // iteration and bounds-checked access to compare decoder outputs.
      base::span<const uint8_t> cpp_span = UNSAFE_BUFFERS(base::span(
          static_cast<const uint8_t*>(cpp_bitmap.getPixels()), cpp_size));
      // SAFETY: Same as above - getPixels() pointer validated by SkBitmap.
      base::span<const uint8_t> rust_span = UNSAFE_BUFFERS(base::span(
          static_cast<const uint8_t*>(rust_bitmap.getPixels()), cpp_size));

      bool pixels_match = std::equal(cpp_span.begin(), cpp_span.end(),
                                     rust_span.begin(), rust_span.end());
      if (!pixels_match) {
        // Find first differing pixel for debugging
        int first_diff_offset = -1;
        for (size_t i = 0; i < cpp_span.size(); i++) {
          if (cpp_span[i] != rust_span[i]) {
            first_diff_offset = static_cast<int>(i);
            break;
          }
        }

        int bytes_per_pixel = cpp_bitmap.bytesPerPixel();
        int pixel_index = first_diff_offset / bytes_per_pixel;
        int row = pixel_index / cpp_bitmap.width();
        int col = pixel_index % cpp_bitmap.width();

        EXPECT_TRUE(pixels_match)
            << bmp_path << ": Pixel data mismatch. "
            << "First difference at byte " << first_diff_offset
            << " (pixel row=" << row << ", col=" << col << "). "
            << "BMPImageDecoder value: 0x" << std::hex
            << static_cast<int>(cpp_span[first_diff_offset])
            << ", BmpRustImageDecoder value: 0x"
            << static_cast<int>(rust_span[first_diff_offset]);
      }
    }
  }
}

// Helper to generate readable test names - sanitizes invalid characters
std::string BMPComparisonTestName(
    const testing::TestParamInfo<std::tuple<BMPSuiteEntry, ExpectedDifference>>&
        info) {
  const auto& [entry, expected_diff] = info.param;
  std::string name = entry.entry_dir() + "_" + entry.entry_bmp();
  // Replace invalid characters (dashes, etc.) with underscores
  for (char& c : name) {
    if (!IsASCIIAlphanumeric(c) && c != '_') {
      c = '_';
    }
  }
  return name;
}

// Helper functions for creating test entries
auto NoDifference() {
  return ExpectedDifference();
}

auto PixelDiff(std::string desc) {
  return ExpectedDifference(DifferenceType::kPixelData, std::move(desc));
}

INSTANTIATE_TEST_SUITE_P(
    BMPSuiteComparison,
    BMPDecoderComparisonTest,
    testing::Values(
        // Good files - all should decode identically
        std::make_tuple(BMPSuiteEntry{"good", "pal1"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal1wb"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal1bg"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal4"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal4gs"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal4rle"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8-0"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8gs"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8rle"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8w126"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8w125"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8w124"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8topdown"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8nonsquare"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "pal8os2"}, NoDifference()),
        std::make_tuple(
            BMPSuiteEntry{"good", "pal8v4"},
            PixelDiff("BMPv4 colorimetry: BMPImageDecoder applies color space "
                      "transformation, BmpRustImageDecoder doesn't yet. "
                      "Fix: https://github.com/image-rs/image/pull/2771")),
        std::make_tuple(BMPSuiteEntry{"good", "pal8v5"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "rgb16"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "rgb16bfdef"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "rgb16-565"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "rgb16-565pal"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "rgb24"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "rgb24pal"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "rgb32"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "rgb32bfdef"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"good", "rgb32bf"}, NoDifference()),

        // Questionable files - behavior may differ
        std::make_tuple(BMPSuiteEntry{"questionable", "pal1p1"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal2"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal2color"},
                        NoDifference()),
        std::make_tuple(
            BMPSuiteEntry{"questionable", "pal4rletrns"},
            PixelDiff("RLE transparency: different alpha handling for pixels "
                      "skipped by delta/EOL/EOF commands. "
                      "Fix: https://github.com/image-rs/image/pull/2773")),
        std::make_tuple(
            BMPSuiteEntry{"questionable", "pal4rlecut"},
            PixelDiff("Truncated RLE4: different alpha handling for pixels "
                      "skipped by premature end of RLE stream. "
                      "Fix: https://github.com/image-rs/image/pull/2773")),
        std::make_tuple(
            BMPSuiteEntry{"questionable", "pal8rletrns"},
            PixelDiff("RLE transparency: different alpha handling for pixels "
                      "skipped by delta/EOL/EOF commands. "
                      "Fix: https://github.com/image-rs/image/pull/2773")),
        std::make_tuple(
            BMPSuiteEntry{"questionable", "pal8rlecut"},
            PixelDiff("Truncated RLE8: different alpha handling for pixels "
                      "skipped by premature end of RLE stream. "
                      "Fix: https://github.com/image-rs/image/pull/2773")),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal8offs"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal8oversizepal"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal8os2-sz"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal8os2-hs"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal8os2sp"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal8os2v2"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal8os2v2-16"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal8os2v2-sz"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal8os2v2-40sz"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb24rle24"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "pal1huffmsb"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb16faketrns"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb16-231"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb16-3103"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba16-4444"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba16-5551"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba16-1924"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb24largepal"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb24jpeg"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb24png"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb32h52"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb32-xbgr"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb32fakealpha"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb32-111110"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgb32-7187"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba32-1"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba32-1010102"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba32-81284"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba32-61754"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba32abf"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba32h56"},
                        NoDifference()),
        std::make_tuple(BMPSuiteEntry{"questionable", "rgba64"},
                        NoDifference()),

        // Bad files - both should reject
        std::make_tuple(BMPSuiteEntry{"bad", "badbitcount"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badbitssize"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "baddens1"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "baddens2"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badfilesize"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badheadersize"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badpalettesize"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badplanes"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badrle"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badrle4"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badrle4bis"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badrle4ter"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badrlebis"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badrleter"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "badwidth"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "pal8badindex"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "reallybig"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "rgb16-880"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "rletopdown"}, NoDifference()),
        std::make_tuple(BMPSuiteEntry{"bad", "shortfile"}, NoDifference())),
    BMPComparisonTestName);

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
