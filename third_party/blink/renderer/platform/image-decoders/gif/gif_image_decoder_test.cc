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

#include "third_party/blink/renderer/platform/image-decoders/gif/gif_image_decoder.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

const char kWebTestsResourcesDir[] = "web_tests/images/resources";

std::unique_ptr<ImageDecoder> CreateDecoder() {
  return std::make_unique<GIFImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::TransformToSRGB(),
      ImageDecoder::kNoDecodedImageByteLimit);
}

void TestRepetitionCount(const char* dir,
                         const char* file,
                         int expected_repetition_count) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);
  EXPECT_EQ(expected_repetition_count, decoder->RepetitionCount());
}

}  // anonymous namespace

TEST(GIFImageDecoderTest, decodeTwoFrames) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  scoped_refptr<SharedBuffer> data =
      ReadFile(kWebTestsResourcesDir, "animated.gif");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  uint32_t generation_id0 = frame->Bitmap().getGenerationID();
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(16, frame->Bitmap().width());
  EXPECT_EQ(16, frame->Bitmap().height());

  frame = decoder->DecodeFrameBufferAtIndex(1);
  uint32_t generation_id1 = frame->Bitmap().getGenerationID();
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(16, frame->Bitmap().width());
  EXPECT_EQ(16, frame->Bitmap().height());
  EXPECT_TRUE(generation_id0 != generation_id1);

  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(GIFImageDecoderTest, crbug779261) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();
  scoped_refptr<SharedBuffer> data =
      ReadFile(kWebTestsResourcesDir, "crbug779261.gif");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  for (size_t i = 0; i < decoder->FrameCount(); ++i) {
    // In crbug.com/779261, an independent, transparent frame following an
    // opaque frame failed to decode. This image has an opaque frame 0 with
    // DisposalMethod::kDisposeOverwriteBgcolor, making frame 1, which has
    // transparency, independent and contain alpha.
    const bool has_alpha = 0 == i ? false : true;
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
    EXPECT_EQ(has_alpha, frame->HasAlpha());
  }

  EXPECT_FALSE(decoder->Failed());
}

TEST(GIFImageDecoderTest, parseAndDecode) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  scoped_refptr<SharedBuffer> data =
      ReadFile(kWebTestsResourcesDir, "animated.gif");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  // This call will parse the entire file.
  EXPECT_EQ(2u, decoder->FrameCount());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(16, frame->Bitmap().width());
  EXPECT_EQ(16, frame->Bitmap().height());

  frame = decoder->DecodeFrameBufferAtIndex(1);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(16, frame->Bitmap().width());
  EXPECT_EQ(16, frame->Bitmap().height());
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(GIFImageDecoderTest, parseByteByByte) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  const Vector<char> data =
      ReadFile(kWebTestsResourcesDir, "animated.gif")->CopyAs<Vector<char>>();

  size_t frame_count = 0;

  // Pass data to decoder byte by byte.
  for (size_t length = 1; length <= data.size(); ++length) {
    scoped_refptr<SharedBuffer> temp_data =
        SharedBuffer::Create(data.data(), length);
    decoder->SetData(temp_data.get(), length == data.size());

    EXPECT_LE(frame_count, decoder->FrameCount());
    frame_count = decoder->FrameCount();
  }

  EXPECT_EQ(2u, decoder->FrameCount());

  decoder->DecodeFrameBufferAtIndex(0);
  decoder->DecodeFrameBufferAtIndex(1);
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(GIFImageDecoderTest, parseAndDecodeByteByByte) {
  TestByteByByteDecode(&CreateDecoder, kWebTestsResourcesDir,
                       "animated-gif-with-offsets.gif", 5u,
                       kAnimationLoopInfinite);
}

TEST(GIFImageDecoderTest, brokenSecondFrame) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  scoped_refptr<SharedBuffer> data =
      ReadFile(kDecodersTestingDir, "broken.gif");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  // One frame is detected but cannot be decoded.
  EXPECT_EQ(1u, decoder->FrameCount());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(1);
  EXPECT_FALSE(frame);
}

TEST(GIFImageDecoderTest, progressiveDecode) {
  TestProgressiveDecoding(&CreateDecoder, kDecodersTestingDir, "radient.gif");
}

TEST(GIFImageDecoderTest, allDataReceivedTruncation) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  const Vector<char> data =
      ReadFile(kWebTestsResourcesDir, "animated.gif")->CopyAs<Vector<char>>();

  ASSERT_GE(data.size(), 10u);
  scoped_refptr<SharedBuffer> temp_data =
      SharedBuffer::Create(data.data(), data.size() - 10);
  decoder->SetData(temp_data.get(), true);

  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());

  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_FALSE(decoder->Failed());
  decoder->DecodeFrameBufferAtIndex(1);
  EXPECT_TRUE(decoder->Failed());
}

TEST(GIFImageDecoderTest, frameIsComplete) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  scoped_refptr<SharedBuffer> data =
      ReadFile(kWebTestsResourcesDir, "animated.gif");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(1));
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(GIFImageDecoderTest, frameIsCompleteLoading) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  scoped_refptr<SharedBuffer> data_buffer =
      ReadFile(kWebTestsResourcesDir, "animated.gif");
  ASSERT_TRUE(data_buffer.get());
  const Vector<char> data = data_buffer->CopyAs<Vector<char>>();

  ASSERT_GE(data.size(), 10u);
  scoped_refptr<SharedBuffer> temp_data =
      SharedBuffer::Create(data.data(), data.size() - 10);
  decoder->SetData(temp_data.get(), false);

  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  EXPECT_FALSE(decoder->FrameIsReceivedAtIndex(1));

  decoder->SetData(data_buffer.get(), true);
  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(1));
}

TEST(GIFImageDecoderTest, badTerminator) {
  scoped_refptr<SharedBuffer> reference_data =
      ReadFile(kDecodersTestingDir, "radient.gif");
  scoped_refptr<SharedBuffer> test_data =
      ReadFile(kDecodersTestingDir, "radient-bad-terminator.gif");
  ASSERT_TRUE(reference_data.get());
  ASSERT_TRUE(test_data.get());

  std::unique_ptr<ImageDecoder> reference_decoder = CreateDecoder();
  reference_decoder->SetData(reference_data.get(), true);
  EXPECT_EQ(1u, reference_decoder->FrameCount());
  ImageFrame* reference_frame = reference_decoder->DecodeFrameBufferAtIndex(0);
  DCHECK(reference_frame);

  std::unique_ptr<ImageDecoder> test_decoder = CreateDecoder();
  test_decoder->SetData(test_data.get(), true);
  EXPECT_EQ(1u, test_decoder->FrameCount());
  ImageFrame* test_frame = test_decoder->DecodeFrameBufferAtIndex(0);
  DCHECK(test_frame);

  EXPECT_EQ(HashBitmap(reference_frame->Bitmap()),
            HashBitmap(test_frame->Bitmap()));
}

TEST(GIFImageDecoderTest, updateRequiredPreviousFrameAfterFirstDecode) {
  TestUpdateRequiredPreviousFrameAfterFirstDecode(
      &CreateDecoder, kWebTestsResourcesDir, "animated-10color.gif");
}

TEST(GIFImageDecoderTest, randomFrameDecode) {
  // Single frame image.
  TestRandomFrameDecode(&CreateDecoder, kDecodersTestingDir, "radient.gif");
  // Multiple frame images.
  TestRandomFrameDecode(&CreateDecoder, kWebTestsResourcesDir,
                        "animated-gif-with-offsets.gif");
  TestRandomFrameDecode(&CreateDecoder, kWebTestsResourcesDir,
                        "animated-10color.gif");
}

TEST(GIFImageDecoderTest, randomDecodeAfterClearFrameBufferCache) {
  // Single frame image.
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateDecoder, kDecodersTestingDir, "radient.gif");
  // Multiple frame images.
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateDecoder, kWebTestsResourcesDir, "animated-gif-with-offsets.gif");
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateDecoder, kWebTestsResourcesDir, "animated-10color.gif");
}

TEST(GIFImageDecoderTest, resumePartialDecodeAfterClearFrameBufferCache) {
  TestResumePartialDecodeAfterClearFrameBufferCache(
      &CreateDecoder, kWebTestsResourcesDir, "animated-10color.gif");
}

// The first LZW codes in the image are invalid values that try to create a loop
// in the dictionary. Decoding should fail, but not infinitely loop or corrupt
// memory.
TEST(GIFImageDecoderTest, badInitialCode) {
  scoped_refptr<SharedBuffer> test_data =
      ReadFile(kDecodersTestingDir, "bad-initial-code.gif");
  ASSERT_TRUE(test_data.get());

  std::unique_ptr<ImageDecoder> test_decoder = CreateDecoder();
  test_decoder->SetData(test_data.get(), true);
  EXPECT_EQ(1u, test_decoder->FrameCount());
  ASSERT_TRUE(test_decoder->DecodeFrameBufferAtIndex(0));
  EXPECT_TRUE(test_decoder->Failed());
}

// The image has an invalid LZW code that exceeds dictionary size. Decoding
// should fail.
TEST(GIFImageDecoderTest, badCode) {
  scoped_refptr<SharedBuffer> test_data =
      ReadFile(kDecodersTestingDir, "bad-code.gif");
  ASSERT_TRUE(test_data.get());

  std::unique_ptr<ImageDecoder> test_decoder = CreateDecoder();
  test_decoder->SetData(test_data.get(), true);
  EXPECT_EQ(1u, test_decoder->FrameCount());
  ASSERT_TRUE(test_decoder->DecodeFrameBufferAtIndex(0));
  EXPECT_TRUE(test_decoder->Failed());
}

TEST(GIFImageDecoderTest, invalidDisposalMethod) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  // The image has 2 frames, with disposal method 4 and 5, respectively.
  scoped_refptr<SharedBuffer> data =
      ReadFile(kDecodersTestingDir, "invalid-disposal-method.gif");
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  EXPECT_EQ(2u, decoder->FrameCount());
  // Disposal method 4 is converted to ImageFrame::DisposeOverwritePrevious.
  // This is because some specs say method 3 is "overwrite previous", while
  // others say setting the third bit (i.e. method 4) is.
  EXPECT_EQ(ImageFrame::kDisposeOverwritePrevious,
            decoder->DecodeFrameBufferAtIndex(0)->GetDisposalMethod());
  // Unknown disposal methods (5 in this case) are converted to
  // ImageFrame::DisposeKeep.
  EXPECT_EQ(ImageFrame::kDisposeKeep,
            decoder->DecodeFrameBufferAtIndex(1)->GetDisposalMethod());
}

TEST(GIFImageDecoderTest, firstFrameHasGreaterSizeThanScreenSize) {
  const Vector<char> full_data =
      ReadFile(kDecodersTestingDir,
               "first-frame-has-greater-size-than-screen-size.gif")
          ->CopyAs<Vector<char>>();

  std::unique_ptr<ImageDecoder> decoder;
  IntSize frame_size;

  // Compute hashes when the file is truncated.
  for (size_t i = 1; i <= full_data.size(); ++i) {
    decoder = CreateDecoder();
    scoped_refptr<SharedBuffer> data =
        SharedBuffer::Create(full_data.data(), i);
    decoder->SetData(data.get(), i == full_data.size());

    if (decoder->IsSizeAvailable() && !frame_size.Width() &&
        !frame_size.Height()) {
      frame_size = decoder->DecodedSize();
      continue;
    }

    ASSERT_EQ(frame_size.Width(), decoder->DecodedSize().Width());
    ASSERT_EQ(frame_size.Height(), decoder->DecodedSize().Height());
  }
}

TEST(GIFImageDecoderTest, verifyRepetitionCount) {
  TestRepetitionCount(kWebTestsResourcesDir, "full2loop.gif", 2);
  TestRepetitionCount(kDecodersTestingDir, "radient.gif", kAnimationNone);
}

TEST(GIFImageDecoderTest, repetitionCountChangesWhenSeen) {
  scoped_refptr<SharedBuffer> full_data_buffer =
      ReadFile(kWebTestsResourcesDir, "animated-10color.gif");
  ASSERT_TRUE(full_data_buffer.get());
  const Vector<char> full_data = full_data_buffer->CopyAs<Vector<char>>();

  // This size must be before the repetition count is encountered in the file.
  const size_t kTruncatedSize = 60;
  ASSERT_TRUE(kTruncatedSize < full_data.size());
  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(full_data.data(), kTruncatedSize);

  std::unique_ptr<ImageDecoder> decoder = std::make_unique<GIFImageDecoder>(
      ImageDecoder::kAlphaPremultiplied, ColorBehavior::TransformToSRGB(),
      ImageDecoder::kNoDecodedImageByteLimit);

  decoder->SetData(partial_data.get(), false);
  ASSERT_EQ(kAnimationLoopOnce, decoder->RepetitionCount());
  decoder->SetData(full_data_buffer.get(), true);
  ASSERT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(GIFImageDecoderTest, bitmapAlphaType) {
  scoped_refptr<SharedBuffer> full_data_buffer =
      ReadFile(kDecodersTestingDir, "radient.gif");
  ASSERT_TRUE(full_data_buffer.get());
  const Vector<char> full_data = full_data_buffer->CopyAs<Vector<char>>();

  // Empirically chosen truncation size:
  //   a) large enough to produce a partial frame &&
  //   b) small enough to not fully decode the frame
  const size_t kTruncateSize = 800;
  ASSERT_TRUE(kTruncateSize < full_data.size());
  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(full_data.data(), kTruncateSize);

  std::unique_ptr<ImageDecoder> premul_decoder =
      std::make_unique<GIFImageDecoder>(ImageDecoder::kAlphaPremultiplied,
                                        ColorBehavior::TransformToSRGB(),
                                        ImageDecoder::kNoDecodedImageByteLimit);
  std::unique_ptr<ImageDecoder> unpremul_decoder =
      std::make_unique<GIFImageDecoder>(ImageDecoder::kAlphaNotPremultiplied,
                                        ColorBehavior::TransformToSRGB(),
                                        ImageDecoder::kNoDecodedImageByteLimit);

  // Partially decoded frame => the frame alpha type is unknown and should
  // reflect the requested format.
  premul_decoder->SetData(partial_data.get(), false);
  ASSERT_TRUE(premul_decoder->FrameCount());
  unpremul_decoder->SetData(partial_data.get(), false);
  ASSERT_TRUE(unpremul_decoder->FrameCount());
  ImageFrame* premul_frame = premul_decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(premul_frame &&
              premul_frame->GetStatus() != ImageFrame::kFrameComplete);
  EXPECT_EQ(kPremul_SkAlphaType, premul_frame->Bitmap().alphaType());
  ImageFrame* unpremul_frame = unpremul_decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(unpremul_frame &&
              unpremul_frame->GetStatus() != ImageFrame::kFrameComplete);
  EXPECT_EQ(kUnpremul_SkAlphaType, unpremul_frame->Bitmap().alphaType());

  // Fully decoded frame => the frame alpha type is known (opaque).
  premul_decoder->SetData(full_data_buffer.get(), true);
  ASSERT_TRUE(premul_decoder->FrameCount());
  unpremul_decoder->SetData(full_data_buffer.get(), true);
  ASSERT_TRUE(unpremul_decoder->FrameCount());
  premul_frame = premul_decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(premul_frame &&
              premul_frame->GetStatus() == ImageFrame::kFrameComplete);
  EXPECT_EQ(kOpaque_SkAlphaType, premul_frame->Bitmap().alphaType());
  unpremul_frame = unpremul_decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(unpremul_frame &&
              unpremul_frame->GetStatus() == ImageFrame::kFrameComplete);
  EXPECT_EQ(kOpaque_SkAlphaType, unpremul_frame->Bitmap().alphaType());
}

namespace {
// Needed to exercise ImageDecoder::SetMemoryAllocator, but still does the
// default allocation.
class Allocator final : public SkBitmap::Allocator {
  bool allocPixelRef(SkBitmap* dst) override { return dst->tryAllocPixels(); }
};
}

// Ensure that calling SetMemoryAllocator does not short-circuit
// InitializeNewFrame.
TEST(GIFImageDecoderTest, externalAllocator) {
  auto data = ReadFile(kWebTestsResourcesDir, "boston.gif");
  ASSERT_TRUE(data.get());

  auto decoder = CreateDecoder();
  decoder->SetData(data.get(), true);

  Allocator allocator;
  decoder->SetMemoryAllocator(&allocator);
  EXPECT_EQ(1u, decoder->FrameCount());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  decoder->SetMemoryAllocator(nullptr);

  ASSERT_TRUE(frame);
  EXPECT_EQ(IntRect(IntPoint(), decoder->Size()), frame->OriginalFrameRect());
  EXPECT_FALSE(frame->HasAlpha());
}

TEST(GIFImageDecoderTest, recursiveDecodeFailure) {
  auto data = ReadFile(kWebTestsResourcesDir, "count-down-color-test.gif");
  ASSERT_TRUE(data.get());

  {
    auto decoder = CreateDecoder();
    decoder->SetData(data.get(), true);
    for (size_t i = 0; i <= 3; ++i) {
      ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
      ASSERT_NE(frame, nullptr);
      ASSERT_EQ(frame->GetStatus(), ImageFrame::kFrameComplete);
    }
  }

  // Modify data to have an error in frame 2.
  const size_t kErrorOffset = 15302u;
  scoped_refptr<SharedBuffer> modified_data =
      SharedBuffer::Create(data->Data(), kErrorOffset);
  modified_data->Append("A", 1u);
  modified_data->Append(data->Data() + kErrorOffset + 1,
                        data->size() - kErrorOffset - 1);
  {
    auto decoder = CreateDecoder();
    decoder->SetData(modified_data.get(), true);
    decoder->DecodeFrameBufferAtIndex(2);
    ASSERT_TRUE(decoder->Failed());
  }

  {
    // Decode frame 3, recursively decoding frame 2, which 3 depends on.
    auto decoder = CreateDecoder();
    decoder->SetData(modified_data.get(), true);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(3);
    EXPECT_TRUE(decoder->Failed());
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->RequiredPreviousFrameIndex(), 2u);
  }
}

}  // namespace blink
