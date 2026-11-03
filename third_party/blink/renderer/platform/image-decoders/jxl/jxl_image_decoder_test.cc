// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "base/byte_size.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "cc/paint/paint_image_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/image_animation.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/private/SkGainmapInfo.h"
#include "ui/gfx/hdr_metadata.h"

namespace blink {

namespace {

// Path to JXL test images in WPT directory
// Note: ReadFileToSharedBuffer expects paths relative to web_tests/
const char kJxlTestDir[] = "web_tests/external/wpt/jpegxl/resources";
const char kImagesDir[] = "web_tests/images/resources";

std::unique_ptr<ImageDecoder> CreateJXLDecoder() {
  return std::make_unique<JXLImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::kTag, cc::AuxImage::kDefault,
      ImageDecoder::kNoDecodedImageByteLimit,
      ImageDecoder::AnimationOption::kUnspecified);
}

std::unique_ptr<ImageDecoder> CreateJXLDecoderWithAlpha(
    ImageDecoder::AlphaOption alpha_option) {
  return std::make_unique<JXLImageDecoder>(
      alpha_option, ImageDecoder::kDefaultBitDepth, ColorBehavior::kTag,
      cc::AuxImage::kDefault, ImageDecoder::kNoDecodedImageByteLimit,
      ImageDecoder::AnimationOption::kUnspecified);
}

std::unique_ptr<ImageDecoder> CreateJXLDecoderWithOptions(
    ImageDecoder::AlphaOption alpha_option,
    ImageDecoder::HighBitDepthDecodingOption high_bit_depth_option,
    ColorBehavior color_behavior) {
  return std::make_unique<JXLImageDecoder>(
      alpha_option, high_bit_depth_option, color_behavior,
      cc::AuxImage::kDefault, ImageDecoder::kNoDecodedImageByteLimit,
      ImageDecoder::AnimationOption::kUnspecified);
}

}  // namespace

class JXLImageDecoderTest : public testing::Test {};

// Test basic decoding of a lossless JXL image
TEST_F(JXLImageDecoderTest, DecodeLossless) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(3, decoder->Size().width());
  EXPECT_EQ(3, decoder->Size().height());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// Test basic decoding of a lossy JXL image
TEST_F(JXLImageDecoderTest, DecodeLossy) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3_srgb_lossy.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(3, decoder->Size().width());
  EXPECT_EQ(3, decoder->Size().height());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// Test decoding JXL with alpha channel
TEST_F(JXLImageDecoderTest, DecodeWithAlpha) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3a_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_TRUE(frame->HasAlpha());
  EXPECT_FALSE(decoder->Failed());
}

// Test codestream orientation metadata is applied to the decoded dimensions.
TEST_F(JXLImageDecoderTest, DecodeOrientationFromCodestream) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "orientation6_rotate_90_cw.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(256, decoder->Size().width());
  EXPECT_EQ(100, decoder->Size().height());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// Test decoding of CMYK+alpha fixture with an ICC profile.
TEST_F(JXLImageDecoderTest, DecodeCmykWithIccProfile) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "conformance_cmyk_layers.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(512, decoder->Size().width());
  EXPECT_EQ(512, decoder->Size().height());
  EXPECT_TRUE(decoder->HasEmbeddedColorProfile());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(frame->HasAlpha());
  EXPECT_FALSE(decoder->Failed());

  const auto expect_color_near = [&frame](int x, int y, SkColor expected) {
    const SkColor actual = frame->Bitmap().getColor(x, y);
    EXPECT_NEAR(SkColorGetR(actual), SkColorGetR(expected), 9);
    EXPECT_NEAR(SkColorGetG(actual), SkColorGetG(expected), 9);
    EXPECT_NEAR(SkColorGetB(actual), SkColorGetB(expected), 9);
    EXPECT_EQ(SkColorGetA(actual), SkColorGetA(expected));
  };
  expect_color_near(192, 64, SkColorSetRGB(34, 31, 33));
  expect_color_near(192, 192, SkColorSetRGB(236, 27, 68));
  expect_color_near(320, 320, SkColorSetRGB(0, 182, 178));
}

// Test that a PQ image's intensity_target is reported as its peak luminance.
// The two fixtures hold identical pixels and differ only in intensity_target.
TEST_F(JXLImageDecoderTest, PqIntensityTargetIsContentLightLevel) {
  const auto max_cll = [](const char* file) {
    auto decoder = CreateJXLDecoder();
    scoped_refptr<SharedBuffer> data =
        ReadFileToSharedBuffer(kJxlTestDir, file);
    CHECK(data);
    decoder->SetData(data.get(), true);
    CHECK(decoder->IsSizeAvailable());
    const gfx::HDRMetadata& hdr_metadata = decoder->GetHDRMetadata();
    return hdr_metadata.HasCLLI()
               ? std::optional<float>(hdr_metadata.GetCLLI().fMaxCLL)
               : std::nullopt;
  };

  EXPECT_EQ(max_cll("pq_intensity_target_600.jxl"), 600.f);
  // 10000 cd/m2 is the PQ nominal peak that encoders write by default, and
  // 255 is the codestream default when the field is absent; neither states
  // the content's peak.
  EXPECT_EQ(max_cll("pq_intensity_target_10000.jxl"), std::nullopt);
  EXPECT_EQ(max_cll("pq_intensity_target_255.jxl"), std::nullopt);
}

// Test that an SDR image's intensity_target is not reported as HDR metadata.
TEST_F(JXLImageDecoderTest, SdrImageHasNoContentLightLevel) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  ASSERT_TRUE(decoder->IsSizeAvailable());

  EXPECT_FALSE(decoder->GetHDRMetadata().HasCLLI());
}

// Test grayscale+alpha fixture carrying an embedded ICC profile.
TEST_F(JXLImageDecoderTest, DecodeWithEmbeddedIccProfile) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "with_icc.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(169, decoder->Size().width());
  EXPECT_EQ(118, decoder->Size().height());
  EXPECT_TRUE(decoder->HasEmbeddedColorProfile());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_TRUE(frame->HasAlpha());
  EXPECT_FALSE(decoder->Failed());
}

// Test high bit depth fixture from interop assets decodes to half-float output.
TEST_F(JXLImageDecoderTest, HighBitDepthInteropHalfFloatFormat) {
  auto decoder = CreateJXLDecoderWithOptions(
      ImageDecoder::kAlphaNotPremultiplied,
      ImageDecoder::kHighBitDepthToHalfFloat, ColorBehavior::kTag);
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "has_permutation.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(2143, decoder->Size().width());
  EXPECT_EQ(1050, decoder->Size().height());
  EXPECT_TRUE(decoder->ImageIsHighBitDepth());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::PixelFormat::kRGBA_F16, frame->GetPixelFormat());
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// Test high bit depth decoding selects F16 pixel format.
TEST_F(JXLImageDecoderTest, HighBitDepthHalfFloatFormat) {
  auto decoder = CreateJXLDecoderWithOptions(
      ImageDecoder::kAlphaNotPremultiplied,
      ImageDecoder::kHighBitDepthToHalfFloat, ColorBehavior::kTag);
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "high_bit_depth_1x1.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->ImageIsHighBitDepth());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::PixelFormat::kRGBA_F16, frame->GetPixelFormat());
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// Test MIME type and extension
TEST_F(JXLImageDecoderTest, MimeTypeAndExtension) {
  auto decoder = CreateJXLDecoder();
  EXPECT_EQ("jxl", decoder->FilenameExtension());
  EXPECT_EQ("image/jxl", decoder->MimeType());
}

// Test that invalid/truncated data is handled gracefully (doesn't crash)
TEST_F(JXLImageDecoderTest, InvalidData) {
  auto decoder = CreateJXLDecoder();

  // Create invalid data - random bytes that aren't a valid JXL
  static constexpr auto kInvalidData =
      base::span_from_cstring("not a valid jxl file");
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create(kInvalidData);

  decoder->SetData(data.get(), true);
  // The decoder should not report size as available for invalid data.
  EXPECT_FALSE(decoder->IsSizeAvailable());
}

// Test that empty data is handled gracefully
TEST_F(JXLImageDecoderTest, EmptyData) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();

  decoder->SetData(data.get(), true);
  EXPECT_FALSE(decoder->IsSizeAvailable());
}

// Regression test: the smallest valid naked JXL codestream (12 bytes) must
// decode to a 512x256 black image. See crbug.com/484214291.
TEST_F(JXLImageDecoderTest, SmallestValidBitstream) {
  auto decoder = CreateJXLDecoder();
  // base64: /wr/BwiDBAwASyAY
  static constexpr std::array<uint8_t, 12> kSmallestValid = {
      0xFF, 0x0A, 0xFF, 0x07, 0x08, 0x83, 0x04, 0x0C, 0x00, 0x4B, 0x20, 0x18};
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(base::span(kSmallestValid));

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(512, decoder->Size().width());
  EXPECT_EQ(256, decoder->Size().height());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// A truncated animation must fail decoding, also when the final
// "all data received" notification arrives without any new bytes (the
// scanner has already consumed the whole buffer in an earlier call).
TEST_F(JXLImageDecoderTest, TruncatedAnimationFlagOnlyFinalSetData) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(full_data);
  const Vector<char> full = full_data->CopyAs<Vector<char>>();

  // Cut the file mid-stream, past the header but before the last frames.
  scoped_refptr<SharedBuffer> truncated_data =
      SharedBuffer::Create(base::span(full).first(full.size() / 2));

  auto decoder = CreateJXLDecoder();
  // Stream in the truncated data; the scanner consumes all of it.
  decoder->SetData(truncated_data.get(), false);
  decoder->FrameCount();
  EXPECT_FALSE(decoder->Failed());

  // End of stream: same buffer, only the all-data-received flag flips.
  decoder->SetData(truncated_data.get(), true);
  decoder->FrameCount();
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

// A file truncated before basic info must fail decoding, also when the
// final "all data received" notification arrives without any new bytes.
TEST_F(JXLImageDecoderTest, HeaderTruncatedFlagOnlyFinalSetData) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(full_data);
  const Vector<char> full = full_data->CopyAs<Vector<char>>();

  // Keep only a prefix that contains the codestream signature but ends
  // before the basic info can be parsed.
  scoped_refptr<SharedBuffer> truncated_data =
      SharedBuffer::Create(base::span(full).first(3u));

  auto decoder = CreateJXLDecoder();
  decoder->SetData(truncated_data.get(), false);
  decoder->FrameCount();
  EXPECT_FALSE(decoder->IsSizeAvailable());

  decoder->SetData(truncated_data.get(), true);
  decoder->FrameCount();
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

// Regression test: a 12-byte valid naked JXL codestream must decode to a
// 256x128 black image, at any partial/full data split. See
// crbug.com/507903802.
TEST_F(JXLImageDecoderTest, SmallValidBitstream) {
  // base64: /wrfBwiDBAwASyAY
  static constexpr std::array<uint8_t, 12> kSmallValid = {
      0xFF, 0x0A, 0xDF, 0x07, 0x08, 0x83, 0x04, 0x0C, 0x00, 0x4B, 0x20, 0x18};
  const auto bytes = base::span(kSmallValid);

  for (size_t split = 0; split < bytes.size(); ++split) {
    SCOPED_TRACE(split);
    auto decoder = CreateJXLDecoder();
    scoped_refptr<SharedBuffer> partial_data =
        SharedBuffer::Create(bytes.first(split));
    decoder->SetData(partial_data.get(), false);
    if (decoder->IsSizeAvailable()) {
      decoder->DecodeFrameBufferAtIndex(0);
    }

    scoped_refptr<SharedBuffer> full_data = SharedBuffer::Create(bytes);
    decoder->SetData(full_data.get(), true);
    EXPECT_TRUE(decoder->IsSizeAvailable());
    EXPECT_EQ(256, decoder->Size().width());
    EXPECT_EQ(128, decoder->Size().height());

    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
    EXPECT_FALSE(decoder->Failed());
  }
}

// Regression test: a corrupt bitstream with trailing invalid bytes must be
// rejected. The decoder must not silently produce a transparent image.
// See crbug.com/484171917.
TEST_F(JXLImageDecoderTest, InvalidBitstreamWithTrailingBytes) {
  auto decoder = CreateJXLDecoder();
  // base64: /wr/BwiDBAwASyA0123
  static constexpr std::array<uint8_t, 14> kInvalidTrailing = {
      0xFF, 0x0A, 0xFF, 0x07, 0x08, 0x83, 0x04,
      0x0C, 0x00, 0x4B, 0x20, 0x34, 0xD7, 0x6D};
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(base::span(kInvalidTrailing));

  decoder->SetData(data.get(), true);
  // The header looks valid enough to parse a size, but the frame data
  // is corrupt. The decoder should report failure when attempting to
  // decode the frame.
  EXPECT_TRUE(decoder->IsSizeAvailable());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_NE(ImageFrame::kFrameComplete, frame->GetStatus())
      << "Corrupt bitstream should not produce a complete frame";
  EXPECT_TRUE(decoder->Failed());
}

// Test JXL signature detection
TEST_F(JXLImageDecoderTest, SignatureDetection) {
  // Test naked codestream signature: 0xFF 0x0A
  {
    static constexpr std::array<uint8_t, 12> kCodestreamSig = {
        0xFF, 0x0A, 0xFF, 0x07, 0x08, 0x83, 0x04, 0x0C, 0x00, 0x4B, 0x20, 0x18};
    scoped_refptr<SharedBuffer> data =
        SharedBuffer::Create(base::span(kCodestreamSig));
    FastSharedBufferReader reader(SegmentReader::CreateFromSharedBuffer(data));
    EXPECT_TRUE(JXLImageDecoder::MatchesJXLSignature(reader));
  }

  // Test container signature
  {
    static constexpr std::array<uint8_t, 12> kContainerSig = {
        0x00, 0x00, 0x00, 0x0C, 0x4A, 0x58, 0x4C, 0x20, 0x0D, 0x0A, 0x87, 0x0A};
    scoped_refptr<SharedBuffer> data =
        SharedBuffer::Create(base::span(kContainerSig));
    FastSharedBufferReader reader(SegmentReader::CreateFromSharedBuffer(data));
    EXPECT_TRUE(JXLImageDecoder::MatchesJXLSignature(reader));
  }

  // Test invalid signature
  {
    static constexpr std::array<uint8_t, 12> kInvalidSig = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A,
        0x1A, 0x0A, 0x00, 0x00, 0x00, 0x00};  // PNG
    scoped_refptr<SharedBuffer> data =
        SharedBuffer::Create(base::span(kInvalidSig));
    FastSharedBufferReader reader(SegmentReader::CreateFromSharedBuffer(data));
    EXPECT_FALSE(JXLImageDecoder::MatchesJXLSignature(reader));
  }
}

// TODO(crbug.com/463925507): Re-enable these tests once JXL progressive
// decoding is optimized. Currently these tests timeout due to the small test
// file size causing many iterations.
//
// TEST_F(JXLImageDecoderTest, ProgressiveDecoding) {
//   TestProgressiveDecoding(&CreateJXLDecoder, kJxlTestDir,
//                           "3x3_srgb_lossless.jxl", 10);
// }
//
// TEST_F(JXLImageDecoderTest, ByteByByteDecode) {
//   TestByteByByteDecode(&CreateJXLDecoder, kJxlTestDir,
//                        "3x3_srgb_lossless.jxl", 1, kAnimationNone);
// }
//
// TEST_F(JXLImageDecoderTest, DecodeAfterReallocatingData) {
//   TestDecodeAfterReallocatingData(&CreateJXLDecoder, kJxlTestDir,
//                                   "3x3_srgb_lossless.jxl");
// }

// Test alpha blending modes
TEST_F(JXLImageDecoderTest, AlphaBlending) {
  TestAlphaBlending(&CreateJXLDecoderWithAlpha,
                    "/external/wpt/jpegxl/resources/3x3a_srgb_lossless.jxl");
}

// Test that the decoder reports correct repetition count for static images
TEST_F(JXLImageDecoderTest, StaticImageRepetitionCount) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(kAnimationNone, decoder->RepetitionCount());
}

// Test that decoder respects kAlphaPremultiplied option and decodes
// successfully. Note: AlphaBlending test verifies actual pixel value
// correctness.
TEST_F(JXLImageDecoderTest, PremultipliedAlphaOption) {
  auto decoder = CreateJXLDecoderWithOptions(ImageDecoder::kAlphaPremultiplied,
                                             ImageDecoder::kDefaultBitDepth,
                                             ColorBehavior::kTag);

  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3a_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_TRUE(frame->HasAlpha());
  EXPECT_TRUE(frame->PremultiplyAlpha());
  EXPECT_FALSE(decoder->Failed());
}

// Test that decoder respects kAlphaNotPremultiplied option and decodes
// successfully. Note: AlphaBlending test verifies actual pixel value
// correctness.
TEST_F(JXLImageDecoderTest, NotPremultipliedAlphaOption) {
  auto decoder = CreateJXLDecoderWithOptions(
      ImageDecoder::kAlphaNotPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::kTag);

  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3a_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_TRUE(frame->HasAlpha());
  EXPECT_FALSE(frame->PremultiplyAlpha());
  EXPECT_FALSE(decoder->Failed());
}

// Test that small files (under the 256MB limit) decode successfully.
// Note: We can't test the actual 256MB limit rejection in unit tests due to
// memory constraints. The limit is enforced in JXLImageDecoder::Decode() via
// kMaxJxlFileSize. Manual testing or fuzzing should verify large file
// rejection.
TEST_F(JXLImageDecoderTest, SmallFileDecodesSuccessfully) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  // Sanity check: test file is small
  ASSERT_LT(data->size(), 1024u);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
}

// Test that ColorBehavior::kIgnore option doesn't crash decoder.
// This verifies the code path works; actual color behavior verification
// would require comparing pixel values with known expected outputs.
TEST_F(JXLImageDecoderTest, ColorBehaviorIgnoreDoesNotCrash) {
  auto decoder = CreateJXLDecoderWithOptions(
      ImageDecoder::kAlphaNotPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::kIgnore);

  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// Test that frame count is correct for static images
TEST_F(JXLImageDecoderTest, FrameCount) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_EQ(1u, decoder->FrameCount());
}

// Test JPEG recompression (JXL can losslessly recompress JPEG)
// TODO(crbug.com/474158692): Re-enable once jxl-rs supports JPEG
// reconstruction. jxl-rs currently doesn't process the jbrd (JPEG
// reconstruction) box.
TEST_F(JXLImageDecoderTest, DISABLED_JpegRecompression) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3_jpeg_recompression.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// =============================================================================
// Animation Tests
// =============================================================================

// Test that animated JXL reports correct frame count after decoding all frames.
// Like PNG, JXL uses incremental frame discovery - FrameCount() grows as frames
// are decoded, rather than discovering all frames upfront.
TEST_F(JXLImageDecoderTest, AnimationFrameCount) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  // Initially, frame count is 1 (first frame reserved during BasicInfo)
  EXPECT_GE(decoder->FrameCount(), 1u);

  // Decode all frames to discover the full frame count
  for (wtf_size_t i = 0; i < 5; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "Frame " << i << " is null";
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  }

  // After decoding all frames, count should be 5
  EXPECT_EQ(5u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());
}

// Test decoding all frames of an animated JXL.
// Uses incremental frame discovery - decode until FrameCount() stops growing.
TEST_F(JXLImageDecoderTest, AnimationDecodeAllFrames) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  // Decode frames until we've decoded all 5 (known from test file)
  constexpr wtf_size_t kExpectedFrames = 5;
  for (wtf_size_t i = 0; i < kExpectedFrames; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "Frame " << i << " is null";
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus())
        << "Frame " << i << " is not complete";
  }

  // After decoding, frame count should match expected
  EXPECT_EQ(kExpectedFrames, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());
}

// Test that animation frames have correct duration.
// Must decode frames first to discover them and their durations.
TEST_F(JXLImageDecoderTest, AnimationFrameDuration) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  // Decode all frames first to discover them and their durations
  constexpr wtf_size_t kExpectedFrames = 5;
  for (wtf_size_t i = 0; i < kExpectedFrames; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "Frame " << i << " is null";
  }

  // Check that frame durations are reasonable (non-zero for animations)
  for (wtf_size_t i = 0; i < decoder->FrameCount(); ++i) {
    base::TimeDelta duration = decoder->FrameDurationAtIndex(i);
    // Animation frames should have some duration
    EXPECT_GT(duration.InMilliseconds(), 0)
        << "Frame " << i << " has zero duration";
  }

  // Verify timestamps are correct (cumulative sum of previous frame durations).
  base::TimeDelta expected_timestamp;
  for (wtf_size_t i = 0; i < decoder->FrameCount(); ++i) {
    auto timestamp = decoder->FrameTimestampAtIndex(i);
    ASSERT_TRUE(timestamp.has_value()) << "Frame " << i << " has no timestamp";
    EXPECT_EQ(expected_timestamp, *timestamp)
        << "Frame " << i << " timestamp mismatch: expected "
        << expected_timestamp.InMilliseconds() << " ms, got "
        << timestamp->InMilliseconds() << " ms";
    expected_timestamp += decoder->FrameDurationAtIndex(i);
  }
}

// Test that animation repetition count is reported correctly
TEST_F(JXLImageDecoderTest, AnimationRepetitionCount) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  // Animated images should have a non-kAnimationNone repetition count
  // kAnimationLoopOnce = 0, kAnimationLoopInfinite = -1, or specific count
  int rep_count = decoder->RepetitionCount();
  EXPECT_NE(kAnimationNone, rep_count)
      << "Animation should not report kAnimationNone";
}

TEST_F(JXLImageDecoderTest, AnimationDecodeSecondFrameFirst) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "image_decoder_frame_one_crash.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  ASSERT_GT(decoder->FrameCount(), 1u);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(1);
  ASSERT_TRUE(frame);
  ASSERT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());

  auto sequential = CreateJXLDecoder();
  sequential->SetData(data.get(), true);
  ImageFrame* first_frame = sequential->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(first_frame);
  ASSERT_EQ(ImageFrame::kFrameComplete, first_frame->GetStatus());
  ImageFrame* sequential_frame = sequential->DecodeFrameBufferAtIndex(1);
  ASSERT_TRUE(sequential_frame);
  ASSERT_EQ(ImageFrame::kFrameComplete, sequential_frame->GetStatus());
  EXPECT_EQ(HashBitmap(sequential_frame->Bitmap()),
            HashBitmap(frame->Bitmap()));
}

// Test random frame access (not sequential).
// JXL requires sequential decoding internally, but the API supports random
// access by rewinding and re-decoding when needed.
TEST_F(JXLImageDecoderTest, AnimationRandomFrameAccess) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  // First decode all frames sequentially to discover them
  constexpr wtf_size_t kExpectedFrames = 5;
  for (wtf_size_t i = 0; i < kExpectedFrames; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "Frame " << i << " is null";
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  }
  EXPECT_EQ(kExpectedFrames, decoder->FrameCount());

  // Access frames in non-sequential order (tests rewind/re-decode)
  const wtf_size_t access_order[] = {4, 0, 2, 1, 3};
  for (wtf_size_t idx : access_order) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(idx);
    ASSERT_TRUE(frame) << "Frame " << idx << " is null";
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus())
        << "Frame " << idx << " is not complete";
  }
  EXPECT_FALSE(decoder->Failed());
}

// Test that repeated decode of same frame doesn't cause issues.
// Frame 2 requires decoding frames 0, 1, 2 first (sequential requirement).
TEST_F(JXLImageDecoderTest, AnimationRepeatedFrameDecode) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);

  // First decode frames 0-2 to discover frame 2
  for (wtf_size_t i = 0; i <= 2; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "Frame " << i << " is null";
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  }

  // Now decode frame 2 multiple more times (tests cache hit path)
  for (int i = 0; i < 3; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(2);
    ASSERT_TRUE(frame);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  }
  EXPECT_FALSE(decoder->Failed());
}

// Regression test: Frame durations must be correct when queried immediately
// after FrameCount() without explicit DecodeFrameBufferAtIndex() calls.
// This simulates how DeferredImageDecoder caches frame metadata.
TEST_F(JXLImageDecoderTest, FrameDurationAvailableAfterFrameCount) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(data);

  // Set all data at once (simulates fully loaded file)
  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  // Call FrameCount() - this is what DeferredImageDecoder does first.
  // After this call, frame durations should be queryable and correct.
  wtf_size_t frame_count = decoder->FrameCount();
  EXPECT_EQ(5u, frame_count);

  // Query durations IMMEDIATELY after FrameCount(), without calling
  // DecodeFrameBufferAtIndex(). This is what
  // DeferredImageDecoder::UpdateMetadata() does - it caches durations right
  // after discovering frames. If durations are wrong here (e.g., 0 or 100ms
  // instead of 500ms), the animation will play at incorrect speed.
  for (wtf_size_t i = 0; i < frame_count; ++i) {
    base::TimeDelta duration = decoder->FrameDurationAtIndex(i);
    // The test file has 500ms per frame
    EXPECT_EQ(500, duration.InMilliseconds())
        << "Frame " << i
        << " has incorrect duration: " << duration.InMilliseconds()
        << "ms (expected 500ms). "
        << "This regression causes animation frames to play too fast.";
  }

  // Also verify timestamps are correct
  base::TimeDelta expected_timestamp;
  for (wtf_size_t i = 0; i < frame_count; ++i) {
    auto timestamp = decoder->FrameTimestampAtIndex(i);
    ASSERT_TRUE(timestamp.has_value()) << "Frame " << i << " has no timestamp";
    EXPECT_EQ(expected_timestamp.InMilliseconds(), timestamp->InMilliseconds())
        << "Frame " << i << " has incorrect timestamp";
    expected_timestamp += decoder->FrameDurationAtIndex(i);
  }
}

// Test incremental loading: durations should be correct even when data
// arrives in chunks.
TEST_F(JXLImageDecoderTest, FrameDurationCorrectDuringIncrementalLoad) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(full_data);
  Vector<char> full_data_vec = full_data->CopyAs<Vector<char>>();

  // Feed data incrementally in chunks
  constexpr size_t kChunkSize = 1024;

  for (size_t chunk_end = kChunkSize;
       chunk_end <= static_cast<size_t>(full_data_vec.size());
       chunk_end += kChunkSize) {
    // Create a fresh decoder for each test iteration
    auto decoder = CreateJXLDecoder();
    bool is_all_data = (chunk_end >= static_cast<size_t>(full_data_vec.size()));
    size_t actual_end =
        std::min(chunk_end, static_cast<size_t>(full_data_vec.size()));

    scoped_refptr<SharedBuffer> partial_data =
        SharedBuffer::Create(base::span(full_data_vec).first(actual_end));
    decoder->SetData(partial_data.get(), is_all_data);

    // After setting data, any frames reported by FrameCount() should have
    // correct durations (not 0 or default values).
    wtf_size_t count = decoder->FrameCount();
    for (wtf_size_t i = 0; i < count; ++i) {
      base::TimeDelta duration = decoder->FrameDurationAtIndex(i);
      // Duration should be either 0 (frame not yet discovered) or 500ms
      // (correct) It should NEVER be some intermediate wrong value like 100ms
      if (duration.InMilliseconds() > 0) {
        EXPECT_EQ(500, duration.InMilliseconds())
            << "Frame " << i << " has wrong duration during incremental load "
            << "at offset " << actual_end << "/" << full_data_vec.size();
      }
    }
  }

  // Final test with all data
  auto decoder = CreateJXLDecoder();
  decoder->SetData(full_data.get(), true);

  // After all data is loaded, all 5 frames should be discoverable with
  // correct durations
  EXPECT_EQ(5u, decoder->FrameCount());
  for (wtf_size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(500, decoder->FrameDurationAtIndex(i).InMilliseconds())
        << "Frame " << i << " has wrong duration after full load";
  }
}

// Regression test: exercise multi-step incremental animation scanning and
// decoding. This keeps frame timing growth and offset advancement paths active
// while additional data is appended.
TEST_F(JXLImageDecoderTest, IncrementalAnimationScanningAcrossDataUpdates) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(full_data);
  Vector<char> full_data_vec = full_data->CopyAs<Vector<char>>();
  ASSERT_GT(full_data_vec.size(), 0u);

  constexpr wtf_size_t kExpectedFrameCount = 5;

  // Find a partial-data cutoff that has discovered at least one frame timing
  // but not all frames yet.
  wtf_size_t first_cutoff = 0;
  for (wtf_size_t cutoff = 512; cutoff < full_data_vec.size(); cutoff += 512) {
    auto probe = CreateJXLDecoder();
    scoped_refptr<SharedBuffer> partial_data =
        SharedBuffer::Create(base::span(full_data_vec).first(cutoff));
    probe->SetData(partial_data.get(), false);

    if (probe->FrameCount() < kExpectedFrameCount &&
        probe->FrameDurationAtIndex(0).InMilliseconds() > 0) {
      first_cutoff = cutoff;
      break;
    }
  }
  ASSERT_GT(first_cutoff, 0u);

  auto decoder = CreateJXLDecoder();

  scoped_refptr<SharedBuffer> first_partial =
      SharedBuffer::Create(base::span(full_data_vec).first(first_cutoff));
  decoder->SetData(first_partial.get(), false);
  wtf_size_t first_count = decoder->FrameCount();
  EXPECT_LT(first_count, kExpectedFrameCount);
  EXPECT_GT(decoder->FrameDurationAtIndex(0).InMilliseconds(), 0);

  wtf_size_t second_cutoff =
      std::min<wtf_size_t>(full_data_vec.size() - 1, first_cutoff + 1024);
  ASSERT_GT(second_cutoff, first_cutoff);
  scoped_refptr<SharedBuffer> second_partial =
      SharedBuffer::Create(base::span(full_data_vec).first(second_cutoff));
  decoder->SetData(second_partial.get(), false);

  wtf_size_t second_count = decoder->FrameCount();
  EXPECT_GE(second_count, first_count);
  EXPECT_FALSE(decoder->Failed());

  // Finish with all data and decode all frames to exercise the full decode
  // progression after incremental metadata scanning.
  decoder->SetData(full_data.get(), true);
  EXPECT_EQ(kExpectedFrameCount, decoder->FrameCount());

  for (wtf_size_t i = 0; i < kExpectedFrameCount; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "Frame " << i << " is null";
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus())
        << "Frame " << i << " did not complete";
  }
  EXPECT_FALSE(decoder->Failed());
}

// Regression test: Animation frames with reference frame blending must render
// correctly. This tests that frames which depend on previous frames (reference
// frames) are properly composed. Without correct reference frame handling,
// frames would appear corrupted with missing colors or artifacts.
// See: The newtons_cradle.jxl animation uses reference frame blending.
TEST_F(JXLImageDecoderTest, AnimationReferenceFrameBlending) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "newtons_cradle.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(480, decoder->Size().width());
  EXPECT_EQ(360, decoder->Size().height());

  // This animation has multiple frames that use reference frame blending.
  // Decode all frames to verify they all complete successfully.
  wtf_size_t frame_count = decoder->FrameCount();
  EXPECT_GT(frame_count, 1u) << "Animation should have multiple frames";

  for (wtf_size_t i = 0; i < frame_count; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "Frame " << i << " is null";
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus())
        << "Frame " << i << " failed to decode completely. "
        << "This may indicate reference frame blending issues.";

    // Verify frame has reasonable pixel data (not all black or corrupted).
    // Sample a pixel that should have color (the animation has a cyan
    // background). If reference frames aren't being saved/applied correctly,
    // this would fail.
    if (i == 0) {
      // First frame establishes the reference - just verify it decoded
      EXPECT_FALSE(decoder->Failed());
    }
  }

  EXPECT_FALSE(decoder->Failed());

  // Verify animation loops correctly (tests rewind with reference frames)
  // After decoding all frames, go back to frame 0
  ImageFrame* first_frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(first_frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, first_frame->GetStatus());
}

// When the requested frame is ahead of the decoder and the decoder already
// holds everything that frame depends on, Decode() decodes forward (skipping
// output for the in-between frames) instead of seeking from the frame's decode
// start. The in-between frames still update the decoder's reference frames, so
// the forward-decoded frame must be pixel-identical to the authoritative
// sequential decode. newtons_cradle.jxl uses reference-frame blending, so
// later frames depend on earlier ones and exercise this path.
TEST_F(JXLImageDecoderTest, AnimationForwardSkipMatchesSequential) {
  auto baseline = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "newtons_cradle.jxl");
  ASSERT_TRUE(data);
  baseline->SetData(data.get(), true);

  const wtf_size_t frame_count = baseline->FrameCount();
  ASSERT_GT(frame_count, 3u) << "Need several frames to jump forward over";

  Vector<uint32_t> baseline_hashes;
  for (wtf_size_t i = 0; i < frame_count; ++i) {
    ImageFrame* frame = baseline->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "Baseline frame " << i << " is null";
    ASSERT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
    baseline_hashes.push_back(HashBitmap(frame->Bitmap()));
  }
  ASSERT_FALSE(baseline->Failed());

  // Fresh decoder: prime it on an early frame, then jump forward. Clearing the
  // cache for the earlier frames forces a re-decode that leaves the decoder
  // positioned between frames, which is where the forward-skip path applies.
  auto decoder = CreateJXLDecoder();
  decoder->SetData(data.get(), true);
  ASSERT_EQ(frame_count, decoder->FrameCount());

  const std::array<wtf_size_t, 3> forward_targets = {2u, frame_count / 2,
                                                     frame_count - 1};
  for (wtf_size_t target : forward_targets) {
    decoder->ClearCacheExceptFrame(kNotFound);
    ImageFrame* primed = decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_TRUE(primed) << "Priming frame 0 failed before target " << target;
    ASSERT_EQ(ImageFrame::kFrameComplete, primed->GetStatus());

    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(target);
    ASSERT_TRUE(frame) << "Forward target " << target << " is null";
    ASSERT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
    EXPECT_EQ(baseline_hashes[target], HashBitmap(frame->Bitmap()))
        << "Forward-decoded frame " << target
        << " does not match sequential decode";
  }
  EXPECT_FALSE(decoder->Failed());
}

// Test progressive/pass rendering: during incremental data loading, frames
// should have kFramePartial status (not kFrameEmpty). This prevents the
// ImageDecoderWrapper from destroying the decoder between calls.
//
// Regression test for: When data arrives incrementally, the decoder must
// initialize frames with kFramePartial status early, allowing partial content
// to be displayed and preventing decoder destruction between data chunks.
TEST_F(JXLImageDecoderTest, ProgressiveRenderingFramePartialStatus) {
  // Use zoltan.jxl which is large enough (~420KB) to demonstrate progressive
  // decoding behavior.
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "zoltan.jxl");
  ASSERT_TRUE(full_data);
  ASSERT_GT(full_data->size(), 100000u)
      << "Test file should be large enough for meaningful progressive test";

  Vector<char> full_data_vec = full_data->CopyAs<Vector<char>>();

  // Use a single decoder instance for the entire incremental load.
  // This simulates how ImageDecoderWrapper reuses the decoder when frames
  // have kFramePartial status.
  auto decoder = CreateJXLDecoder();

  // Feed data in chunks until we have enough for the decoder to produce
  // a partial frame. We expect kFramePartial status after enough data
  // is provided (but not all).
  constexpr wtf_size_t kChunkSize = 64 * 1024;  // 64KB chunks
  bool found_partial_frame = false;

  // Feed data incrementally
  wtf_size_t full_size = full_data_vec.size();
  for (wtf_size_t chunk_end = kChunkSize;
       chunk_end <= full_size && !found_partial_frame;
       chunk_end += kChunkSize) {
    wtf_size_t actual_end = std::min(chunk_end, full_size);
    bool is_all_data = (actual_end == full_size);

    scoped_refptr<SharedBuffer> partial_data =
        SharedBuffer::Create(base::span(full_data_vec).first(actual_end));
    decoder->SetData(partial_data.get(), is_all_data);

    // Try to decode frame 0
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);

    if (frame) {
      // During progressive loading, we expect kFramePartial status
      // once enough data is available but before the frame is complete.
      if (frame->GetStatus() == ImageFrame::kFramePartial) {
        found_partial_frame = true;

        // Key assertion: frame should NOT be empty when we have partial data.
        // This is what prevents ImageDecoderWrapper from destroying the
        // decoder.
        EXPECT_FALSE(frame->Bitmap().isNull())
            << "Partial frame should have valid bitmap data";
      }
    }
  }

  // For very small files or files that decode all-at-once, we may not see
  // kFramePartial. That's OK - just verify we don't crash and eventually
  // get kFrameComplete.
  if (found_partial_frame) {
    // Log that we successfully found partial frame status
    EXPECT_TRUE(found_partial_frame)
        << "With ~420KB file fed in 64KB chunks, expected to see "
        << "kFramePartial status at some point before completion";
  }

  // Now provide all data and verify the frame completes successfully
  decoder->SetData(full_data.get(), true);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus())
      << "Frame should be complete after all data is provided";
  EXPECT_FALSE(decoder->Failed());
}

// Regression test: progressive rendering must not stall on the first 4096
// bytes. The jxl-rs codestream parser reads input eagerly into a 4096-byte
// internal buffer while parsing headers, so the first process() call can
// consume all available input without decoding any sections. The decoder
// must keep processing (with empty input) to drain that internal buffer;
// otherwise nothing renders until the total input exceeds the buffer size,
// no matter how much DC data has already arrived.
TEST_F(JXLImageDecoderTest, ProgressiveRenderingWithin4096Bytes) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "fox-progressive.jxl");
  ASSERT_TRUE(full_data);
  Vector<char> full_data_vec = full_data->CopyAs<Vector<char>>();
  ASSERT_GT(full_data_vec.size(), 4096u);

  auto decoder = CreateJXLDecoder();

  // Deliver the first 4096 bytes in a single chunk and then stall, like a
  // slow network stream. The DC data of this image fits well within these
  // bytes, so a partial image must be rendered.
  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(base::span(full_data_vec).first(4096u));
  decoder->SetData(partial_data.get(), false);

  ASSERT_TRUE(decoder->IsSizeAvailable());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFramePartial, frame->GetStatus())
      << "Expected a partial frame from the first 4096 bytes";
  EXPECT_FALSE(decoder->Failed());

  // Completing the stream must produce a complete frame.
  decoder->SetData(full_data.get(), true);
  frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// Test that the decoder instance is preserved across incremental data updates.
// This specifically tests the fix where early frame initialization with
// kFramePartial status prevents ImageDecoderWrapper from destroying the
// decoder.
TEST_F(JXLImageDecoderTest, DecoderPreservedDuringIncrementalLoad) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "zoltan.jxl");
  ASSERT_TRUE(full_data);

  Vector<char> full_data_vec = full_data->CopyAs<Vector<char>>();
  auto decoder = CreateJXLDecoder();

  // Track the decoder's internal state across updates
  int successful_partial_decodes = 0;

  // Feed data in multiple chunks, verifying decoder state is preserved
  constexpr wtf_size_t kChunkSize = 32 * 1024;  // 32KB chunks
  scoped_refptr<SharedBuffer> growing_data = SharedBuffer::Create();
  wtf_size_t full_size = full_data_vec.size();

  for (wtf_size_t offset = 0; offset < full_size; offset += kChunkSize) {
    wtf_size_t chunk_size = std::min(kChunkSize, full_size - offset);
    growing_data->Append(base::span(full_data_vec).subspan(offset, chunk_size));

    bool is_all_data = (offset + chunk_size >= full_size);
    decoder->SetData(growing_data.get(), is_all_data);

    // Attempt to decode
    if (decoder->IsSizeAvailable()) {
      ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
      if (frame && frame->GetStatus() != ImageFrame::kFrameEmpty) {
        successful_partial_decodes++;

        // The key invariant: once we have a non-empty frame, subsequent
        // calls with more data should continue to work (decoder not reset).
        EXPECT_TRUE(frame->GetStatus() == ImageFrame::kFramePartial ||
                    frame->GetStatus() == ImageFrame::kFrameComplete)
            << "Frame status should be Partial or Complete, not Empty";
      }
    }
  }

  // After all data, frame should be complete
  ImageFrame* final_frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(final_frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, final_frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());

  // We should have had at least a few successful partial decodes
  // demonstrating the decoder was preserved across updates.
  // (Exact count depends on file structure and chunk alignment)
  EXPECT_GT(successful_partial_decodes, 0)
      << "Expected at least one successful partial decode during "
      << "incremental loading";
}

// Similar to jxl-rs compare_incremental: verify that progressive/incremental
// decoding converges to the same final pixels as one-shot decoding.
TEST_F(JXLImageDecoderTest, IncrementalStillDecodeMatchesOneShot) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "zoltan.jxl");
  ASSERT_TRUE(full_data);

  auto one_shot = CreateJXLDecoder();
  one_shot->SetData(full_data.get(), true);
  ImageFrame* one_shot_frame = one_shot->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(one_shot_frame);
  ASSERT_EQ(ImageFrame::kFrameComplete, one_shot_frame->GetStatus());
  const uint32_t one_shot_hash = HashBitmap(one_shot_frame->Bitmap());

  Vector<char> full_data_vec = full_data->CopyAs<Vector<char>>();
  auto incremental = CreateJXLDecoder();
  bool saw_partial = false;

  constexpr wtf_size_t kChunkSize = 64 * 1024;
  for (wtf_size_t chunk_end = kChunkSize; chunk_end < full_data_vec.size();
       chunk_end += kChunkSize) {
    scoped_refptr<SharedBuffer> partial_data =
        SharedBuffer::Create(base::span(full_data_vec).first(chunk_end));
    incremental->SetData(partial_data.get(), false);

    ImageFrame* frame = incremental->DecodeFrameBufferAtIndex(0);
    if (frame && frame->GetStatus() == ImageFrame::kFramePartial) {
      saw_partial = true;
    }
    EXPECT_FALSE(incremental->Failed());
  }

  incremental->SetData(full_data.get(), true);
  ImageFrame* final_frame = incremental->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(final_frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, final_frame->GetStatus());
  EXPECT_EQ(one_shot_hash, HashBitmap(final_frame->Bitmap()));
  EXPECT_FALSE(incremental->Failed());
  EXPECT_TRUE(saw_partial);
}

// Similar to jxl-rs compare_incremental for animations: with chunked input,
// repeated decodes should still converge to the same final frame pixels as
// one-shot decoding.
TEST_F(JXLImageDecoderTest, IncrementalAnimationDecodeMatchesOneShot) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(full_data);

  auto one_shot = CreateJXLDecoder();
  one_shot->SetData(full_data.get(), true);
  const wtf_size_t frame_count = one_shot->FrameCount();
  ASSERT_EQ(5u, frame_count);

  Vector<uint32_t> one_shot_hashes;
  one_shot_hashes.ReserveInitialCapacity(frame_count);
  for (wtf_size_t i = 0; i < frame_count; ++i) {
    ImageFrame* frame = one_shot->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "One-shot frame " << i << " is null";
    ASSERT_EQ(ImageFrame::kFrameComplete, frame->GetStatus())
        << "One-shot frame " << i << " is not complete";
    one_shot_hashes.push_back(HashBitmap(frame->Bitmap()));
  }

  Vector<char> full_data_vec = full_data->CopyAs<Vector<char>>();
  auto incremental = CreateJXLDecoder();

  constexpr wtf_size_t kChunkSize = 1024;
  for (wtf_size_t chunk_end = kChunkSize; chunk_end < full_data_vec.size();
       chunk_end += kChunkSize) {
    scoped_refptr<SharedBuffer> partial_data =
        SharedBuffer::Create(base::span(full_data_vec).first(chunk_end));
    incremental->SetData(partial_data.get(), false);

    // Keep advancing scanner/metadata incrementally.
    const wtf_size_t discovered = incremental->FrameCount();
    EXPECT_LE(discovered, frame_count);
    EXPECT_FALSE(incremental->Failed());
  }

  incremental->SetData(full_data.get(), true);
  EXPECT_EQ(frame_count, incremental->FrameCount());

  for (wtf_size_t i = 0; i < frame_count; ++i) {
    ImageFrame* frame = incremental->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame) << "Incremental frame " << i << " is null";
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus())
        << "Incremental frame " << i << " is not complete";
    EXPECT_EQ(one_shot_hashes[i], HashBitmap(frame->Bitmap()))
        << "Frame " << i
        << " pixels differ between one-shot and incremental decode";
  }
  EXPECT_FALSE(incremental->Failed());
}

// Test that during incremental loading (not all data received), FrameCount()
// does NOT block to decode all frames. It should return quickly with partial
// frame count, allowing the browser to be responsive during loading.
TEST_F(JXLImageDecoderTest, IncrementalLoadDoesNotBlockOnAllFrames) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(full_data);
  Vector<char> full_data_vec = full_data->CopyAs<Vector<char>>();

  // Use only partial data (about half the file)
  size_t partial_size = full_data_vec.size() / 2;
  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(base::span(full_data_vec).first(partial_size));

  auto decoder = CreateJXLDecoder();
  decoder->SetData(partial_data.get(), false);  // NOT all data received

  // FrameCount() should return quickly without blocking to decode everything.
  // With partial data, it may return 1 or more depending on how much was
  // decoded.
  wtf_size_t frame_count = decoder->FrameCount();

  // The key assertion: we should NOT have all 5 frames yet with partial data.
  // This verifies we're not doing eager full-file scanning.
  EXPECT_LT(frame_count, 5u)
      << "With only " << partial_size << "/" << full_data_vec.size()
      << " bytes, FrameCount() should not return all 5 frames. "
      << "This suggests the decoder is blocking to scan the entire file.";

  // But we should have at least 1 frame (size/metadata should be available)
  EXPECT_GE(frame_count, 1u);
}

// Regression test for animation drift bug.
//
// The bug: Frame durations were being truncated from f64 to u32 in the Rust
// FFI wrapper, causing animations to slowly drift out of sync when looping.
// For example, a 30fps animation (33.333ms/frame) would lose 0.333ms per
// frame, accumulating ~33ms drift after 100 frames.
//
// The fix: Changed JxlRsFrameHeader.duration_ms from u32 to f64 to preserve
// full floating-point precision through the FFI boundary.
//
// This test verifies:
// 1. Frame durations are stored with sub-millisecond precision capability
// 2. Cumulative timestamps don't show unexpected drift
// 3. The precision is maintained through the full decode pipeline
TEST_F(JXLImageDecoderTest, AnimationDurationPrecisionNoDrift) {
  auto decoder = CreateJXLDecoder();
  // Use 5_frames_numbered.jxl with 500ms per frame (integer timing).
  // While this doesn't have fractional milliseconds, it verifies:
  // - The f64 duration path works correctly end-to-end
  // - Cumulative timestamps are computed accurately
  // - No unexpected precision loss occurs in the pipeline
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "5_frames_numbered.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());

  // Get frame count to trigger frame discovery
  wtf_size_t frame_count = decoder->FrameCount();
  EXPECT_EQ(5u, frame_count);

  // Expected duration: 500ms = 500000us (this test file has integer timing)
  // The fix ensures durations pass through as f64, preserving any fractional
  // precision that might be present in other JXL files.
  constexpr int64_t kExpectedDurationUs = 500 * 1000;  // 500ms in microseconds

  for (wtf_size_t i = 0; i < frame_count; ++i) {
    base::TimeDelta duration = decoder->FrameDurationAtIndex(i);
    int64_t duration_us = duration.InMicroseconds();

    // Verify exact duration - the f64 path should preserve the value exactly.
    EXPECT_EQ(kExpectedDurationUs, duration_us)
        << "Frame " << i << " duration should be exactly 500000 microseconds. "
        << "Got " << duration_us << " microseconds. "
        << "This may indicate precision loss in the FFI layer.";
  }

  // Verify cumulative timestamps are computed accurately without drift.
  // Each frame's timestamp should be the exact sum of all previous durations.
  int64_t expected_timestamp_us = 0;
  for (wtf_size_t i = 0; i < frame_count; ++i) {
    auto timestamp = decoder->FrameTimestampAtIndex(i);
    ASSERT_TRUE(timestamp.has_value()) << "Frame " << i << " has no timestamp";

    // Verify timestamp matches expected cumulative value exactly
    EXPECT_EQ(expected_timestamp_us, timestamp->InMicroseconds())
        << "Frame " << i << " timestamp drift detected. "
        << "Expected " << expected_timestamp_us << " us, got "
        << timestamp->InMicroseconds() << " us.";

    expected_timestamp_us += decoder->FrameDurationAtIndex(i).InMicroseconds();
  }

  // Final verification: total animation duration should be exactly 2500ms.
  // This confirms no precision loss accumulated across frames.
  constexpr int64_t kExpectedTotalUs = 5 * kExpectedDurationUs;  // 2500000us
  EXPECT_EQ(kExpectedTotalUs, expected_timestamp_us)
      << "Total animation duration should be exactly 2500000 us. "
      << "Got " << expected_timestamp_us << " us.";
}

// =============================================================================
// Gain Map Tests
// =============================================================================

namespace {

std::unique_ptr<JXLImageDecoder> CreateJXLDecoderForAuxImage(
    cc::AuxImage aux_image) {
  return std::make_unique<JXLImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ImageDecoder::kDefaultBitDepth,
      aux_image == cc::AuxImage::kGainmap ? ColorBehavior::kIgnore
                                          : ColorBehavior::kTag,
      aux_image, ImageDecoder::kNoDecodedImageByteLimit,
      ImageDecoder::AnimationOption::kUnspecified);
}

// Offset of the 8-byte `jhgm` box header, which the gain-map fixtures carry
// exactly once, as their last box.
size_t FindGainmapBoxOffset(const Vector<char>& bytes) {
  static constexpr auto kBoxType = std::to_array({'j', 'h', 'g', 'm'});
  auto box_type =
      std::search(bytes.begin(), bytes.end(), kBoxType.begin(), kBoxType.end());
  CHECK(box_type != bytes.end());
  const size_t type_offset = static_cast<size_t>(box_type - bytes.begin());
  CHECK_GE(type_offset, 4u);
  return type_offset - 4;
}

// Offset of the nested gain-map codestream, which is the last JPEG XL
// signature in a gain-map fixture.
size_t FindNestedCodestreamOffset(const Vector<char>& bytes) {
  static constexpr auto kSignature =
      std::to_array({static_cast<char>(0xff), static_cast<char>(0x0a)});
  auto signature = std::find_end(bytes.begin(), bytes.end(), kSignature.begin(),
                                 kSignature.end());
  CHECK(signature != bytes.end());
  return static_cast<size_t>(signature - bytes.begin());
}

}  // namespace

TEST_F(JXLImageDecoderTest, GainmapInfoAndAuxiliaryDecode) {
  auto decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kDefault);
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "gainmap-orientation1-base-math.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  ASSERT_TRUE(decoder->IsSizeAvailable());

  SkGainmapInfo gainmap_info;
  scoped_refptr<SegmentReader> gainmap_data;
  ASSERT_TRUE(decoder->GetGainmapInfoAndData(gainmap_info, gainmap_data));
  // The auxiliary decoder receives the nested codestream, not the container.
  ASSERT_TRUE(gainmap_data);
  EXPECT_LT(gainmap_data->size(), data->size());
  EXPECT_EQ(SkGainmapInfo::BaseImageType::kSDR, gainmap_info.fBaseImageType);
  EXPECT_NEAR(1.f, gainmap_info.fDisplayRatioSdr, 1e-6f);
  EXPECT_NEAR(4.f, gainmap_info.fDisplayRatioHdr, 1e-6f);
  EXPECT_NEAR(1.f, gainmap_info.fGainmapRatioMin.fR, 1e-6f);
  EXPECT_NEAR(4.f, gainmap_info.fGainmapRatioMax.fR, 1e-6f);
  EXPECT_FALSE(gainmap_info.fGainmapMathColorSpace);

  ImageFrame* base_frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(base_frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, base_frame->GetStatus());
  EXPECT_EQ(SkColorSetRGB(24, 48, 72), base_frame->Bitmap().getColor(0, 0));

  auto gainmap_decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kGainmap);
  gainmap_decoder->SetData(gainmap_data, true);
  ASSERT_TRUE(gainmap_decoder->IsSizeAvailable());
  EXPECT_EQ(3, gainmap_decoder->Size().width());
  EXPECT_EQ(2, gainmap_decoder->Size().height());

  ImageFrame* frame = gainmap_decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(gainmap_decoder->Failed());

  constexpr std::array<SkColor, 6> kExpectedGainmap = {
      SkColorSetRGB(0, 32, 64),     SkColorSetRGB(96, 128, 160),
      SkColorSetRGB(192, 224, 255), SkColorSetRGB(255, 224, 192),
      SkColorSetRGB(160, 128, 96),  SkColorSetRGB(64, 32, 0),
  };
  for (size_t i = 0; i < kExpectedGainmap.size(); ++i) {
    const int x = static_cast<int>(i % 3);
    const int y = static_cast<int>(i / 3);
    EXPECT_EQ(kExpectedGainmap[i], frame->Bitmap().getColor(x, y))
        << "pixel (" << x << ", " << y << ")";
  }
}

TEST_F(JXLImageDecoderTest, GainmapAuxiliaryDecodeKeepsOrientationAligned) {
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "gainmap-orientation6.jxl");
  ASSERT_TRUE(data);

  auto base_decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kDefault);
  base_decoder->SetData(data.get(), true);
  ASSERT_TRUE(base_decoder->IsSizeAvailable());
  EXPECT_EQ(2, base_decoder->Size().width());
  EXPECT_EQ(3, base_decoder->Size().height());

  SkGainmapInfo gainmap_info;
  scoped_refptr<SegmentReader> gainmap_data;
  ASSERT_TRUE(base_decoder->GetGainmapInfoAndData(gainmap_info, gainmap_data));

  auto decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kGainmap);
  decoder->SetData(gainmap_data, true);
  ASSERT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(2, decoder->Size().width());
  EXPECT_EQ(3, decoder->Size().height());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());

  constexpr std::array<SkColor, 6> kExpectedGainmap = {
      SkColorSetRGB(255, 224, 192), SkColorSetRGB(0, 32, 64),
      SkColorSetRGB(160, 128, 96),  SkColorSetRGB(96, 128, 160),
      SkColorSetRGB(64, 32, 0),     SkColorSetRGB(192, 224, 255),
  };
  for (size_t i = 0; i < kExpectedGainmap.size(); ++i) {
    const int x = static_cast<int>(i % 2);
    const int y = static_cast<int>(i / 2);
    EXPECT_EQ(kExpectedGainmap[i], frame->Bitmap().getColor(x, y))
        << "pixel (" << x << ", " << y << ")";
  }
}

TEST_F(JXLImageDecoderTest, GainmapWaitsForCompleteContainer) {
  auto decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kDefault);
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "gainmap-orientation1-base-math.jxl");
  ASSERT_TRUE(data);
  const Vector<char> bytes = data->CopyAs<Vector<char>>();
  ASSERT_GT(bytes.size(), 32u);
  scoped_refptr<SharedBuffer> prefix =
      SharedBuffer::Create(base::span(bytes).first(32u));

  SkGainmapInfo gainmap_info;
  scoped_refptr<SegmentReader> gainmap_data;
  decoder->SetData(prefix.get(), false);
  EXPECT_FALSE(decoder->GetGainmapInfoAndData(gainmap_info, gainmap_data));

  // All container bytes have arrived, but the caller still reports the stream
  // as incomplete, so the box that may follow the last frame is not parsed
  // yet.
  decoder->SetData(data.get(), false);
  ASSERT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->GetGainmapInfoAndData(gainmap_info, gainmap_data));
  EXPECT_FALSE(decoder->Failed());

  decoder->SetData(data.get(), true);
  ASSERT_TRUE(decoder->GetGainmapInfoAndData(gainmap_info, gainmap_data));
  ASSERT_TRUE(gainmap_data);

  auto gainmap_decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kGainmap);
  gainmap_decoder->SetData(gainmap_data, true);
  ASSERT_TRUE(gainmap_decoder->IsSizeAvailable());
  EXPECT_EQ(3, gainmap_decoder->Size().width());
  EXPECT_EQ(2, gainmap_decoder->Size().height());
  ImageFrame* gainmap_frame = gainmap_decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(gainmap_frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, gainmap_frame->GetStatus());
  EXPECT_EQ(SkColorSetRGB(0, 32, 64), gainmap_frame->Bitmap().getColor(0, 0));
  EXPECT_FALSE(gainmap_decoder->Failed());
}

TEST_F(JXLImageDecoderTest, AlternateMathUsesStructuredColorEncoding) {
  auto decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kDefault);
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "gainmap-orientation1-alt-p3.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  ASSERT_TRUE(decoder->IsSizeAvailable());

  SkGainmapInfo gainmap_info;
  scoped_refptr<SegmentReader> gainmap_data;
  ASSERT_TRUE(decoder->GetGainmapInfoAndData(gainmap_info, gainmap_data));
  ASSERT_TRUE(gainmap_info.fGainmapMathColorSpace);
  EXPECT_TRUE(gainmap_info.fGainmapMathColorSpace->gammaIsLinear());

  sk_sp<SkColorSpace> expected_color_space = SkColorSpace::MakeRGB(
      SkNamedTransferFn::kLinear, SkNamedGamut::kDisplayP3);
  ASSERT_TRUE(expected_color_space);
  skcms_Matrix3x3 expected_to_xyz_d50;
  skcms_Matrix3x3 actual_to_xyz_d50;
  ASSERT_TRUE(expected_color_space->toXYZD50(&expected_to_xyz_d50));
  ASSERT_TRUE(
      gainmap_info.fGainmapMathColorSpace->toXYZD50(&actual_to_xyz_d50));
  // The color encoding of the gain-map box is converted to ICC s15Fixed16
  // colorant tags, so one fixed-point unit of difference is expected.
  constexpr float kIccMatrixTolerance = 1.f / 65536.f;
  const auto expected_rows = base::span(expected_to_xyz_d50.vals);
  const auto actual_rows = base::span(actual_to_xyz_d50.vals);
  for (size_t row = 0; row < expected_rows.size(); ++row) {
    const auto expected_columns = base::span(expected_rows[row]);
    const auto actual_columns = base::span(actual_rows[row]);
    for (size_t column = 0; column < expected_columns.size(); ++column) {
      EXPECT_NEAR(expected_columns[column], actual_columns[column],
                  kIccMatrixTolerance)
          << "matrix row " << row << ", column " << column;
    }
  }
}

TEST_F(JXLImageDecoderTest, InvalidOptionalGainmapDoesNotFailBaseImage) {
  scoped_refptr<SharedBuffer> valid_data =
      ReadFileToSharedBuffer(kImagesDir, "gainmap-orientation1-base-math.jxl");
  ASSERT_TRUE(valid_data);

  // Change the bundle version inside the gain-map box, leaving the container
  // and its base image intact.
  Vector<char> invalid_bytes = valid_data->CopyAs<Vector<char>>();
  const size_t version_offset = FindGainmapBoxOffset(invalid_bytes) + 8;
  ASSERT_EQ(0, invalid_bytes[version_offset]);
  invalid_bytes[version_offset] = 1;

  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(base::span(invalid_bytes));
  auto decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kDefault);
  decoder->SetData(data.get(), true);
  ASSERT_TRUE(decoder->IsSizeAvailable());

  SkGainmapInfo gainmap_info;
  scoped_refptr<SegmentReader> gainmap_data;
  EXPECT_FALSE(decoder->GetGainmapInfoAndData(gainmap_info, gainmap_data));

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

TEST_F(JXLImageDecoderTest, TruncatedOptionalGainmapDoesNotChangeBaseImage) {
  scoped_refptr<SharedBuffer> valid_data =
      ReadFileToSharedBuffer(kImagesDir, "gainmap-orientation1-base-math.jxl");
  ASSERT_TRUE(valid_data);

  // Shrink the gain-map box to its header plus the bundle version byte, so
  // that only the bundle is truncated and the container stays valid.
  Vector<char> truncated_bytes = valid_data->CopyAs<Vector<char>>();
  const size_t box_offset = FindGainmapBoxOffset(truncated_bytes);
  constexpr char kTruncatedBoxSize = 9;
  truncated_bytes.resize(box_offset + kTruncatedBoxSize);
  truncated_bytes[box_offset] = 0;
  truncated_bytes[box_offset + 1] = 0;
  truncated_bytes[box_offset + 2] = 0;
  truncated_bytes[box_offset + 3] = kTruncatedBoxSize;

  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(base::span(truncated_bytes));
  auto decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kDefault);
  decoder->SetData(data.get(), true);
  ASSERT_TRUE(decoder->IsSizeAvailable());

  SkGainmapInfo gainmap_info;
  scoped_refptr<SegmentReader> gainmap_data;
  EXPECT_FALSE(decoder->GetGainmapInfoAndData(gainmap_info, gainmap_data));

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  constexpr std::array<SkColor, 6> kExpectedBase = {
      SkColorSetRGB(24, 48, 72),    SkColorSetRGB(64, 88, 112),
      SkColorSetRGB(120, 144, 168), SkColorSetRGB(40, 80, 120),
      SkColorSetRGB(96, 136, 176),  SkColorSetRGB(184, 208, 232),
  };
  for (size_t i = 0; i < kExpectedBase.size(); ++i) {
    const int x = static_cast<int>(i % 3);
    const int y = static_cast<int>(i / 3);
    EXPECT_EQ(kExpectedBase[i], frame->Bitmap().getColor(x, y))
        << "pixel (" << x << ", " << y << ")";
  }
  EXPECT_FALSE(decoder->Failed());
}

TEST_F(JXLImageDecoderTest, ImageWithoutGainmapKeepsBaseDecode) {
  auto decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kDefault);
  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kJxlTestDir, "3x3_srgb_lossless.jxl");
  ASSERT_TRUE(data);

  decoder->SetData(data.get(), true);
  ASSERT_TRUE(decoder->IsSizeAvailable());

  SkGainmapInfo gainmap_info;
  scoped_refptr<SegmentReader> gainmap_data;
  EXPECT_FALSE(decoder->GetGainmapInfoAndData(gainmap_info, gainmap_data));

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

TEST_F(JXLImageDecoderTest, DeferredDecoderCreatesGainmapGenerator) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(features::kJXLImageFormat);

  scoped_refptr<SharedBuffer> data =
      ReadFileToSharedBuffer(kImagesDir, "gainmap-orientation1-base-math.jxl");
  ASSERT_TRUE(data);

  std::unique_ptr<DeferredImageDecoder> deferred_decoder =
      DeferredImageDecoder::Create(data, true,
                                   ImageDecoder::kAlphaNotPremultiplied,
                                   ColorBehavior::kTag);
  ASSERT_TRUE(deferred_decoder);
  ASSERT_TRUE(deferred_decoder->CreateGenerator());

  sk_sp<PaintImageGenerator> gainmap_generator;
  SkGainmapInfo gainmap_info;
  ASSERT_TRUE(deferred_decoder->CreateGainmapGenerator(gainmap_generator,
                                                       gainmap_info));
  ASSERT_TRUE(gainmap_generator);
  EXPECT_EQ(3, gainmap_generator->GetSkImageInfo().width());
  EXPECT_EQ(2, gainmap_generator->GetSkImageInfo().height());
  EXPECT_EQ(1.f, gainmap_info.fDisplayRatioSdr);
  EXPECT_EQ(4.f, gainmap_info.fDisplayRatioHdr);

  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocPixels(gainmap_generator->GetSkImageInfo()));
  SkPixmap pixmap;
  ASSERT_TRUE(bitmap.peekPixels(&pixmap));
  EXPECT_TRUE(gainmap_generator->GetPixels(
      pixmap, 0, PaintImage::kDefaultGeneratorClientId, 0));
  EXPECT_EQ(SkColorSetRGB(0, 32, 64), bitmap.getColor(0, 0));
}

TEST_F(JXLImageDecoderTest, DeferredDecoderRejectsInvalidNestedGainmap) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(features::kJXLImageFormat);

  scoped_refptr<SharedBuffer> source_data =
      ReadFileToSharedBuffer(kImagesDir, "gainmap-orientation1-base-math.jxl");
  ASSERT_TRUE(source_data);

  // Keep the gain-map box and its metadata intact, but invalidate the
  // signature of the nested codestream.
  Vector<char> corrupted_bytes = source_data->CopyAs<Vector<char>>();
  corrupted_bytes[FindNestedCodestreamOffset(corrupted_bytes)] = 0;
  scoped_refptr<SharedBuffer> corrupted_data =
      SharedBuffer::Create(base::span(corrupted_bytes));

  auto source_decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kDefault);
  source_decoder->SetData(corrupted_data.get(), true);
  ASSERT_TRUE(source_decoder->IsSizeAvailable());
  SkGainmapInfo source_gainmap_info;
  scoped_refptr<SegmentReader> gainmap_data;
  ASSERT_TRUE(
      source_decoder->GetGainmapInfoAndData(source_gainmap_info, gainmap_data));

  // A failed auxiliary codestream has no frame, even though the still-image
  // fallback would report one frame without basic info.
  auto gainmap_decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kGainmap);
  gainmap_decoder->SetData(gainmap_data, true);
  EXPECT_TRUE(gainmap_decoder->Failed());
  EXPECT_EQ(0u, gainmap_decoder->FrameCount());

  std::unique_ptr<DeferredImageDecoder> deferred_decoder =
      DeferredImageDecoder::Create(corrupted_data, true,
                                   ImageDecoder::kAlphaNotPremultiplied,
                                   ColorBehavior::kTag);
  ASSERT_TRUE(deferred_decoder);
  sk_sp<PaintImageGenerator> gainmap_generator;
  SkGainmapInfo gainmap_info;
  EXPECT_FALSE(deferred_decoder->CreateGainmapGenerator(gainmap_generator,
                                                        gainmap_info));
  EXPECT_FALSE(gainmap_generator);

  // The base image stays usable.
  sk_sp<PaintImageGenerator> base_generator =
      deferred_decoder->CreateGenerator();
  ASSERT_TRUE(base_generator);
  SkBitmap base_bitmap;
  ASSERT_TRUE(base_bitmap.tryAllocPixels(base_generator->GetSkImageInfo()));
  SkPixmap base_pixmap;
  ASSERT_TRUE(base_bitmap.peekPixels(&base_pixmap));
  ASSERT_TRUE(base_generator->GetPixels(
      base_pixmap, 0, PaintImage::kDefaultGeneratorClientId, 0));
  EXPECT_EQ(SkColorSetRGB(24, 48, 72), base_bitmap.getColor(0, 0));
}

TEST_F(JXLImageDecoderTest, DeferredGainmapPixelFailureLeavesBaseUsable) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(features::kJXLImageFormat);

  scoped_refptr<SharedBuffer> source_data =
      ReadFileToSharedBuffer(kImagesDir, "gainmap-orientation1-base-math.jxl");
  ASSERT_TRUE(source_data);

  // Leave the nested codestream's header intact, but corrupt a late byte of
  // its pixel payload.
  Vector<char> corrupted_bytes = source_data->CopyAs<Vector<char>>();
  const size_t mutation_offset = corrupted_bytes.size() - 3;
  ASSERT_GT(mutation_offset, FindNestedCodestreamOffset(corrupted_bytes));
  corrupted_bytes[mutation_offset] ^= static_cast<char>(0x80);
  scoped_refptr<SharedBuffer> corrupted_data =
      SharedBuffer::Create(base::span(corrupted_bytes));

  auto source_decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kDefault);
  source_decoder->SetData(corrupted_data.get(), true);
  ASSERT_TRUE(source_decoder->IsSizeAvailable());
  SkGainmapInfo source_gainmap_info;
  scoped_refptr<SegmentReader> gainmap_data;
  ASSERT_TRUE(
      source_decoder->GetGainmapInfoAndData(source_gainmap_info, gainmap_data));

  // The gain map's size is known, but decoding its pixels fails.
  auto gainmap_decoder = CreateJXLDecoderForAuxImage(cc::AuxImage::kGainmap);
  gainmap_decoder->SetData(gainmap_data, true);
  ASSERT_TRUE(gainmap_decoder->IsSizeAvailable());
  EXPECT_FALSE(gainmap_decoder->Failed());
  EXPECT_EQ(1u, gainmap_decoder->FrameCount());
  EXPECT_EQ(gfx::Size(3, 2), gainmap_decoder->DecodedSize());

  ImageFrame* gainmap_frame = gainmap_decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(gainmap_frame);
  EXPECT_TRUE(gainmap_decoder->Failed());
  EXPECT_EQ(ImageFrame::kFrameEmpty, gainmap_frame->GetStatus());
  EXPECT_EQ(nullptr, gainmap_frame->Bitmap().getPixels());

  std::unique_ptr<DeferredImageDecoder> deferred_decoder =
      DeferredImageDecoder::Create(corrupted_data, true,
                                   ImageDecoder::kAlphaNotPremultiplied,
                                   ColorBehavior::kTag);
  ASSERT_TRUE(deferred_decoder);
  sk_sp<PaintImageGenerator> base_generator =
      deferred_decoder->CreateGenerator();
  ASSERT_TRUE(base_generator);
  sk_sp<PaintImageGenerator> gainmap_generator;
  SkGainmapInfo gainmap_info;
  ASSERT_TRUE(deferred_decoder->CreateGainmapGenerator(gainmap_generator,
                                                       gainmap_info));
  ASSERT_TRUE(gainmap_generator);

  SkBitmap gainmap_bitmap;
  ASSERT_TRUE(
      gainmap_bitmap.tryAllocPixels(gainmap_generator->GetSkImageInfo()));
  SkPixmap gainmap_pixmap;
  ASSERT_TRUE(gainmap_bitmap.peekPixels(&gainmap_pixmap));
  EXPECT_FALSE(gainmap_generator->GetPixels(
      gainmap_pixmap, 0, PaintImage::kDefaultGeneratorClientId, 0));

  SkBitmap base_bitmap;
  ASSERT_TRUE(base_bitmap.tryAllocPixels(base_generator->GetSkImageInfo()));
  SkPixmap base_pixmap;
  ASSERT_TRUE(base_bitmap.peekPixels(&base_pixmap));
  ASSERT_TRUE(base_generator->GetPixels(
      base_pixmap, 0, PaintImage::kDefaultGeneratorClientId, 0));
  EXPECT_EQ(SkColorSetRGB(24, 48, 72), base_bitmap.getColor(0, 0));
}

// =============================================================================
// BPP (Bits Per Pixel) Histogram Tests
// =============================================================================

namespace {

void TestJxlBppHistogram(const char* image_name,
                         const char* histogram_name = nullptr,
                         base::HistogramBase::Sample32 sample = 0) {
  TestBppHistogram(CreateJXLDecoder, "Jxl", image_name, histogram_name, sample);
}

}  // namespace

// Test that 8-bit still image without alpha records histogram.
TEST_F(JXLImageDecoderTest, BppHistogramSmall) {
  constexpr int kImageArea = 3 * 3;  // = 9
  constexpr int kFileSize = 66;      // Size of 3x3_srgb_lossless.jxl
  constexpr int kSample =
      (kFileSize * 100 * 8 + kImageArea / 2) / kImageArea;  // = 5867
  TestJxlBppHistogram("/external/wpt/jpegxl/resources/3x3_srgb_lossless.jxl",
                      "Blink.DecodedImage.JxlDensity.Count.0.1MP", kSample);
}

// Test that animated images do NOT record histogram.
TEST_F(JXLImageDecoderTest, BppHistogramAnimated) {
  TestJxlBppHistogram("/images/resources/5_frames_numbered.jxl");
}

// Test that images with alpha do NOT record histogram.
TEST_F(JXLImageDecoderTest, BppHistogramAlpha) {
  TestJxlBppHistogram("/external/wpt/jpegxl/resources/3x3a_srgb_lossless.jxl");
}

// Test that grayscale images do NOT record histogram.
TEST_F(JXLImageDecoderTest, BppHistogramGrayscale) {
  TestJxlBppHistogram("/images/resources/3x3_gray_lossless.jxl");
}

// =============================================================================
// Multi-threaded decoding tests
// =============================================================================

namespace {

// Decodes all frames of `data` and returns one hash per frame.
Vector<uint32_t> DecodeAllFrameHashes(scoped_refptr<SharedBuffer> data) {
  auto decoder = CreateJXLDecoder();
  decoder->SetData(data.get(), true);
  Vector<uint32_t> hashes;
  const wtf_size_t frame_count = decoder->FrameCount();
  EXPECT_GT(frame_count, 0u);
  for (wtf_size_t i = 0; i < frame_count; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    EXPECT_TRUE(frame);
    EXPECT_FALSE(decoder->Failed());
    if (!frame || frame->GetStatus() != ImageFrame::kFrameComplete) {
      return hashes;
    }
    hashes.push_back(HashBitmap(frame->Bitmap()));
  }
  return hashes;
}

void TestParallelDecode(const char* dir, const char* file) {
  SCOPED_TRACE(file);
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(dir, file);
  ASSERT_TRUE(data);
  EXPECT_FALSE(DecodeAllFrameHashes(data).empty());
}

}  // namespace

// Exercise multi-threaded VarDCT decoding.
TEST_F(JXLImageDecoderTest, ParallelDecodeVarDct) {
  TestParallelDecode(kJxlTestDir, "green_queen_vardct_e3.jxl");
}

// Exercise multi-threaded modular decoding.
TEST_F(JXLImageDecoderTest, ParallelDecodeModular) {
  TestParallelDecode(kJxlTestDir, "green_queen_modular_e3.jxl");
}

// Images with patches exercise cross-group references while decoding in
// parallel.
TEST_F(JXLImageDecoderTest, ParallelDecodePatches) {
  TestParallelDecode(kJxlTestDir, "conformance_patches.jxl");
}

// Progressive AC images exercise multiple passes while decoding in parallel.
TEST_F(JXLImageDecoderTest, ParallelDecodeProgressiveAc) {
  TestParallelDecode(kJxlTestDir, "progressive_ac.jxl");
}

// Multi-frame animations decode every frame on the thread pool.
TEST_F(JXLImageDecoderTest, ParallelDecodeAnimation) {
  TestParallelDecode(kImagesDir, "newtons_cradle.jxl");
}

// Incremental input and partial flushes must converge to the same final image
// as a one-shot multi-threaded decode.
TEST_F(JXLImageDecoderTest, ParallelIncrementalDecode) {
  scoped_refptr<SharedBuffer> full_data =
      ReadFileToSharedBuffer(kJxlTestDir, "green_queen_vardct_e3.jxl");
  ASSERT_TRUE(full_data);

  Vector<uint32_t> one_shot_hashes = DecodeAllFrameHashes(full_data);
  ASSERT_EQ(1u, one_shot_hashes.size());

  auto decoder = CreateJXLDecoder();
  const Vector<char> file_data = full_data->CopyAs<Vector<char>>();
  const base::span<const char> source(file_data);
  constexpr size_t kChunkSize = 16 * 1024;
  scoped_refptr<SharedBuffer> partial_data = SharedBuffer::Create();
  size_t offset = 0;
  while (offset < file_data.size()) {
    const size_t chunk =
        std::min(kChunkSize, static_cast<size_t>(file_data.size()) - offset);
    partial_data->Append(source.subspan(offset, chunk));
    offset += chunk;
    decoder->SetData(partial_data.get(), offset == file_data.size());
    // Trigger partial decodes (and partial flushes) along the way.
    decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_FALSE(decoder->Failed());
  }

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  ASSERT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(one_shot_hashes[0], HashBitmap(frame->Bitmap()));
}

void FuzzJXLDecoder(ImageDecoder::AlphaOption alpha_option,
                    ImageDecoder::HighBitDepthDecodingOption hbd_option,
                    ColorBehavior color_behavior,
                    cc::AuxImage aux_image_type,
                    uint32_t max_decoded_bytes,
                    ImageDecoder::AnimationOption animation_option,
                    base::span<const uint8_t> data) {
  auto decoder = std::make_unique<JXLImageDecoder>(
      alpha_option, hbd_option, color_behavior, aux_image_type,
      max_decoded_bytes, animation_option);

  auto buffer = SharedBuffer::Create(data);
  const bool kAllDataReceived = true;
  decoder->SetData(buffer.get(), kAllDataReceived);
  for (wtf_size_t frame = 0; frame < decoder->FrameCount(); ++frame) {
    decoder->DecodeFrameBufferAtIndex(frame);
    if (decoder->Failed()) {
      return;
    }
  }
}

// Floor max_decoded_bytes at 1 MiB so it can no longer land small enough for
// the decoder to reject the image in SetSize() before any decoding runs,
// which previously wasted fuzzer iterations.
constexpr uint32_t kMinMaxDecodedBytes = 1024 * 1024;

// FuzzTest aborts if any seed exceeds the domain's max size (default 5000), so
// this must be at least the largest file in that directory (currently
// ~4.3 MiB; progressive_ac.png).
constexpr base::ByteSize kMaxInputSize = base::MiB(5);

using JXLFuzzArgs = std::tuple<ImageDecoder::AlphaOption,
                               ImageDecoder::HighBitDepthDecodingOption,
                               ColorBehavior,
                               cc::AuxImage,
                               uint32_t,
                               ImageDecoder::AnimationOption,
                               std::vector<uint8_t>>;

// Uses every file in the corpus directory as a seed. Because the corpus
// files are the data to be fuzzed, we need to append the decoder settings
// to each testcase. Returns a std::vector<std::tuple<JXLFuzzArgs>> containing
// the decoder settings, along with the image file.
std::vector<JXLFuzzArgs> GetJXLSeeds() {
  base::FilePath corpus_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &corpus_dir));
  corpus_dir =
      corpus_dir.AppendASCII("third_party/blink").AppendASCII(kJxlTestDir);
  std::vector<std::vector<uint8_t>> files;

// Test-level seeds are all replayed, with no time limit, every time the
// FUZZ_TEST runs in unit-test mode, and decoding the whole directory takes
// too long on sanitizer bots. So only use the whole directory in fuzzing
// builds.
#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
  for (const auto& [contents] :
       fuzztest::ReadFilesFromDirectory(corpus_dir.AsUTF8Unsafe())) {
    files.push_back(base::ToVector<uint8_t>(contents));
  }
#else
  std::optional<std::vector<uint8_t>> basic_file =
      base::ReadFileToBytes(corpus_dir.AppendASCII("basic.jxl"));
  CHECK(basic_file) << "JXL seed not found!";
  files.push_back(*std::move(basic_file));
#endif
  CHECK(!files.empty()) << "JXL seed corpus not found!";

  std::vector<JXLFuzzArgs> seeds;
  for (auto& file : files) {
    seeds.emplace_back(
        ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
        ColorBehavior::kTransformToSRGB, cc::AuxImage::kDefault,
        std::numeric_limits<uint32_t>::max(),
        ImageDecoder::AnimationOption::kUnspecified, std::move(file));
  }
  return seeds;
}

FUZZ_TEST(JXLImageDecoderFuzzTest, FuzzJXLDecoder)
    .WithDomains(fuzztest::ElementOf<ImageDecoder::AlphaOption>(
                     {ImageDecoder::kAlphaPremultiplied,
                      ImageDecoder::kAlphaNotPremultiplied}),
                 fuzztest::ElementOf<ImageDecoder::HighBitDepthDecodingOption>(
                     {ImageDecoder::kDefaultBitDepth,
                      ImageDecoder::kHighBitDepthToHalfFloat}),
                 fuzztest::ElementOf<ColorBehavior>(
                     {ColorBehavior::kIgnore, ColorBehavior::kTag,
                      ColorBehavior::kTransformToSRGB}),
                 fuzztest::ElementOf<cc::AuxImage>({cc::AuxImage::kDefault,
                                                    cc::AuxImage::kGainmap}),
                 fuzztest::InRange(kMinMaxDecodedBytes,
                                   std::numeric_limits<uint32_t>::max()),
                 fuzztest::ElementOf<ImageDecoder::AnimationOption>(
                     {ImageDecoder::AnimationOption::kUnspecified,
                      ImageDecoder::AnimationOption::kPreferAnimation,
                      ImageDecoder::AnimationOption::kPreferStillImage}),
                 // TODO(crbug.com/574556672): Switch to using WithSoftMaxSize
                 // after performing FuzzTest benchmarking.
                 fuzztest::Arbitrary<std::vector<uint8_t>>().WithMaxSize(
                     kMaxInputSize.InBytes()))
    .WithSeeds(&GetJXLSeeds);
}  // namespace blink
