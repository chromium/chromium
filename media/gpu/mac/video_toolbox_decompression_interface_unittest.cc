// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_status.h"
#include "media/base/media_util.h"
#include "media/gpu/mac/video_toolbox_decompression_interface.h"
#include "media/gpu/mac/video_toolbox_decompression_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using testing::_;

namespace {

void* CreateContext(intptr_t i) {
  return reinterpret_cast<void*>(i);
}

base::ScopedCFTypeRef<CMFormatDescriptionRef> CreateFormat() {
  base::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  OSStatus status =
      CMFormatDescriptionCreate(kCFAllocatorDefault, kCMMediaType_Video, 'test',
                                nullptr, format.InitializeInto());
  CHECK_EQ(status, noErr);
  return format;
}

base::ScopedCFTypeRef<CMSampleBufferRef> CreateSample(
    CMFormatDescriptionRef format) {
  base::ScopedCFTypeRef<CMSampleBufferRef> sample;
  OSStatus status = CMSampleBufferCreate(
      kCFAllocatorDefault, nullptr, true, nullptr, nullptr, format, 0, 0,
      nullptr, 0, nullptr, sample.InitializeInto());
  CHECK_EQ(status, noErr);
  return sample;
}

base::ScopedCFTypeRef<CVImageBufferRef> CreateImage() {
  base::ScopedCFTypeRef<CVImageBufferRef> image;
  OSStatus status =
      CVPixelBufferCreate(kCFAllocatorDefault, /*width=*/16, /*height=*/16,
                          kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                          nullptr, image.InitializeInto());
  CHECK_EQ(status, noErr);
  return image;
}

class FakeDecompressionSession : public VideoToolboxDecompressionSession {
 public:
  FakeDecompressionSession(raw_ptr<VideoToolboxDecompressionInterface> vtdi)
      : vtdi_(vtdi) {}

  ~FakeDecompressionSession() override = default;

  bool Create(CMFormatDescriptionRef format,
              CFMutableDictionaryRef decoder_config) override {
    CHECK(!IsValid());
    ++creations;
    if (can_create) {
      valid_ = true;
    }
    return can_create;
  }

  void Invalidate() override {
    valid_ = false;
    pending_decodes_ = {};
  }

  bool IsValid() override { return valid_; }

  bool CanAcceptFormat(CMFormatDescriptionRef format) override {
    CHECK(valid_);
    return can_accept_format;
  }

  bool DecodeFrame(CMSampleBufferRef sample, void* context) override {
    if (can_decode_frame) {
      pending_decodes_.push(context);
    }
    return can_decode_frame;
  }

  // Call vtdi->OnOutput() for the first pending decode, with an image.
  void CompleteDecode() {
    CHECK(!pending_decodes_.empty());

    void* context = pending_decodes_.front();
    OSStatus status = noErr;
    VTDecodeInfoFlags flags = 0;
    base::ScopedCFTypeRef<CVImageBufferRef> image = CreateImage();

    pending_decodes_.pop();
    vtdi_->OnOutput(context, status, flags, std::move(image));
  }

  // Call vtdi->OnOutput() for the first pending decode, with an error code.
  void FailDecode() {
    CHECK(!pending_decodes_.empty());

    void* context = pending_decodes_.front();
    OSStatus status = -1;
    VTDecodeInfoFlags flags = 0;
    base::ScopedCFTypeRef<CVImageBufferRef> image;

    pending_decodes_.pop();
    vtdi_->OnOutput(context, status, flags, std::move(image));
  }

  size_t ActiveDecodes() { return pending_decodes_.size(); }

  bool can_create = true;
  bool can_accept_format = true;
  bool can_decode_frame = true;

  size_t creations = 0;

 private:
  raw_ptr<VideoToolboxDecompressionInterface> vtdi_ = nullptr;
  bool valid_ = false;
  base::queue<void*> pending_decodes_;
};

}  // namespace

class VideoToolboxDecompressionInterfaceTest : public testing::Test {
 public:
  VideoToolboxDecompressionInterfaceTest() {
    video_toolbox_->SetDecompressionSessionForTesting(
        base::WrapUnique(decompression_session_.get()));
  }

  ~VideoToolboxDecompressionInterfaceTest() override = default;

 protected:
  MOCK_METHOD1(OnError, void(DecoderStatus));
  MOCK_METHOD2(OnOutput, void(base::ScopedCFTypeRef<CVImageBufferRef>, void*));

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoToolboxDecompressionInterface> video_toolbox_{
      std::make_unique<VideoToolboxDecompressionInterface>(
          task_environment_.GetMainThreadTaskRunner(),
          std::make_unique<NullMediaLog>(),
          base::BindRepeating(&VideoToolboxDecompressionInterfaceTest::OnOutput,
                              base::Unretained(this)),
          base::BindOnce(&VideoToolboxDecompressionInterfaceTest::OnError,
                         base::Unretained(this)))};
  raw_ptr<FakeDecompressionSession> decompression_session_ =
      new FakeDecompressionSession(video_toolbox_.get());
};

TEST_F(VideoToolboxDecompressionInterfaceTest, Construct) {}

TEST_F(VideoToolboxDecompressionInterfaceTest, Decode) {
  auto format = CreateFormat();
  auto sample = CreateSample(format);
  void* context = CreateContext(0);

  video_toolbox_->Decode(sample, context);

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 1ul);
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 1ul);

  EXPECT_CALL(*this, OnOutput(_, context));

  decompression_session_->CompleteDecode();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

TEST_F(VideoToolboxDecompressionInterfaceTest, CreateFailure) {
  auto format = CreateFormat();
  auto sample = CreateSample(format);
  void* context = CreateContext(0);

  decompression_session_->can_create = false;

  EXPECT_CALL(*this, OnError(_));

  video_toolbox_->Decode(sample, context);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

TEST_F(VideoToolboxDecompressionInterfaceTest, CompatibleFormatChange) {
  auto format0 = CreateFormat();
  auto format1 = CreateFormat();
  auto sample0 = CreateSample(format0);
  auto sample1 = CreateSample(format1);
  void* context0 = CreateContext(0);
  void* context1 = CreateContext(1);

  video_toolbox_->Decode(sample0, context0);
  video_toolbox_->Decode(sample1, context1);

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 2ul);
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 2ul);

  EXPECT_CALL(*this, OnOutput(_, context0));
  EXPECT_CALL(*this, OnOutput(_, context1));

  decompression_session_->CompleteDecode();
  decompression_session_->CompleteDecode();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

TEST_F(VideoToolboxDecompressionInterfaceTest, IncompatibleFormatChange) {
  auto format0 = CreateFormat();
  auto format1 = CreateFormat();
  auto sample0 = CreateSample(format0);
  auto sample1 = CreateSample(format1);
  void* context0 = CreateContext(0);
  void* context1 = CreateContext(1);

  // CanAcceptFormat() is only called when necessary, so this only affects the
  // second sample.
  decompression_session_->can_accept_format = false;

  video_toolbox_->Decode(sample0, context0);
  video_toolbox_->Decode(sample1, context1);

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 2ul);
  // The second decode will not be started until after the first session is
  // invalidated (which happens after the first CompleteDecode()).
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 1ul);

  EXPECT_CALL(*this, OnOutput(_, context0));
  EXPECT_CALL(*this, OnOutput(_, context1));

  decompression_session_->CompleteDecode();
  decompression_session_->CompleteDecode();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 2ul);
}

TEST_F(VideoToolboxDecompressionInterfaceTest, DecodeError_Early) {
  auto format = CreateFormat();
  auto sample = CreateSample(format);
  void* context = CreateContext(0);

  decompression_session_->can_decode_frame = false;

  EXPECT_CALL(*this, OnError(_));

  video_toolbox_->Decode(sample, context);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

TEST_F(VideoToolboxDecompressionInterfaceTest, DecodeError_Late) {
  auto format = CreateFormat();
  auto sample = CreateSample(format);
  void* context = CreateContext(0);

  video_toolbox_->Decode(sample, context);

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 1ul);
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 1ul);

  EXPECT_CALL(*this, OnError(_));

  decompression_session_->FailDecode();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_->PendingDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->ActiveDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

}  // namespace media
