// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/image_decoder_wrapper.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace blink {
namespace {

SkImageInfo ImageInfo() {
  return SkImageInfo::MakeN32Premul(100, 100);
}

}  // namespace

class ImageDecoderWrapperTest : public testing::Test,
                                public MockImageDecoderClient {
 public:
  void SetUp() override {
    ImageDecodingStore::Instance().SetCacheLimitInBytes(1024 * 1024);
    generator_ = ImageFrameGenerator::Create(
        SkISize::Make(100, 100), /*is_multi_frame=*/true,
        ColorBehavior::kIgnore, cc::AuxImage::kDefault, {});
    generator_->SetImageDecoderFactory(
        MockImageDecoderFactory::Create(this, SkISize::Make(100, 100)));
    segment_reader_ =
        SegmentReader::CreateFromSharedBuffer(SharedBuffer::Create());
  }

  void TearDown() override { ImageDecodingStore::Instance().Clear(); }
  void DecoderBeingDestroyed() override { ++decoders_destroyed_; }
  void DecodeRequested() override { ++decode_request_count_; }
  ImageFrame::Status GetStatus(wtf_size_t index) override { return status_; }
  wtf_size_t FrameCount() override { return 2; }
  int RepetitionCount() const override { return kAnimationLoopOnce; }
  base::TimeDelta FrameDuration() const override { return base::TimeDelta(); }
  gfx::Size FrameSize(wtf_size_t index) const override { return frame_size_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<SegmentReader> segment_reader_;
  scoped_refptr<ImageFrameGenerator> generator_;
  ImageFrame::Status status_ = ImageFrame::kFramePartial;
  gfx::Size frame_size_ = gfx::Size(100, 100);
  int decoders_destroyed_ = 0;
  int decode_request_count_ = 0;
};

TEST_F(ImageDecoderWrapperTest, RejectMismatchedFrameWithoutWritingPixels) {
  status_ = ImageFrame::kFrameComplete;
  frame_size_ = gfx::Size(100, 50);
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocPixels(ImageInfo()));
  bitmap.eraseColor(SK_ColorMAGENTA);
  const auto client_id = cc::PaintImage::GetNextGeneratorClientId();
  EXPECT_FALSE(generator_->DecodeAndScale(segment_reader_.get(), true, 0,
                                          bitmap.pixmap(), client_id));
  EXPECT_FALSE(generator_->DecodeFailed());
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(x, y));
    }
  }

  frame_size_ = gfx::Size(100, 100);
  EXPECT_TRUE(generator_->DecodeAndScale(segment_reader_.get(), true, 0,
                                         bitmap.pixmap(), client_id));
}

TEST_F(ImageDecoderWrapperTest, MismatchedCachedFrameUnlocksDecoder) {
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocPixels(ImageInfo()));
  const auto client_id = cc::PaintImage::GetNextGeneratorClientId();
  ASSERT_TRUE(generator_->DecodeAndScale(segment_reader_.get(), false, 0,
                                         bitmap.pixmap(), client_id));
  ASSERT_EQ(0, decoders_destroyed_);

  frame_size_ = gfx::Size(100, 50);
  bitmap.eraseColor(SK_ColorMAGENTA);
  EXPECT_FALSE(generator_->DecodeAndScale(segment_reader_.get(), false, 0,
                                          bitmap.pixmap(), client_id));
  EXPECT_FALSE(generator_->DecodeFailed());
  EXPECT_EQ(0, decoders_destroyed_);
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(x, y));
    }
  }

  frame_size_ = gfx::Size(100, 100);
  EXPECT_TRUE(generator_->DecodeAndScale(segment_reader_.get(), false, 0,
                                         bitmap.pixmap(), client_id));
  EXPECT_EQ(3, decode_request_count_);
  EXPECT_EQ(0, decoders_destroyed_);
  generator_ = nullptr;
  EXPECT_EQ(1, decoders_destroyed_);
}

TEST_F(ImageDecoderWrapperTest, PixelCopyFailureUnlocksDecoder) {
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocPixels(ImageInfo()));
  const auto client_id = cc::PaintImage::GetNextGeneratorClientId();
  ASSERT_TRUE(generator_->DecodeAndScale(segment_reader_.get(), false, 0,
                                         bitmap.pixmap(), client_id));
  ASSERT_EQ(0, decoders_destroyed_);

  // Matching dimensions reach readPixels(), but a null destination fails.
  SkPixmap invalid_pixmap(ImageInfo(), nullptr, bitmap.rowBytes());
  EXPECT_FALSE(generator_->DecodeAndScale(segment_reader_.get(), false, 0,
                                          invalid_pixmap, client_id));
  EXPECT_FALSE(generator_->DecodeFailed());
  EXPECT_EQ(0, decoders_destroyed_);

  EXPECT_TRUE(generator_->DecodeAndScale(segment_reader_.get(), false, 0,
                                         bitmap.pixmap(), client_id));
  EXPECT_EQ(3, decode_request_count_);
  EXPECT_EQ(0, decoders_destroyed_);
  generator_ = nullptr;
  EXPECT_EQ(1, decoders_destroyed_);
}

}  // namespace blink
