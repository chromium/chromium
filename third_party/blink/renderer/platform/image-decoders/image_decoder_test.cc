// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"

#include <memory>
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class TestImageDecoder : public ImageDecoder {
 public:
  explicit TestImageDecoder(
      ImageDecoder::HighBitDepthDecodingOption high_bit_depth_decoding_option,
      wtf_size_t max_decoded_bytes = kNoDecodedImageByteLimit)
      : ImageDecoder(kAlphaNotPremultiplied,
                     high_bit_depth_decoding_option,
                     ColorBehavior::kTransformToSRGB,
                     cc::AuxImage::kDefault,
                     max_decoded_bytes) {}

  TestImageDecoder() : TestImageDecoder(ImageDecoder::kDefaultBitDepth) {}

  String FilenameExtension() const override { return ""; }
  const AtomicString& MimeType() const override { return g_empty_atom; }

  Vector<ImageFrame, 1>& FrameBufferCache() { return frame_buffer_cache_; }

  void ResetRequiredPreviousFrames(bool known_opaque = false) {
    for (size_t i = 0; i < frame_buffer_cache_.size(); ++i) {
      frame_buffer_cache_[i].SetRequiredPreviousFrameIndex(
          FindRequiredPreviousFrame(i, known_opaque));
    }
  }

  void InitFrames(wtf_size_t num_frames,
                  unsigned width = 100,
                  unsigned height = 100) {
    SetSize(width, height);
    frame_buffer_cache_.resize(num_frames);
    for (wtf_size_t i = 0; i < num_frames; ++i) {
      frame_buffer_cache_[i].SetOriginalFrameRect(gfx::Rect(width, height));
    }
  }

  bool ImageIsHighBitDepth() override { return image_is_high_bit_depth_; }
  void SetImageToHighBitDepthForTest() { image_is_high_bit_depth_ = true; }

 private:
  bool image_is_high_bit_depth_ = false;
  void DecodeSize() override {}
  void Decode(wtf_size_t index) override {}
};

TEST(ImageDecoderTest, sizeCalculationMayOverflow) {
  // Test coverage:
  // Regular bit depth image with regular decoder
  // Regular bit depth image with high bit depth decoder
  // High bit depth image with regular decoder
  // High bit depth image with high bit depth decoder
  bool high_bit_depth_decoder_status[] = {false, true};
  bool high_bit_depth_image_status[] = {false, true};

  for (bool high_bit_depth_decoder : high_bit_depth_decoder_status) {
    for (bool high_bit_depth_image : high_bit_depth_image_status) {
      std::unique_ptr<TestImageDecoder> decoder;
      if (high_bit_depth_decoder) {
        decoder = std::make_unique<TestImageDecoder>(
            ImageDecoder::kHighBitDepthToHalfFloat);
      } else {
        decoder = std::make_unique<TestImageDecoder>();
      }
      if (high_bit_depth_image) {
        decoder->SetImageToHighBitDepthForTest();
      }

      unsigned log_pixel_size = 2;  // pixel is 4 bytes
      if (high_bit_depth_decoder && high_bit_depth_image) {
        log_pixel_size = 3;  // pixel is 8 byts
      }
      unsigned overflow_dim_shift = 31 - log_pixel_size;
      unsigned overflow_dim_shift_half = (overflow_dim_shift + 1) / 2;

      EXPECT_FALSE(decoder->SetSize(1 << overflow_dim_shift, 1));
      EXPECT_FALSE(decoder->SetSize(1, 1 << overflow_dim_shift));
      EXPECT_FALSE(decoder->SetSize(1 << overflow_dim_shift_half,
                                    1 << overflow_dim_shift_half));
      EXPECT_TRUE(decoder->SetSize(1 << (overflow_dim_shift - 1), 1));
      EXPECT_TRUE(decoder->SetSize(1, 1 << (overflow_dim_shift - 1)));
      EXPECT_TRUE(decoder->SetSize(1 << (overflow_dim_shift_half - 1),
                                   1 << (overflow_dim_shift_half - 1)));
    }
  }
}

TEST(ImageDecoderTest, requiredPreviousFrameIndex) {
  std::unique_ptr<TestImageDecoder> decoder(
      std::make_unique<TestImageDecoder>());
  decoder->InitFrames(6);
  Vector<ImageFrame, 1>& frame_buffers = decoder->FrameBufferCache();

  frame_buffers[1].SetDisposalMethod(ImageFrame::kDisposeKeep);
  frame_buffers[2].SetDisposalMethod(ImageFrame::kDisposeOverwritePrevious);
  frame_buffers[3].SetDisposalMethod(ImageFrame::kDisposeOverwritePrevious);
  frame_buffers[4].SetDisposalMethod(ImageFrame::kDisposeKeep);

  decoder->ResetRequiredPreviousFrames();

  // The first frame doesn't require any previous frame.
  EXPECT_EQ(kNotFound, frame_buffers[0].RequiredPreviousFrameIndex());
  // The previous DisposeNotSpecified frame is required.
  EXPECT_EQ(0u, frame_buffers[1].RequiredPreviousFrameIndex());
  // DisposeKeep is treated as DisposeNotSpecified.
  EXPECT_EQ(1u, frame_buffers[2].RequiredPreviousFrameIndex());
  // Previous DisposeOverwritePrevious frames are skipped.
  EXPECT_EQ(1u, frame_buffers[3].RequiredPreviousFrameIndex());
  EXPECT_EQ(1u, frame_buffers[4].RequiredPreviousFrameIndex());
  EXPECT_EQ(4u, frame_buffers[5].RequiredPreviousFrameIndex());
}

TEST(ImageDecoderTest, requiredPreviousFrameIndexDisposeOverwriteBgcolor) {
  std::unique_ptr<TestImageDecoder> decoder(
      std::make_unique<TestImageDecoder>());
  decoder->InitFrames(3);
  Vector<ImageFrame, 1>& frame_buffers = decoder->FrameBufferCache();

  // Fully covering DisposeOverwriteBgcolor previous frame resets the starting
  // state.
  frame_buffers[1].SetDisposalMethod(ImageFrame::kDisposeOverwriteBgcolor);
  decoder->ResetRequiredPreviousFrames();
  EXPECT_EQ(kNotFound, frame_buffers[2].RequiredPreviousFrameIndex());

  // Partially covering DisposeOverwriteBgcolor previous frame is required by
  // this frame.
  frame_buffers[1].SetOriginalFrameRect(gfx::Rect(50, 50, 50, 50));
  decoder->ResetRequiredPreviousFrames();
  EXPECT_EQ(1u, frame_buffers[2].RequiredPreviousFrameIndex());
}

TEST(ImageDecoderTest, requiredPreviousFrameIndexForFrame1) {
  std::unique_ptr<TestImageDecoder> decoder(
      std::make_unique<TestImageDecoder>());
  decoder->InitFrames(2);
  Vector<ImageFrame, 1>& frame_buffers = decoder->FrameBufferCache();

  decoder->ResetRequiredPreviousFrames();
  EXPECT_EQ(0u, frame_buffers[1].RequiredPreviousFrameIndex());

  // The first frame with DisposeOverwritePrevious or DisposeOverwriteBgcolor
  // resets the starting state.
  frame_buffers[0].SetDisposalMethod(ImageFrame::kDisposeOverwritePrevious);
  decoder->ResetRequiredPreviousFrames();
  EXPECT_EQ(kNotFound, frame_buffers[1].RequiredPreviousFrameIndex());
  frame_buffers[0].SetDisposalMethod(ImageFrame::kDisposeOverwriteBgcolor);
  decoder->ResetRequiredPreviousFrames();
  EXPECT_EQ(kNotFound, frame_buffers[1].RequiredPreviousFrameIndex());

  // ... even if it partially covers.
  frame_buffers[0].SetOriginalFrameRect(gfx::Rect(50, 50, 50, 50));

  frame_buffers[0].SetDisposalMethod(ImageFrame::kDisposeOverwritePrevious);
  decoder->ResetRequiredPreviousFrames();
  EXPECT_EQ(kNotFound, frame_buffers[1].RequiredPreviousFrameIndex());
  frame_buffers[0].SetDisposalMethod(ImageFrame::kDisposeOverwriteBgcolor);
  decoder->ResetRequiredPreviousFrames();
  EXPECT_EQ(kNotFound, frame_buffers[1].RequiredPreviousFrameIndex());
}

TEST(ImageDecoderTest, requiredPreviousFrameIndexBlendAtopBgcolor) {
  std::unique_ptr<TestImageDecoder> decoder(
      std::make_unique<TestImageDecoder>());
  decoder->InitFrames(3);
  Vector<ImageFrame, 1>& frame_buffers = decoder->FrameBufferCache();

  frame_buffers[1].SetOriginalFrameRect(gfx::Rect(25, 25, 50, 50));
  frame_buffers[2].SetAlphaBlendSource(ImageFrame::kBlendAtopBgcolor);

  // A full frame with 'blending method == BlendAtopBgcolor' doesn't depend on
  // any prior frames.
  for (int dispose_method = ImageFrame::kDisposeNotSpecified;
       dispose_method <= ImageFrame::kDisposeOverwritePrevious;
       ++dispose_method) {
    frame_buffers[1].SetDisposalMethod(
        static_cast<ImageFrame::DisposalMethod>(dispose_method));
    decoder->ResetRequiredPreviousFrames();
    EXPECT_EQ(kNotFound, frame_buffers[2].RequiredPreviousFrameIndex());
  }

  // A non-full frame with 'blending method == BlendAtopBgcolor' does depend on
  // a prior frame.
  frame_buffers[2].SetOriginalFrameRect(gfx::Rect(50, 50, 50, 50));
  for (int dispose_method = ImageFrame::kDisposeNotSpecified;
       dispose_method <= ImageFrame::kDisposeOverwritePrevious;
       ++dispose_method) {
    frame_buffers[1].SetDisposalMethod(
        static_cast<ImageFrame::DisposalMethod>(dispose_method));
    decoder->ResetRequiredPreviousFrames();
    EXPECT_NE(kNotFound, frame_buffers[2].RequiredPreviousFrameIndex());
  }
}

TEST(ImageDecoderTest, requiredPreviousFrameIndexKnownOpaque) {
  std::unique_ptr<TestImageDecoder> decoder(
      std::make_unique<TestImageDecoder>());
  decoder->InitFrames(3);
  Vector<ImageFrame, 1>& frame_buffers = decoder->FrameBufferCache();

  frame_buffers[1].SetOriginalFrameRect(gfx::Rect(25, 25, 50, 50));

  // A full frame that is known to be opaque doesn't depend on any prior frames.
  for (int dispose_method = ImageFrame::kDisposeNotSpecified;
       dispose_method <= ImageFrame::kDisposeOverwritePrevious;
       ++dispose_method) {
    frame_buffers[1].SetDisposalMethod(
        static_cast<ImageFrame::DisposalMethod>(dispose_method));
    decoder->ResetRequiredPreviousFrames(true);
    EXPECT_EQ(kNotFound, frame_buffers[2].RequiredPreviousFrameIndex());
  }

  // A non-full frame that is known to be opaque does depend on a prior frame.
  frame_buffers[2].SetOriginalFrameRect(gfx::Rect(50, 50, 50, 50));
  for (int dispose_method = ImageFrame::kDisposeNotSpecified;
       dispose_method <= ImageFrame::kDisposeOverwritePrevious;
       ++dispose_method) {
    frame_buffers[1].SetDisposalMethod(
        static_cast<ImageFrame::DisposalMethod>(dispose_method));
    decoder->ResetRequiredPreviousFrames(true);
    EXPECT_NE(kNotFound, frame_buffers[2].RequiredPreviousFrameIndex());
  }
}

TEST(ImageDecoderTest, clearCacheExceptFrameDoNothing) {
  std::unique_ptr<TestImageDecoder> decoder(
      std::make_unique<TestImageDecoder>());
  decoder->ClearCacheExceptFrame(0);

  // This should not crash.
  decoder->InitFrames(20);
  decoder->ClearCacheExceptFrame(kNotFound);
}

TEST(ImageDecoderTest, clearCacheExceptFrameAll) {
  const size_t kNumFrames = 10;
  std::unique_ptr<TestImageDecoder> decoder(
      std::make_unique<TestImageDecoder>());
  decoder->InitFrames(kNumFrames);
  Vector<ImageFrame, 1>& frame_buffers = decoder->FrameBufferCache();
  for (size_t i = 0; i < kNumFrames; ++i) {
    frame_buffers[i].SetStatus(i % 2 ? ImageFrame::kFramePartial
                                     : ImageFrame::kFrameComplete);
  }

  decoder->ClearCacheExceptFrame(kNotFound);

  for (size_t i = 0; i < kNumFrames; ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(ImageFrame::kFrameEmpty, frame_buffers[i].GetStatus());
  }
}

TEST(ImageDecoderTest, clearCacheExceptFramePreverveClearExceptFrame) {
  const wtf_size_t kNumFrames = 10;
  std::unique_ptr<TestImageDecoder> decoder(
      std::make_unique<TestImageDecoder>());
  decoder->InitFrames(kNumFrames);
  Vector<ImageFrame, 1>& frame_buffers = decoder->FrameBufferCache();
  for (size_t i = 0; i < kNumFrames; ++i) {
    frame_buffers[i].SetStatus(ImageFrame::kFrameComplete);
  }

  decoder->ResetRequiredPreviousFrames();
  decoder->ClearCacheExceptFrame(5);
  for (wtf_size_t i = 0; i < kNumFrames; ++i) {
    SCOPED_TRACE(testing::Message() << i);
    if (i == 5) {
      EXPECT_EQ(ImageFrame::kFrameComplete, frame_buffers[i].GetStatus());
    } else {
      EXPECT_EQ(ImageFrame::kFrameEmpty, frame_buffers[i].GetStatus());
    }
  }
}

#if BUILDFLAG(IS_FUCHSIA)

TEST(ImageDecoderTest, decodedSizeLimitBoundary) {
  constexpr unsigned kWidth = 100;
  constexpr unsigned kHeight = 200;
  constexpr unsigned kBitDepth = 4;
  std::unique_ptr<TestImageDecoder> decoder(std::make_unique<TestImageDecoder>(
      ImageDecoder::kDefaultBitDepth, (kWidth * kHeight * kBitDepth)));

  // Smallest allowable size, should succeed.
  EXPECT_TRUE(decoder->SetSize(1, 1));
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());

  // At the limit, should succeed.
  EXPECT_TRUE(decoder->SetSize(kWidth, kHeight));
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());

  // Just over the limit, should fail.
  EXPECT_TRUE(decoder->SetSize(kWidth + 1, kHeight));
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->Failed());
}

TEST(ImageDecoderTest, decodedSizeUnlimited) {
  // Very large values for width and height should be OK.
  constexpr unsigned kWidth = 10000;
  constexpr unsigned kHeight = 10000;

  std::unique_ptr<TestImageDecoder> decoder(std::make_unique<TestImageDecoder>(
      ImageDecoder::kDefaultBitDepth, ImageDecoder::kNoDecodedImageByteLimit));
  EXPECT_TRUE(decoder->SetSize(kWidth, kHeight));
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());
}

#else

// The limit is currently ignored on non-Fuchsia platforms (except for
// JPEG, which would decode a down-sampled version).
TEST(ImageDecoderTest, decodedSizeLimitIsIgnored) {
  constexpr unsigned kWidth = 100;
  constexpr unsigned kHeight = 200;
  constexpr unsigned kBitDepth = 4;
  std::unique_ptr<TestImageDecoder> decoder(std::make_unique<TestImageDecoder>(
      ImageDecoder::kDefaultBitDepth, (kWidth * kHeight * kBitDepth)));

  // Just over the limit. The limit should be ignored.
  EXPECT_TRUE(decoder->SetSize(kWidth + 1, kHeight));
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());
}

#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(ENABLE_AV1_DECODER)
TEST(ImageDecoderTest, hasSufficientDataToSniffMimeTypeAvif) {
  // The first 36 bytes of the Netflix AVIF test image
  // Chimera-AV1-10bit-1280x720-2380kbps-100.avif. Since the major_brand is
  // not "avif" or "avis", we must parse the compatible_brands to determine if
  // this is an AVIF image.
  constexpr uint8_t kData[] = {
      // A File Type Box.
      0x00, 0x00, 0x00, 0x1c,  // unsigned int(32) size; 0x1c = 28
      'f', 't', 'y', 'p',      // unsigned int(32) type = boxtype;
      'm', 'i', 'f', '1',      // unsigned int(32) major_brand;
      0x00, 0x00, 0x00, 0x00,  // unsigned int(32) minor_version;
      'm', 'i', 'f', '1',      // unsigned int(32) compatible_brands[];
      'a', 'v', 'i', 'f',      //
      'm', 'i', 'a', 'f',      //
      // The beginning of a Media Data Box.
      0x00, 0x00, 0xa4, 0x3a,  // unsigned int(32) size;
      'm', 'd', 'a', 't'       // unsigned int(32) type = boxtype;
  };

  scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create<size_t>(kData, 8);
  EXPECT_FALSE(ImageDecoder::HasSufficientDataToSniffMimeType(*buffer));
  EXPECT_EQ(ImageDecoder::SniffMimeType(buffer), String());
  buffer->Append<size_t>(kData + 8, 8);
  EXPECT_FALSE(ImageDecoder::HasSufficientDataToSniffMimeType(*buffer));
  EXPECT_EQ(ImageDecoder::SniffMimeType(buffer), String());
  buffer->Append<size_t>(kData + 16, sizeof(kData) - 16);
  EXPECT_TRUE(ImageDecoder::HasSufficientDataToSniffMimeType(*buffer));
  EXPECT_EQ(ImageDecoder::SniffMimeType(buffer), "image/avif");
}
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

}  // namespace blink
