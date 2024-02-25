// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/decoder_status.h"
#include "media/base/media_util.h"
#include "media/gpu/mac/video_toolbox_decompression_metadata.h"
#include "media/gpu/mac/video_toolbox_decompression_session_manager.h"
#include "media/gpu/mac/video_toolbox_decompression_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {

namespace {

MATCHER_P(MetadataEq, id, "") {
  return arg->timestamp == base::Microseconds(id);
}

std::unique_ptr<VideoToolboxDecodeMetadata> CreateMetadata(int id) {
  auto metadata = std::make_unique<VideoToolboxDecodeMetadata>();
  metadata->timestamp = base::Microseconds(id);
  return metadata;
}

base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> CreateFormat() {
  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  OSStatus status =
      CMFormatDescriptionCreate(kCFAllocatorDefault, kCMMediaType_Video, 'test',
                                nullptr, format.InitializeInto());
  CHECK_EQ(status, noErr);
  return format;
}

base::apple::ScopedCFTypeRef<CMSampleBufferRef> CreateSample(
    CMFormatDescriptionRef format) {
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  OSStatus status = CMSampleBufferCreate(
      kCFAllocatorDefault, nullptr, true, nullptr, nullptr, format, 0, 0,
      nullptr, 0, nullptr, sample.InitializeInto());
  CHECK_EQ(status, noErr);
  return sample;
}

base::apple::ScopedCFTypeRef<CVImageBufferRef> CreateImage() {
  base::apple::ScopedCFTypeRef<CVImageBufferRef> image;
  OSStatus status =
      CVPixelBufferCreate(kCFAllocatorDefault, /*width=*/16, /*height=*/16,
                          kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                          nullptr, image.InitializeInto());
  CHECK_EQ(status, noErr);
  return image;
}

class FakeDecompressionSession : public VideoToolboxDecompressionSession {
 public:
  explicit FakeDecompressionSession(
      VideoToolboxDecompressionSessionImpl::OutputCB output_cb)
      : output_cb_(std::move(output_cb)) {}

  ~FakeDecompressionSession() override = default;

  bool Create(CMFormatDescriptionRef format,
              CFDictionaryRef decoder_config,
              CFDictionaryRef image_config) override {
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

  bool DecodeFrame(CMSampleBufferRef sample, uintptr_t context) override {
    if (can_decode_frame) {
      pending_decodes_.push(context);
    }
    return can_decode_frame;
  }

  // Output the first pending decode, with an image.
  void CompleteDecode() {
    CHECK(!pending_decodes_.empty());

    uintptr_t context = pending_decodes_.front();
    OSStatus status = noErr;
    VTDecodeInfoFlags flags = 0;
    base::apple::ScopedCFTypeRef<CVImageBufferRef> image = CreateImage();

    pending_decodes_.pop();
    output_cb_.Run(context, status, flags, std::move(image));
  }

  // Output the first pending decode, with an error code.
  void FailDecode() {
    CHECK(!pending_decodes_.empty());

    uintptr_t context = pending_decodes_.front();
    OSStatus status = -1;
    VTDecodeInfoFlags flags = 0;
    base::apple::ScopedCFTypeRef<CVImageBufferRef> image;

    pending_decodes_.pop();
    output_cb_.Run(context, status, flags, std::move(image));
  }

  size_t NumDecodes() { return pending_decodes_.size(); }

  bool can_create = true;
  bool can_accept_format = true;
  bool can_decode_frame = true;

  size_t creations = 0;

 private:
  VideoToolboxDecompressionSessionImpl::OutputCB output_cb_;
  bool valid_ = false;
  base::queue<uintptr_t> pending_decodes_;
};

}  // namespace

class VideoToolboxDecompressionSessionManagerTest : public testing::Test {
 public:
  VideoToolboxDecompressionSessionManagerTest() {
    video_toolbox_.SetDecompressionSessionForTesting(
        base::WrapUnique(decompression_session_.get()));
  }

  ~VideoToolboxDecompressionSessionManagerTest() override = default;

 protected:
  MOCK_METHOD1(OnError, void(DecoderStatus));
  MOCK_METHOD2(OnOutput,
               void(base::apple::ScopedCFTypeRef<CVImageBufferRef>,
                    std::unique_ptr<VideoToolboxDecodeMetadata>));

  base::test::TaskEnvironment task_environment_;
  VideoToolboxDecompressionSessionManager video_toolbox_{
      task_environment_.GetMainThreadTaskRunner(),
      std::make_unique<NullMediaLog>(),
      base::BindRepeating(&VideoToolboxDecompressionSessionManagerTest::OnOutput,
                          base::Unretained(this)),
      base::BindOnce(&VideoToolboxDecompressionSessionManagerTest::OnError,
                     base::Unretained(this))};
  raw_ptr<FakeDecompressionSession> decompression_session_{
      new FakeDecompressionSession(
          base::BindRepeating(&VideoToolboxDecompressionSessionManager::OnOutput,
                              base::Unretained(&video_toolbox_)))};
};

TEST_F(VideoToolboxDecompressionSessionManagerTest, Construct) {}

TEST_F(VideoToolboxDecompressionSessionManagerTest, Decode) {
  auto format = CreateFormat();
  auto sample = CreateSample(format.get());
  auto metadata = CreateMetadata(0);

  video_toolbox_.Decode(sample, std::move(metadata));

  EXPECT_EQ(video_toolbox_.NumDecodes(), 1ul);
  EXPECT_EQ(decompression_session_->NumDecodes(), 1ul);

  EXPECT_CALL(*this, OnOutput(_, MetadataEq(0)));

  decompression_session_->CompleteDecode();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_.NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

TEST_F(VideoToolboxDecompressionSessionManagerTest, CreateFailure) {
  auto format = CreateFormat();
  auto sample = CreateSample(format.get());
  auto metadata = CreateMetadata(0);

  decompression_session_->can_create = false;

  EXPECT_CALL(*this, OnError(_));

  video_toolbox_.Decode(sample, std::move(metadata));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_.NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

TEST_F(VideoToolboxDecompressionSessionManagerTest, CompatibleFormatChange) {
  auto format0 = CreateFormat();
  auto format1 = CreateFormat();
  auto sample0 = CreateSample(format0.get());
  auto sample1 = CreateSample(format1.get());
  auto metadata0 = CreateMetadata(0);
  auto metadata1 = CreateMetadata(1);

  video_toolbox_.Decode(sample0, std::move(metadata0));
  video_toolbox_.Decode(sample1, std::move(metadata1));

  EXPECT_EQ(video_toolbox_.NumDecodes(), 2ul);
  EXPECT_EQ(decompression_session_->NumDecodes(), 2ul);

  EXPECT_CALL(*this, OnOutput(_, MetadataEq(0)));
  EXPECT_CALL(*this, OnOutput(_, MetadataEq(1)));

  decompression_session_->CompleteDecode();
  decompression_session_->CompleteDecode();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_.NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

TEST_F(VideoToolboxDecompressionSessionManagerTest, IncompatibleFormatChange) {
  auto format0 = CreateFormat();
  auto format1 = CreateFormat();
  auto sample0 = CreateSample(format0.get());
  auto sample1 = CreateSample(format1.get());
  auto metadata0 = CreateMetadata(0);
  auto metadata1 = CreateMetadata(1);

  // CanAcceptFormat() is only called when necessary, so this only affects the
  // second sample.
  decompression_session_->can_accept_format = false;

  video_toolbox_.Decode(sample0, std::move(metadata0));
  video_toolbox_.Decode(sample1, std::move(metadata1));

  EXPECT_EQ(video_toolbox_.NumDecodes(), 2ul);
  // The second decode will not be started until after the first session is
  // invalidated (which happens after the first CompleteDecode()).
  EXPECT_EQ(decompression_session_->NumDecodes(), 1ul);

  EXPECT_CALL(*this, OnOutput(_, MetadataEq(0)));
  EXPECT_CALL(*this, OnOutput(_, MetadataEq(1)));

  decompression_session_->CompleteDecode();
  decompression_session_->CompleteDecode();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_.NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 2ul);
}

TEST_F(VideoToolboxDecompressionSessionManagerTest, DecodeError_Early) {
  auto format = CreateFormat();
  auto sample = CreateSample(format.get());
  auto metadata = CreateMetadata(0);

  decompression_session_->can_decode_frame = false;

  EXPECT_CALL(*this, OnError(_));

  video_toolbox_.Decode(sample, std::move(metadata));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_.NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

TEST_F(VideoToolboxDecompressionSessionManagerTest, DecodeError_Late) {
  auto format = CreateFormat();
  auto sample = CreateSample(format.get());
  auto metadata = CreateMetadata(0);

  video_toolbox_.Decode(sample, std::move(metadata));

  EXPECT_EQ(video_toolbox_.NumDecodes(), 1ul);
  EXPECT_EQ(decompression_session_->NumDecodes(), 1ul);

  EXPECT_CALL(*this, OnError(_));

  decompression_session_->FailDecode();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(video_toolbox_.NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->NumDecodes(), 0ul);
  EXPECT_EQ(decompression_session_->creations, 1ul);
}

}  // namespace media
