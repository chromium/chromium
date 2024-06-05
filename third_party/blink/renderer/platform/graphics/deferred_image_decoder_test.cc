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

#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"

#include <memory>
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_image_decoder.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_skia.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace {

// Raw data for a PNG file with 1x1 white pixels.
const unsigned char kWhitePNG[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xde, 0x00, 0x00, 0x00,
    0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
    0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00,
    0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x0c, 0x49,
    0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff, 0xff, 0x3f, 0x00, 0x05,
    0xfe, 0x02, 0xfe, 0xdc, 0xcc, 0x59, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x49,
    0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

// Raw data for a GIF file with 1x1 white pixels. Modified from animatedGIF.
const unsigned char kWhiteGIF[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0xf0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x21, 0xff, 0x0b, 0x4e, 0x45,
    0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2e, 0x30, 0x03, 0x01, 0x00,
    0x00, 0x00, 0x21, 0xff, 0x0b, 0x49, 0x6d, 0x61, 0x67, 0x65, 0x4d, 0x61,
    0x67, 0x69, 0x63, 0x6b, 0x0d, 0x67, 0x61, 0x6d, 0x6d, 0x61, 0x3d, 0x30,
    0x2e, 0x34, 0x35, 0x34, 0x35, 0x35, 0x00, 0x21, 0xff, 0x0b, 0x49, 0x6d,
    0x61, 0x67, 0x65, 0x4d, 0x61, 0x67, 0x69, 0x63, 0x6b, 0x0d, 0x67, 0x61,
    0x6d, 0x6d, 0x61, 0x3d, 0x30, 0x2e, 0x34, 0x35, 0x34, 0x35, 0x35, 0x00,
    0x21, 0xf9, 0x04, 0x00, 0x00, 0x00, 0xff, 0x00, 0x2c, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x4c, 0x01, 0x00, 0x3b};

}  // namespace

class DeferredImageDecoderTest : public testing::Test,
                                 public MockImageDecoderClient {
 public:
  void SetUp() override {
    paint_image_id_ = PaintImage::GetNextId();
    ImageDecodingStore::Instance().SetCacheLimitInBytes(1024 * 1024);
    data_ = SharedBuffer::Create(kWhitePNG, sizeof(kWhitePNG));
    original_data_ = data_->CopyAs<Vector<char>>();
    frame_count_ = 1;
    auto decoder = std::make_unique<MockImageDecoder>(this);
    actual_decoder_ = decoder.get();
    actual_decoder_->SetSize(1, 1);
    lazy_decoder_ = DeferredImageDecoder::CreateForTesting(std::move(decoder));
    bitmap_.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
    canvas_ = std::make_unique<cc::SkiaPaintCanvas>(bitmap_);
    decode_request_count_ = 0;
    repetition_count_ = kAnimationNone;
    status_ = ImageFrame::kFrameComplete;
    frame_duration_ = base::TimeDelta();
    decoded_size_ = actual_decoder_->Size();
  }

  void TearDown() override { ImageDecodingStore::Instance().Clear(); }

  void DecoderBeingDestroyed() override { actual_decoder_ = nullptr; }

  void DecodeRequested() override { ++decode_request_count_; }

  wtf_size_t FrameCount() override { return frame_count_; }

  int RepetitionCount() const override { return repetition_count_; }

  ImageFrame::Status GetStatus(wtf_size_t index) override { return status_; }

  base::TimeDelta FrameDuration() const override { return frame_duration_; }

  gfx::Size DecodedSize() const override { return decoded_size_; }

  PaintImage CreatePaintImage(
      PaintImage::CompletionState state = PaintImage::CompletionState::kDone) {
    return CreatePaintImage(lazy_decoder_.get(), state);
  }

  PaintImage CreatePaintImage(
      DeferredImageDecoder* decoder,
      PaintImage::CompletionState state = PaintImage::CompletionState::kDone) {
    PaintImage::AnimationType type = FrameCount() > 1
                                         ? PaintImage::AnimationType::kAnimated
                                         : PaintImage::AnimationType::kStatic;

    return PaintImageBuilder::WithDefault()
        .set_id(paint_image_id_)
        .set_animation_type(type)
        .set_completion_state(state)
        .set_paint_image_generator(decoder->CreateGenerator())
        .TakePaintImage();
  }

 protected:
  void UseMockImageDecoderFactory() {
    lazy_decoder_->FrameGenerator()->SetImageDecoderFactory(
        MockImageDecoderFactory::Create(this, decoded_size_));
  }

  test::TaskEnvironment task_environment_;
  // Don't own this but saves the pointer to query states.
  PaintImage::Id paint_image_id_;
  raw_ptr<MockImageDecoder> actual_decoder_;
  std::unique_ptr<DeferredImageDecoder> lazy_decoder_;
  SkBitmap bitmap_;
  std::unique_ptr<cc::PaintCanvas> canvas_;
  int decode_request_count_;
  scoped_refptr<SharedBuffer> data_;
  Vector<char> original_data_;
  wtf_size_t frame_count_;
  int repetition_count_;
  ImageFrame::Status status_;
  base::TimeDelta frame_duration_;
  gfx::Size decoded_size_;
};

TEST_F(DeferredImageDecoderTest, drawIntoPaintRecord) {
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  PaintImage image = CreatePaintImage();
  ASSERT_TRUE(image);
  EXPECT_EQ(1, image.width());
  EXPECT_EQ(1, image.height());

  PaintRecorder recorder;
  cc::PaintCanvas* temp_canvas = recorder.beginRecording();
  temp_canvas->drawImage(image, 0, 0);
  PaintRecord record = recorder.finishRecordingAsPicture();
  EXPECT_EQ(0, decode_request_count_);

  canvas_->drawPicture(std::move(record));
  EXPECT_EQ(0, decode_request_count_);
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), bitmap_.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, drawIntoPaintRecordProgressive) {
  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(original_data_.data(), original_data_.size() - 10);

  // Received only half the file.
  lazy_decoder_->SetData(partial_data, false /* all_data_received */);
  PaintRecorder recorder;
  cc::PaintCanvas* temp_canvas = recorder.beginRecording();
  PaintImage image =
      CreatePaintImage(PaintImage::CompletionState::kPartiallyDone);
  ASSERT_TRUE(image);
  temp_canvas->drawImage(image, 0, 0);
  canvas_->drawPicture(recorder.finishRecordingAsPicture());

  // Fully received the file and draw the PaintRecord again.
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  image = CreatePaintImage();
  ASSERT_TRUE(image);
  temp_canvas = recorder.beginRecording();
  temp_canvas->drawImage(image, 0, 0);
  canvas_->drawPicture(recorder.finishRecordingAsPicture());
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), bitmap_.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, allDataReceivedPriorToDecodeNonIncrementally) {
  // The image is received completely at once.
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  PaintImage image = CreatePaintImage();
  ASSERT_TRUE(image);
  ASSERT_TRUE(image.GetImageHeaderMetadata());
  EXPECT_TRUE(
      image.GetImageHeaderMetadata()->all_data_received_prior_to_decode);
}

TEST_F(DeferredImageDecoderTest, allDataReceivedPriorToDecodeIncrementally) {
  // The image is received in two parts, but a PaintImageGenerator is created
  // only after all the data is received.
  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(original_data_.data(), original_data_.size() - 10);
  lazy_decoder_->SetData(partial_data, false /* all_data_received */);
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  PaintImage image = CreatePaintImage();
  ASSERT_TRUE(image);
  ASSERT_TRUE(image.GetImageHeaderMetadata());
  EXPECT_TRUE(
      image.GetImageHeaderMetadata()->all_data_received_prior_to_decode);
}

TEST_F(DeferredImageDecoderTest, notAllDataReceivedPriorToDecode) {
  // The image is received in two parts, and a PaintImageGenerator is created
  // for each one. In real usage, it's likely that the software image decoder
  // will start working with partial data.
  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(original_data_.data(), original_data_.size() - 10);
  lazy_decoder_->SetData(partial_data, false /* all_data_received */);
  PaintImage image =
      CreatePaintImage(PaintImage::CompletionState::kPartiallyDone);
  ASSERT_TRUE(image);
  ASSERT_TRUE(image.GetImageHeaderMetadata());
  EXPECT_FALSE(
      image.GetImageHeaderMetadata()->all_data_received_prior_to_decode);

  lazy_decoder_->SetData(data_, true /* all_data_received */);
  image = CreatePaintImage();
  ASSERT_TRUE(image);
  ASSERT_TRUE(image.GetImageHeaderMetadata());
  EXPECT_FALSE(
      image.GetImageHeaderMetadata()->all_data_received_prior_to_decode);
}

static void RasterizeMain(cc::PaintCanvas* canvas, PaintRecord record) {
  canvas->drawPicture(std::move(record));
}

// Flaky on Mac. crbug.com/792540.
#if BUILDFLAG(IS_MAC)
#define MAYBE_decodeOnOtherThread DISABLED_decodeOnOtherThread
#else
#define MAYBE_decodeOnOtherThread decodeOnOtherThread
#endif
TEST_F(DeferredImageDecoderTest, MAYBE_decodeOnOtherThread) {
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  PaintImage image = CreatePaintImage();
  ASSERT_TRUE(image);
  EXPECT_EQ(1, image.width());
  EXPECT_EQ(1, image.height());

  PaintRecorder recorder;
  cc::PaintCanvas* temp_canvas = recorder.beginRecording();
  temp_canvas->drawImage(image, 0, 0);
  PaintRecord record = recorder.finishRecordingAsPicture();
  EXPECT_EQ(0, decode_request_count_);

  // Create a thread to rasterize PaintRecord.
  std::unique_ptr<NonMainThread> thread =
      NonMainThread::CreateThread(ThreadCreationParams(ThreadType::kTestThread)
                                      .SetThreadNameForTest("RasterThread"));
  PostCrossThreadTask(
      *thread->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&RasterizeMain, CrossThreadUnretained(canvas_.get()),
                          record));
  thread.reset();
  EXPECT_EQ(0, decode_request_count_);
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), bitmap_.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, singleFrameImageLoading) {
  status_ = ImageFrame::kFramePartial;
  lazy_decoder_->SetData(data_, false /* all_data_received */);
  EXPECT_FALSE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  PaintImage image = CreatePaintImage();
  ASSERT_TRUE(image);
  EXPECT_FALSE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(actual_decoder_);

  status_ = ImageFrame::kFrameComplete;
  data_->Append(" ", 1u);
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  EXPECT_FALSE(actual_decoder_);
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(0));

  image = CreatePaintImage();
  ASSERT_TRUE(image);
  EXPECT_FALSE(decode_request_count_);
}

TEST_F(DeferredImageDecoderTest, multiFrameImageLoading) {
  repetition_count_ = 10;
  frame_count_ = 1;
  frame_duration_ = base::Milliseconds(10);
  status_ = ImageFrame::kFramePartial;
  lazy_decoder_->SetData(data_, false /* all_data_received */);

  PaintImage image = CreatePaintImage();
  ASSERT_TRUE(image);
  EXPECT_FALSE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  // Anything <= 10ms is clamped to 100ms. See the implementation for details.
  EXPECT_EQ(base::Milliseconds(100), lazy_decoder_->FrameDurationAtIndex(0));

  frame_count_ = 2;
  frame_duration_ = base::Milliseconds(20);
  status_ = ImageFrame::kFrameComplete;
  data_->Append(" ", 1u);
  lazy_decoder_->SetData(data_, false /* all_data_received */);

  image = CreatePaintImage();
  ASSERT_TRUE(image);
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(1));
  EXPECT_EQ(base::Milliseconds(20), lazy_decoder_->FrameDurationAtIndex(1));
  EXPECT_TRUE(actual_decoder_);

  frame_count_ = 3;
  frame_duration_ = base::Milliseconds(30);
  status_ = ImageFrame::kFrameComplete;
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  EXPECT_FALSE(actual_decoder_);
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(1));
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(2));
  EXPECT_EQ(base::Milliseconds(100), lazy_decoder_->FrameDurationAtIndex(0));
  EXPECT_EQ(base::Milliseconds(20), lazy_decoder_->FrameDurationAtIndex(1));
  EXPECT_EQ(base::Milliseconds(30), lazy_decoder_->FrameDurationAtIndex(2));
  EXPECT_EQ(10, lazy_decoder_->RepetitionCount());
}

TEST_F(DeferredImageDecoderTest, decodedSize) {
  decoded_size_ = gfx::Size(22, 33);
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  PaintImage image = CreatePaintImage();
  ASSERT_TRUE(image);
  EXPECT_EQ(decoded_size_.width(), image.width());
  EXPECT_EQ(decoded_size_.height(), image.height());

  UseMockImageDecoderFactory();

  // The following code should not fail any assert.
  PaintRecorder recorder;
  cc::PaintCanvas* temp_canvas = recorder.beginRecording();
  temp_canvas->drawImage(image, 0, 0);
  PaintRecord record = recorder.finishRecordingAsPicture();
  EXPECT_EQ(0, decode_request_count_);
  canvas_->drawPicture(std::move(record));
  EXPECT_EQ(1, decode_request_count_);
}

TEST_F(DeferredImageDecoderTest, smallerFrameCount) {
  frame_count_ = 1;
  lazy_decoder_->SetData(data_, false /* all_data_received */);
  EXPECT_EQ(frame_count_, lazy_decoder_->FrameCount());
  frame_count_ = 2;
  lazy_decoder_->SetData(data_, false /* all_data_received */);
  EXPECT_EQ(frame_count_, lazy_decoder_->FrameCount());
  frame_count_ = 0;
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  EXPECT_EQ(frame_count_, lazy_decoder_->FrameCount());
}

TEST_F(DeferredImageDecoderTest, frameOpacity) {
  for (bool test_gif : {false, true}) {
    if (test_gif)
      data_ = SharedBuffer::Create(kWhiteGIF, sizeof(kWhiteGIF));

    std::unique_ptr<DeferredImageDecoder> decoder =
        DeferredImageDecoder::Create(data_, true,
                                     ImageDecoder::kAlphaPremultiplied,
                                     ColorBehavior::kTransformToSRGB);

    SkImageInfo pix_info = SkImageInfo::MakeN32Premul(1, 1);

    size_t row_bytes = pix_info.minRowBytes();
    size_t size = pix_info.computeByteSize(row_bytes);

    Vector<char> storage(base::checked_cast<wtf_size_t>(size));
    SkPixmap pixmap(pix_info, storage.data(), row_bytes);

    // Before decoding, the frame is not known to be opaque.
    sk_sp<SkImage> frame = CreatePaintImage(decoder.get()).GetSwSkImage();
    ASSERT_TRUE(frame);
    EXPECT_FALSE(frame->isOpaque());
    EXPECT_EQ(decoder->AlphaType(), kPremul_SkAlphaType);

    // Force a lazy decode by reading pixels.
    EXPECT_TRUE(frame->readPixels(pixmap, 0, 0));

    // After decoding, the frame is known to be opaque.
    EXPECT_EQ(decoder->AlphaType(), kOpaque_SkAlphaType);
    frame = CreatePaintImage(decoder.get()).GetSwSkImage();
    ASSERT_TRUE(frame);
    EXPECT_TRUE(frame->isOpaque());

    // Re-generating the opaque-marked frame should not fail.
    EXPECT_TRUE(frame->readPixels(pixmap, 0, 0));
  }
}

TEST_F(DeferredImageDecoderTest, data) {
  Vector<char> data_binary = data_->CopyAs<Vector<char>>();
  scoped_refptr<SharedBuffer> original_buffer =
      SharedBuffer::Create(data_binary.data(), data_binary.size());
  EXPECT_EQ(original_buffer->size(), data_binary.size());
  lazy_decoder_->SetData(original_buffer, false /* all_data_received */);
  scoped_refptr<SharedBuffer> new_buffer = lazy_decoder_->Data();
  EXPECT_EQ(original_buffer->size(), new_buffer->size());
  const Vector<char> original_data = original_buffer->CopyAs<Vector<char>>();
  const Vector<char> new_data = new_buffer->CopyAs<Vector<char>>();
  EXPECT_EQ(0, std::memcmp(original_data.data(), new_data.data(),
                           new_buffer->size()));
}

class MultiFrameDeferredImageDecoderTest : public DeferredImageDecoderTest {
 public:
  ImageFrame::Status GetStatus(wtf_size_t index) override {
    return index > last_complete_frame_ ? ImageFrame::Status::kFramePartial
                                        : ImageFrame::Status::kFrameComplete;
  }

  wtf_size_t last_complete_frame_ = 0u;
};

TEST_F(MultiFrameDeferredImageDecoderTest, PaintImage) {
  frame_count_ = 2;
  frame_duration_ = base::Milliseconds(20);
  last_complete_frame_ = 0u;
  lazy_decoder_->SetData(data_, false /* all_data_received */);

  // Only the first frame is complete.
  PaintImage image = CreatePaintImage();
  ASSERT_TRUE(image);
  EXPECT_EQ(image.GetFrameMetadata().size(), 2u);
  EXPECT_TRUE(image.GetFrameMetadata()[0].complete);
  EXPECT_FALSE(image.GetFrameMetadata()[1].complete);
  EXPECT_EQ(image.GetFrameMetadata()[0].duration, frame_duration_);
  EXPECT_EQ(image.GetFrameMetadata()[1].duration, frame_duration_);

  auto frame0_key = image.GetKeyForFrame(0);
  auto frame1_key = image.GetKeyForFrame(1);
  EXPECT_NE(frame0_key, frame1_key);

  // Send some more data but the frame status remains the same.
  last_complete_frame_ = 0u;
  lazy_decoder_->SetData(data_, false /* all_data_received */);
  PaintImage updated_image = CreatePaintImage();
  ASSERT_TRUE(updated_image);
  EXPECT_EQ(updated_image.GetFrameMetadata().size(), 2u);
  EXPECT_TRUE(updated_image.GetFrameMetadata()[0].complete);
  EXPECT_FALSE(updated_image.GetFrameMetadata()[1].complete);

  // Since the first frame was complete, the key remains constant. While the
  // second frame generates a new key after it is updated.
  auto updated_frame0_key = updated_image.GetKeyForFrame(0);
  auto updated_frame1_key = updated_image.GetKeyForFrame(1);
  EXPECT_NE(updated_frame0_key, updated_frame1_key);
  EXPECT_EQ(updated_frame0_key, frame0_key);
  EXPECT_NE(updated_frame1_key, frame1_key);

  // Mark all frames complete.
  last_complete_frame_ = 1u;
  lazy_decoder_->SetData(data_, true /* all_data_received */);
  PaintImage complete_image = CreatePaintImage();
  ASSERT_TRUE(complete_image);
  EXPECT_EQ(complete_image.GetFrameMetadata().size(), 2u);
  EXPECT_TRUE(complete_image.GetFrameMetadata()[0].complete);
  EXPECT_TRUE(complete_image.GetFrameMetadata()[1].complete);

  auto complete_frame0_key = complete_image.GetKeyForFrame(0);
  auto complete_frame1_key = complete_image.GetKeyForFrame(1);
  EXPECT_NE(complete_frame0_key, complete_frame1_key);
  EXPECT_EQ(updated_frame0_key, complete_frame0_key);
  EXPECT_NE(updated_frame1_key, complete_frame1_key);
}

TEST_F(MultiFrameDeferredImageDecoderTest, FrameDurationOverride) {
  frame_count_ = 2;
  frame_duration_ = base::Milliseconds(5);
  last_complete_frame_ = 1u;
  lazy_decoder_->SetData(data_, true /* all_data_received */);

  // If the frame duration is below a threshold, we override it to a constant
  // value of 100 ms.
  PaintImage image = CreatePaintImage();
  EXPECT_EQ(image.GetFrameMetadata()[0].duration, base::Milliseconds(100));
  EXPECT_EQ(image.GetFrameMetadata()[1].duration, base::Milliseconds(100));
}

}  // namespace blink
