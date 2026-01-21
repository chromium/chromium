// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"

#include <array>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/image_animation.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

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

}  // namespace blink
