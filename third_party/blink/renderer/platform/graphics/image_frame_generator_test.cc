/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"

#include <memory>
#include "base/location.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Helper methods to generate standard sizes.
SkISize FullSize() {
  return SkISize::Make(100, 100);
}

SkImageInfo ImageInfo() {
  return SkImageInfo::Make(100, 100, kBGRA_8888_SkColorType,
                           kOpaque_SkAlphaType);
}

}  // namespace

class ImageFrameGeneratorTest : public testing::Test,
                                public MockImageDecoderClient {
 public:
  void SetUp() override {
    ImageDecodingStore::Instance().SetCacheLimitInBytes(1024 * 1024);
    generator_ = ImageFrameGenerator::Create(FullSize(), false,
                                             ColorBehavior::Ignore(), {});
    data_ = SharedBuffer::Create();
    segment_reader_ = SegmentReader::CreateFromSharedBuffer(data_);
    UseMockImageDecoderFactory();
    decoders_destroyed_ = 0;
    decode_request_count_ = 0;
    memory_allocator_set_count_ = 0;
    status_ = ImageFrame::kFrameEmpty;
    frame_count_ = 1;
    requested_clear_except_frame_ = kNotFound;
  }

  void TearDown() override { ImageDecodingStore::Instance().Clear(); }

  void DecoderBeingDestroyed() override { ++decoders_destroyed_; }

  void DecodeRequested() override { ++decode_request_count_; }

  void MemoryAllocatorSet() override { ++memory_allocator_set_count_; }

  ImageFrame::Status GetStatus(size_t index) override {
    ImageFrame::Status current_status = status_;
    status_ = next_frame_status_;
    return current_status;
  }

  void ClearCacheExceptFrameRequested(size_t clear_except_frame) override {
    requested_clear_except_frame_ = clear_except_frame;
  }

  size_t FrameCount() override { return frame_count_; }
  int RepetitionCount() const override {
    return frame_count_ == 1 ? kAnimationNone : kAnimationLoopOnce;
  }
  base::TimeDelta FrameDuration() const override { return base::TimeDelta(); }

 protected:
  void UseMockImageDecoderFactory() {
    generator_->SetImageDecoderFactory(
        MockImageDecoderFactory::Create(this, FullSize()));
  }

  void AddNewData() { data_->Append("g", 1u); }

  void SetFrameStatus(ImageFrame::Status status) {
    status_ = next_frame_status_ = status;
  }
  void SetNextFrameStatus(ImageFrame::Status status) {
    next_frame_status_ = status;
  }
  void SetFrameCount(size_t count) {
    frame_count_ = count;
    if (count > 1) {
      generator_ = nullptr;
      generator_ = ImageFrameGenerator::Create(FullSize(), true,
                                               ColorBehavior::Ignore(), {});
      UseMockImageDecoderFactory();
    }
  }
  void SetSupportedSizes(Vector<SkISize> sizes) {
    generator_ = nullptr;
    generator_ = ImageFrameGenerator::Create(
        FullSize(), true, ColorBehavior::Ignore(), std::move(sizes));
    UseMockImageDecoderFactory();
  }

  scoped_refptr<SharedBuffer> data_;
  scoped_refptr<SegmentReader> segment_reader_;
  scoped_refptr<ImageFrameGenerator> generator_;
  int decoders_destroyed_;
  int decode_request_count_;
  int memory_allocator_set_count_;
  ImageFrame::Status status_;
  ImageFrame::Status next_frame_status_;
  size_t frame_count_;
  size_t requested_clear_except_frame_;
};

TEST_F(ImageFrameGeneratorTest, GetSupportedSizes) {
  ASSERT_TRUE(FullSize() == SkISize::Make(100, 100));

  Vector<SkISize> supported_sizes = {SkISize::Make(2, 2), SkISize::Make(50, 50),
                                     SkISize::Make(75, 75), FullSize()};
  SetSupportedSizes(supported_sizes);

  struct Test {
    SkISize query_size;
    size_t supported_size_index;
  } tests[] = {{SkISize::Make(1, 1), 0},     {SkISize::Make(2, 2), 0},
               {SkISize::Make(25, 10), 1},   {SkISize::Make(1, 25), 1},
               {SkISize::Make(50, 51), 2},   {SkISize::Make(80, 80), 3},
               {SkISize::Make(100, 100), 3}, {SkISize::Make(1000, 1000), 3}};
  for (auto& test : tests) {
    EXPECT_TRUE(generator_->GetSupportedDecodeSize(test.query_size) ==
                supported_sizes[test.supported_size_index]);
  }
}

TEST_F(ImageFrameGeneratorTest, incompleteDecode) {
  SetFrameStatus(ImageFrame::kFramePartial);

  char buffer[100 * 100 * 4];
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(1, decode_request_count_);
  EXPECT_EQ(0, memory_allocator_set_count_);

  AddNewData();
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(2, decode_request_count_);
  EXPECT_EQ(0, decoders_destroyed_);
  EXPECT_EQ(0, memory_allocator_set_count_);
}

class ImageFrameGeneratorTestPlatform : public TestingPlatformSupport {
 public:
  bool IsLowEndDevice() override { return true; }
};

// This is the same as incompleteData, but with a low-end device set.
TEST_F(ImageFrameGeneratorTest, LowEndDeviceDestroysDecoderOnPartialDecode) {
  ScopedTestingPlatformSupport<ImageFrameGeneratorTestPlatform> platform;

  SetFrameStatus(ImageFrame::kFramePartial);

  char buffer[100 * 100 * 4];
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(1, decode_request_count_);
  EXPECT_EQ(1, decoders_destroyed_);
  // The memory allocator is set to the external one, then cleared after decode.
  EXPECT_EQ(2, memory_allocator_set_count_);

  AddNewData();
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(2, decode_request_count_);
  EXPECT_EQ(2, decoders_destroyed_);
  // The memory allocator is set to the external one, then cleared after decode.
  EXPECT_EQ(4, memory_allocator_set_count_);
}

TEST_F(ImageFrameGeneratorTest, incompleteDecodeBecomesComplete) {
  SetFrameStatus(ImageFrame::kFramePartial);

  char buffer[100 * 100 * 4];
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(1, decode_request_count_);
  EXPECT_EQ(0, decoders_destroyed_);
  EXPECT_EQ(0, memory_allocator_set_count_);

  SetFrameStatus(ImageFrame::kFrameComplete);
  AddNewData();

  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(2, decode_request_count_);
  EXPECT_EQ(1, decoders_destroyed_);

  // Decoder created again.
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(3, decode_request_count_);
}

static void DecodeThreadMain(ImageFrameGenerator* generator,
                             SegmentReader* segment_reader) {
  char buffer[100 * 100 * 4];
  generator->DecodeAndScale(segment_reader, false, 0, ImageInfo(), buffer,
                            100 * 4, ImageDecoder::kAlphaPremultiplied,
                            cc::PaintImage::kDefaultGeneratorClientId);
}

#if defined(OS_ANDROID) || defined(OS_LINUX)
// TODO(crbug.com/948641)
#define MAYBE_incompleteDecodeBecomesCompleteMultiThreaded \
  DISABLED_incompleteDecodeBecomesCompleteMultiThreaded
#else
#define MAYBE_incompleteDecodeBecomesCompleteMultiThreaded \
  incompleteDecodeBecomesCompleteMultiThreaded
#endif  // defined(OS_ANDROID) || defined(OS_LINUX)
TEST_F(ImageFrameGeneratorTest,
       MAYBE_incompleteDecodeBecomesCompleteMultiThreaded) {
  SetFrameStatus(ImageFrame::kFramePartial);

  char buffer[100 * 100 * 4];
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(1, decode_request_count_);
  EXPECT_EQ(0, decoders_destroyed_);

  // LocalFrame can now be decoded completely.
  SetFrameStatus(ImageFrame::kFrameComplete);
  AddNewData();
  std::unique_ptr<Thread> thread = Platform::Current()->CreateThread(
      ThreadCreationParams(ThreadType::kTestThread)
          .SetThreadNameForTest("DecodeThread"));
  PostCrossThreadTask(
      *thread->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&DecodeThreadMain, WTF::RetainedRef(generator_),
                          WTF::RetainedRef(segment_reader_)));
  thread.reset();
  EXPECT_EQ(2, decode_request_count_);
  EXPECT_EQ(1, decoders_destroyed_);

  // Decoder created again.
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(3, decode_request_count_);

  AddNewData();

  // Delete generator.
  generator_ = nullptr;
}

TEST_F(ImageFrameGeneratorTest, frameHasAlpha) {
  SetFrameStatus(ImageFrame::kFramePartial);

  char buffer[100 * 100 * 4];
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_TRUE(generator_->HasAlpha(0));
  EXPECT_EQ(1, decode_request_count_);

  ImageDecoder* temp_decoder = nullptr;
  EXPECT_TRUE(ImageDecodingStore::Instance().LockDecoder(
      generator_.get(), FullSize(), ImageDecoder::kAlphaPremultiplied,
      cc::PaintImage::kDefaultGeneratorClientId, &temp_decoder));
  ASSERT_TRUE(temp_decoder);
  temp_decoder->DecodeFrameBufferAtIndex(0)->SetHasAlpha(false);
  ImageDecodingStore::Instance().UnlockDecoder(
      generator_.get(), cc::PaintImage::kDefaultGeneratorClientId,
      temp_decoder);
  EXPECT_EQ(2, decode_request_count_);

  SetFrameStatus(ImageFrame::kFrameComplete);
  generator_->DecodeAndScale(segment_reader_.get(), false, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(3, decode_request_count_);
  EXPECT_FALSE(generator_->HasAlpha(0));
}

TEST_F(ImageFrameGeneratorTest, clearMultiFrameDecoder) {
  SetFrameCount(3);
  SetFrameStatus(ImageFrame::kFrameComplete);

  char buffer[100 * 100 * 4];
  generator_->DecodeAndScale(segment_reader_.get(), true, 0, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(1, decode_request_count_);
  EXPECT_EQ(0, decoders_destroyed_);
  EXPECT_EQ(0U, requested_clear_except_frame_);

  SetFrameStatus(ImageFrame::kFrameComplete);

  generator_->DecodeAndScale(segment_reader_.get(), true, 1, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(2, decode_request_count_);
  EXPECT_EQ(0, decoders_destroyed_);
  EXPECT_EQ(1U, requested_clear_except_frame_);

  SetFrameStatus(ImageFrame::kFrameComplete);

  // Decoding the last frame of a multi-frame images should trigger clearing
  // all the frame data, but not destroying the decoder.  See comments in
  // ImageFrameGenerator::tryToResumeDecode().
  generator_->DecodeAndScale(segment_reader_.get(), true, 2, ImageInfo(),
                             buffer, 100 * 4, ImageDecoder::kAlphaPremultiplied,
                             cc::PaintImage::kDefaultGeneratorClientId);
  EXPECT_EQ(3, decode_request_count_);
  EXPECT_EQ(0, decoders_destroyed_);
  EXPECT_EQ(kNotFound, requested_clear_except_frame_);
}

}  // namespace blink
