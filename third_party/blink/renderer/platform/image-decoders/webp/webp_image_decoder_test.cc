/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/image-decoders/webp/webp_image_decoder.h"

#include <memory>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

struct AnimParam {
  int x_offset, y_offset, width, height;
  ImageFrame::DisposalMethod disposal_method;
  ImageFrame::AlphaBlendSource alpha_blend_source;
  base::TimeDelta duration;
  bool has_alpha;
};

std::unique_ptr<ImageDecoder> CreateWEBPDecoder(
    ImageDecoder::AlphaOption alpha_option) {
  return std::make_unique<WEBPImageDecoder>(
      alpha_option, ColorBehavior::TransformToSRGB(),
      ImageDecoder::kNoDecodedImageByteLimit);
}

std::unique_ptr<ImageDecoder> CreateWEBPDecoder() {
  return CreateWEBPDecoder(ImageDecoder::kAlphaNotPremultiplied);
}

// If 'parse_error_expected' is true, error is expected during parse
// (FrameCount() call); else error is expected during decode
// (FrameBufferAtIndex() call).
void TestInvalidImage(const char* webp_file, bool parse_error_expected) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();

  scoped_refptr<SharedBuffer> data = ReadFile(webp_file);
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  if (parse_error_expected) {
    EXPECT_EQ(0u, decoder->FrameCount());
    EXPECT_FALSE(decoder->DecodeFrameBufferAtIndex(0));
  } else {
    EXPECT_GT(decoder->FrameCount(), 0u);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_EQ(ImageFrame::kFramePartial, frame->GetStatus());
  }
  EXPECT_EQ(kAnimationLoopOnce, decoder->RepetitionCount());
  EXPECT_TRUE(decoder->Failed());
}

}  // anonymous namespace

TEST(AnimatedWebPTests, uniqueGenerationIDs) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();

  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/webp-animated.webp");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  uint32_t generation_id0 = frame->Bitmap().getGenerationID();
  frame = decoder->DecodeFrameBufferAtIndex(1);
  uint32_t generation_id1 = frame->Bitmap().getGenerationID();

  EXPECT_TRUE(generation_id0 != generation_id1);
}

TEST(AnimatedWebPTests, verifyAnimationParametersTransparentImage) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();
  EXPECT_EQ(kAnimationLoopOnce, decoder->RepetitionCount());

  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/webp-animated.webp");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  const int kCanvasWidth = 11;
  const int kCanvasHeight = 29;
  const AnimParam kFrameParameters[] = {
      {0, 0, 11, 29, ImageFrame::kDisposeKeep,
       ImageFrame::kBlendAtopPreviousFrame,
       base::TimeDelta::FromMilliseconds(1000), true},
      {2, 10, 7, 17, ImageFrame::kDisposeKeep,
       ImageFrame::kBlendAtopPreviousFrame,
       base::TimeDelta::FromMilliseconds(500), true},
      {2, 2, 7, 16, ImageFrame::kDisposeKeep,
       ImageFrame::kBlendAtopPreviousFrame,
       base::TimeDelta::FromMilliseconds(1000), true},
  };

  for (size_t i = 0; i < base::size(kFrameParameters); ++i) {
    const ImageFrame* const frame = decoder->DecodeFrameBufferAtIndex(i);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
    EXPECT_EQ(kCanvasWidth, frame->Bitmap().width());
    EXPECT_EQ(kCanvasHeight, frame->Bitmap().height());
    EXPECT_EQ(kFrameParameters[i].x_offset, frame->OriginalFrameRect().X());
    EXPECT_EQ(kFrameParameters[i].y_offset, frame->OriginalFrameRect().Y());
    EXPECT_EQ(kFrameParameters[i].width, frame->OriginalFrameRect().Width());
    EXPECT_EQ(kFrameParameters[i].height, frame->OriginalFrameRect().Height());
    EXPECT_EQ(kFrameParameters[i].disposal_method, frame->GetDisposalMethod());
    EXPECT_EQ(kFrameParameters[i].alpha_blend_source,
              frame->GetAlphaBlendSource());
    EXPECT_EQ(kFrameParameters[i].duration, frame->Duration());
    EXPECT_EQ(kFrameParameters[i].has_alpha, frame->HasAlpha());
  }

  EXPECT_EQ(base::size(kFrameParameters), decoder->FrameCount());
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(AnimatedWebPTests,
     verifyAnimationParametersOpaqueFramesTransparentBackground) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();
  EXPECT_EQ(kAnimationLoopOnce, decoder->RepetitionCount());

  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/webp-animated-opaque.webp");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  const int kCanvasWidth = 94;
  const int kCanvasHeight = 87;
  const AnimParam kFrameParameters[] = {
      {4, 10, 33, 32, ImageFrame::kDisposeOverwriteBgcolor,
       ImageFrame::kBlendAtopPreviousFrame,
       base::TimeDelta::FromMilliseconds(1000), true},
      {34, 30, 33, 32, ImageFrame::kDisposeOverwriteBgcolor,
       ImageFrame::kBlendAtopPreviousFrame,
       base::TimeDelta::FromMilliseconds(1000), true},
      {62, 50, 32, 32, ImageFrame::kDisposeOverwriteBgcolor,
       ImageFrame::kBlendAtopPreviousFrame,
       base::TimeDelta::FromMilliseconds(1000), true},
      {10, 54, 32, 33, ImageFrame::kDisposeOverwriteBgcolor,
       ImageFrame::kBlendAtopPreviousFrame,
       base::TimeDelta::FromMilliseconds(1000), true},
  };

  for (size_t i = 0; i < base::size(kFrameParameters); ++i) {
    const ImageFrame* const frame = decoder->DecodeFrameBufferAtIndex(i);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
    EXPECT_EQ(kCanvasWidth, frame->Bitmap().width());
    EXPECT_EQ(kCanvasHeight, frame->Bitmap().height());
    EXPECT_EQ(kFrameParameters[i].x_offset, frame->OriginalFrameRect().X());
    EXPECT_EQ(kFrameParameters[i].y_offset, frame->OriginalFrameRect().Y());
    EXPECT_EQ(kFrameParameters[i].width, frame->OriginalFrameRect().Width());
    EXPECT_EQ(kFrameParameters[i].height, frame->OriginalFrameRect().Height());
    EXPECT_EQ(kFrameParameters[i].disposal_method, frame->GetDisposalMethod());
    EXPECT_EQ(kFrameParameters[i].alpha_blend_source,
              frame->GetAlphaBlendSource());
    EXPECT_EQ(kFrameParameters[i].duration, frame->Duration());
    EXPECT_EQ(kFrameParameters[i].has_alpha, frame->HasAlpha());
  }

  EXPECT_EQ(base::size(kFrameParameters), decoder->FrameCount());
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(AnimatedWebPTests, verifyAnimationParametersBlendOverwrite) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();
  EXPECT_EQ(kAnimationLoopOnce, decoder->RepetitionCount());

  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/webp-animated-no-blend.webp");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  const int kCanvasWidth = 94;
  const int kCanvasHeight = 87;
  const AnimParam kFrameParameters[] = {
      {4, 10, 33, 32, ImageFrame::kDisposeOverwriteBgcolor,
       ImageFrame::kBlendAtopBgcolor, base::TimeDelta::FromMilliseconds(1000),
       true},
      {34, 30, 33, 32, ImageFrame::kDisposeOverwriteBgcolor,
       ImageFrame::kBlendAtopBgcolor, base::TimeDelta::FromMilliseconds(1000),
       true},
      {62, 50, 32, 32, ImageFrame::kDisposeOverwriteBgcolor,
       ImageFrame::kBlendAtopBgcolor, base::TimeDelta::FromMilliseconds(1000),
       true},
      {10, 54, 32, 33, ImageFrame::kDisposeOverwriteBgcolor,
       ImageFrame::kBlendAtopBgcolor, base::TimeDelta::FromMilliseconds(1000),
       true},
  };

  for (size_t i = 0; i < base::size(kFrameParameters); ++i) {
    const ImageFrame* const frame = decoder->DecodeFrameBufferAtIndex(i);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
    EXPECT_EQ(kCanvasWidth, frame->Bitmap().width());
    EXPECT_EQ(kCanvasHeight, frame->Bitmap().height());
    EXPECT_EQ(kFrameParameters[i].x_offset, frame->OriginalFrameRect().X());
    EXPECT_EQ(kFrameParameters[i].y_offset, frame->OriginalFrameRect().Y());
    EXPECT_EQ(kFrameParameters[i].width, frame->OriginalFrameRect().Width());
    EXPECT_EQ(kFrameParameters[i].height, frame->OriginalFrameRect().Height());
    EXPECT_EQ(kFrameParameters[i].disposal_method, frame->GetDisposalMethod());
    EXPECT_EQ(kFrameParameters[i].alpha_blend_source,
              frame->GetAlphaBlendSource());
    EXPECT_EQ(kFrameParameters[i].duration, frame->Duration());
    EXPECT_EQ(kFrameParameters[i].has_alpha, frame->HasAlpha());
  }

  EXPECT_EQ(base::size(kFrameParameters), decoder->FrameCount());
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(AnimatedWebPTests, parseAndDecodeByteByByte) {
  TestByteByByteDecode(&CreateWEBPDecoder,
                       "/images/resources/webp-animated.webp", 3u,
                       kAnimationLoopInfinite);
  TestByteByByteDecode(&CreateWEBPDecoder,
                       "/images/resources/webp-animated-icc-xmp.webp", 13u,
                       31999);
}

TEST(AnimatedWebPTests, invalidImages) {
  // ANMF chunk size is smaller than ANMF header size.
  TestInvalidImage("/images/resources/invalid-animated-webp.webp", true);
  // One of the frame rectangles extends outside the image boundary.
  TestInvalidImage("/images/resources/invalid-animated-webp3.webp", true);
}

TEST(AnimatedWebPTests, truncatedLastFrame) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();

  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/invalid-animated-webp2.webp");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  size_t frame_count = 8;
  EXPECT_EQ(frame_count, decoder->FrameCount());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  frame = decoder->DecodeFrameBufferAtIndex(frame_count - 1);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFramePartial, frame->GetStatus());
  EXPECT_TRUE(decoder->Failed());
  frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
}

TEST(AnimatedWebPTests, truncatedInBetweenFrame) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();

  const Vector<char> full_data =
      ReadFile("/images/resources/invalid-animated-webp4.webp")
          ->CopyAs<Vector<char>>();
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(full_data.data(), full_data.size() - 1);
  decoder->SetData(data.get(), false);

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(1);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  frame = decoder->DecodeFrameBufferAtIndex(2);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFramePartial, frame->GetStatus());
  EXPECT_TRUE(decoder->Failed());
}

// Tests for a crash that used to happen for a specific file with specific
// sequence of method calls.
TEST(AnimatedWebPTests, reproCrash) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();

  scoped_refptr<SharedBuffer> full_data_buffer =
      ReadFile("/images/resources/invalid_vp8_vp8x.webp");
  ASSERT_TRUE(full_data_buffer.get());
  const Vector<char> full_data = full_data_buffer->CopyAs<Vector<char>>();

  // Parse partial data up to which error in bitstream is not detected.
  const size_t kPartialSize = 32768;
  ASSERT_GT(full_data.size(), kPartialSize);
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(full_data.data(), kPartialSize);
  decoder->SetData(data.get(), false);
  EXPECT_EQ(1u, decoder->FrameCount());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFramePartial, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());

  // Parse full data now. The error in bitstream should now be detected.
  decoder->SetData(full_data_buffer.get(), true);
  EXPECT_EQ(1u, decoder->FrameCount());
  frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFramePartial, frame->GetStatus());
  EXPECT_EQ(kAnimationLoopOnce, decoder->RepetitionCount());
  EXPECT_TRUE(decoder->Failed());
}

TEST(AnimatedWebPTests, progressiveDecode) {
  TestProgressiveDecoding(&CreateWEBPDecoder,
                          "/images/resources/webp-animated.webp");
}

TEST(AnimatedWebPTests, frameIsCompleteAndDuration) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();

  scoped_refptr<SharedBuffer> data_buffer =
      ReadFile("/images/resources/webp-animated.webp");
  ASSERT_TRUE(data_buffer.get());
  const Vector<char> data = data_buffer->CopyAs<Vector<char>>();

  ASSERT_GE(data.size(), 10u);
  scoped_refptr<SharedBuffer> temp_data =
      SharedBuffer::Create(data.data(), data.size() - 10);
  decoder->SetData(temp_data.get(), false);

  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000),
            decoder->FrameDurationAtIndex(0));
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(1));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(500),
            decoder->FrameDurationAtIndex(1));

  decoder->SetData(data_buffer.get(), true);
  EXPECT_EQ(3u, decoder->FrameCount());
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000),
            decoder->FrameDurationAtIndex(0));
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(1));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(500),
            decoder->FrameDurationAtIndex(1));
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(2));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000),
            decoder->FrameDurationAtIndex(2));
}

TEST(AnimatedWebPTests, updateRequiredPreviousFrameAfterFirstDecode) {
  TestUpdateRequiredPreviousFrameAfterFirstDecode(
      &CreateWEBPDecoder, "/images/resources/webp-animated.webp");
}

TEST(AnimatedWebPTests, randomFrameDecode) {
  TestRandomFrameDecode(&CreateWEBPDecoder,
                        "/images/resources/webp-animated.webp");
  TestRandomFrameDecode(&CreateWEBPDecoder,
                        "/images/resources/webp-animated-opaque.webp");
  TestRandomFrameDecode(&CreateWEBPDecoder,
                        "/images/resources/webp-animated-large.webp");
  TestRandomFrameDecode(&CreateWEBPDecoder,
                        "/images/resources/webp-animated-icc-xmp.webp");
}

TEST(AnimatedWebPTests, randomDecodeAfterClearFrameBufferCache) {
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateWEBPDecoder, "/images/resources/webp-animated.webp");
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateWEBPDecoder, "/images/resources/webp-animated-opaque.webp");
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateWEBPDecoder, "/images/resources/webp-animated-large.webp");
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateWEBPDecoder, "/images/resources/webp-animated-icc-xmp.webp");
}

TEST(AnimatedWebPTests,
     resumePartialDecodeAfterClearFrameBufferCache) {
  TestResumePartialDecodeAfterClearFrameBufferCache(
      &CreateWEBPDecoder, "/images/resources/webp-animated-large.webp");
}

TEST(AnimatedWebPTests, decodeAfterReallocatingData) {
  TestDecodeAfterReallocatingData(&CreateWEBPDecoder,
                                  "/images/resources/webp-animated.webp");
  TestDecodeAfterReallocatingData(
      &CreateWEBPDecoder, "/images/resources/webp-animated-icc-xmp.webp");
}

TEST(AnimatedWebPTests, alphaBlending) {
  TestAlphaBlending(&CreateWEBPDecoder, "/images/resources/webp-animated.webp");
  TestAlphaBlending(&CreateWEBPDecoder,
                    "/images/resources/webp-animated-semitransparent1.webp");
  TestAlphaBlending(&CreateWEBPDecoder,
                    "/images/resources/webp-animated-semitransparent2.webp");
  TestAlphaBlending(&CreateWEBPDecoder,
                    "/images/resources/webp-animated-semitransparent3.webp");
  TestAlphaBlending(&CreateWEBPDecoder,
                    "/images/resources/webp-animated-semitransparent4.webp");
}

TEST(AnimatedWebPTests, isSizeAvailable) {
  TestByteByByteSizeAvailable(&CreateWEBPDecoder,
                              "/images/resources/webp-animated.webp", 142u,
                              false, kAnimationLoopInfinite);
  // FIXME: Add color profile support for animated webp images.
  TestByteByByteSizeAvailable(&CreateWEBPDecoder,
                              "/images/resources/webp-animated-icc-xmp.webp",
                              1404u, false, 31999);
}

TEST(AnimatedWEBPTests, clearCacheExceptFrameWithAncestors) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();

  scoped_refptr<SharedBuffer> full_data =
      ReadFile("/images/resources/webp-animated.webp");
  ASSERT_TRUE(full_data.get());
  decoder->SetData(full_data.get(), true);

  ASSERT_EQ(3u, decoder->FrameCount());
  // We need to store pointers to the image frames, since calling
  // FrameBufferAtIndex will decode the frame if it is not FrameComplete,
  // and we want to read the status of the frame without decoding it again.
  ImageFrame* buffers[3];
  size_t buffer_sizes[3];
  for (size_t i = 0; i < decoder->FrameCount(); i++) {
    buffers[i] = decoder->DecodeFrameBufferAtIndex(i);
    ASSERT_EQ(ImageFrame::kFrameComplete, buffers[i]->GetStatus());
    buffer_sizes[i] = decoder->FrameBytesAtIndex(i);
  }

  // Explicitly set the required previous frame for the frames, since this test
  // is designed on this chain. Whether the frames actually depend on each
  // other is not important for this test - ClearCacheExceptFrame just looks at
  // the frame status and the required previous frame.
  buffers[1]->SetRequiredPreviousFrameIndex(0);
  buffers[2]->SetRequiredPreviousFrameIndex(1);

  // Clear the cache except for a single frame. All other frames should be
  // cleared to FrameEmpty, since this frame is FrameComplete.
  EXPECT_EQ(buffer_sizes[0] + buffer_sizes[2],
            decoder->ClearCacheExceptFrame(1));
  EXPECT_EQ(ImageFrame::kFrameEmpty, buffers[0]->GetStatus());
  EXPECT_EQ(ImageFrame::kFrameComplete, buffers[1]->GetStatus());
  EXPECT_EQ(ImageFrame::kFrameEmpty, buffers[2]->GetStatus());

  // Verify that the required previous frame is also preserved if the provided
  // frame is not FrameComplete. The simulated situation is:
  //
  // Frame 0          <---------    Frame 1         <---------    Frame 2
  // FrameComplete    depends on    FrameComplete   depends on    FramePartial
  //
  // The expected outcome is that frame 1 and frame 2 are preserved, since
  // frame 1 is necessary to fully decode frame 2.
  for (size_t i = 0; i < decoder->FrameCount(); i++) {
    ASSERT_EQ(ImageFrame::kFrameComplete,
              decoder->DecodeFrameBufferAtIndex(i)->GetStatus());
  }
  buffers[2]->SetStatus(ImageFrame::kFramePartial);
  EXPECT_EQ(buffer_sizes[0], decoder->ClearCacheExceptFrame(2));
  EXPECT_EQ(ImageFrame::kFrameEmpty, buffers[0]->GetStatus());
  EXPECT_EQ(ImageFrame::kFrameComplete, buffers[1]->GetStatus());
  EXPECT_EQ(ImageFrame::kFramePartial, buffers[2]->GetStatus());

  // Verify that the nearest FrameComplete required frame is preserved if
  // earlier required frames in the ancestor list are not FrameComplete. The
  // simulated situation is:
  //
  // Frame 0          <---------    Frame 1      <---------    Frame 2
  // FrameComplete    depends on    FrameEmpty   depends on    FramePartial
  //
  // The expected outcome is that frame 0 and frame 2 are preserved. Frame 2
  // should be preserved since it is the frame passed to ClearCacheExceptFrame.
  // Frame 0 should be preserved since it is the nearest FrameComplete ancestor.
  // Thus, since frame 1 is FrameEmpty, no data is cleared in this case.
  for (size_t i = 0; i < decoder->FrameCount(); i++) {
    ASSERT_EQ(ImageFrame::kFrameComplete,
              decoder->DecodeFrameBufferAtIndex(i)->GetStatus());
  }
  buffers[1]->SetStatus(ImageFrame::kFrameEmpty);
  buffers[2]->SetStatus(ImageFrame::kFramePartial);
  EXPECT_EQ(0u, decoder->ClearCacheExceptFrame(2));
  EXPECT_EQ(ImageFrame::kFrameComplete, buffers[0]->GetStatus());
  EXPECT_EQ(ImageFrame::kFrameEmpty, buffers[1]->GetStatus());
  EXPECT_EQ(ImageFrame::kFramePartial, buffers[2]->GetStatus());
}

TEST(StaticWebPTests, truncatedImage) {
  // VP8 data is truncated.
  TestInvalidImage("/images/resources/truncated.webp", false);
  // Chunk size in RIFF header doesn't match the file size.
  TestInvalidImage("/images/resources/truncated2.webp", true);
}

// Regression test for a bug where some valid images were failing to decode
// incrementally.
TEST(StaticWebPTests, incrementalDecode) {
  TestByteByByteDecode(&CreateWEBPDecoder,
                       "/images/resources/crbug.364830.webp", 1u,
                       kAnimationNone);
}

TEST(StaticWebPTests, isSizeAvailable) {
  TestByteByByteSizeAvailable(&CreateWEBPDecoder,
                              "/images/resources/webp-color-profile-lossy.webp",
                              520u, true, kAnimationNone);
  TestByteByByteSizeAvailable(&CreateWEBPDecoder, "/images/resources/test.webp",
                              30u, false, kAnimationNone);
}

TEST(StaticWebPTests, notAnimated) {
  std::unique_ptr<ImageDecoder> decoder = CreateWEBPDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/webp-color-profile-lossy.webp");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);
  EXPECT_EQ(1u, decoder->FrameCount());
  EXPECT_EQ(kAnimationNone, decoder->RepetitionCount());
}

}  // namespace blink
