// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_thumbnail_decoder.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;

namespace media {
namespace {

class VideoThumbnailDecoderTest : public testing::Test {
 public:
  VideoThumbnailDecoderTest() {}
  ~VideoThumbnailDecoderTest() override {}

 protected:
  void SetUp() override {
    auto mock_video_decoder = std::make_unique<MockVideoDecoder>();
    mock_video_decoder_ = mock_video_decoder.get();
    VideoDecoderConfig valid_config(
        kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
        VideoColorSpace(), kNoTransformation, gfx::Size(1, 1), gfx::Rect(1, 1),
        gfx::Size(1, 1), EmptyExtraData(), EncryptionScheme::kUnencrypted);

    thumbnail_decoder_ = std::make_unique<VideoThumbnailDecoder>(
        std::move(mock_video_decoder), valid_config, std::vector<uint8_t>{0u});
  }

  void TearDown() override {}

  void Start() {
    thumbnail_decoder_->Start(base::BindOnce(
        &VideoThumbnailDecoderTest::OnFrameDecoded, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<VideoFrame> CreateFrame() {
    return VideoFrame::CreateZeroInitializedFrame(
        VideoPixelFormat::PIXEL_FORMAT_I420, gfx::Size(1, 1), gfx::Rect(1, 1),
        gfx::Size(1, 1), base::TimeDelta());
  }

  VideoThumbnailDecoder* thumbnail_decoder() {
    return thumbnail_decoder_.get();
  }
  MockVideoDecoder* mock_video_decoder() { return mock_video_decoder_; }
  scoped_refptr<VideoFrame> frame() { return frame_; }

 private:
  void OnFrameDecoded(scoped_refptr<VideoFrame> frame) {
    frame_ = std::move(frame);
  }

  base::test::TaskEnvironment task_environment_;

  MockVideoDecoder* mock_video_decoder_;
  std::unique_ptr<VideoThumbnailDecoder> thumbnail_decoder_;

  // The video frame returned from the thumbnail decoder.
  scoped_refptr<VideoFrame> frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoThumbnailDecoderTest);
};

// Verifies a video frame can be delivered when decoder successfully created
// the video frame.
TEST_F(VideoThumbnailDecoderTest, Success) {
  auto expected_frame = CreateFrame();
  EXPECT_CALL(*mock_video_decoder(), Initialize_(_, _, _, _, _, _))
      .WillOnce(
          DoAll(RunOnceCallback<3>(true), RunCallback<4>(expected_frame)));
  EXPECT_CALL(*mock_video_decoder(), Decode_(_, _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<1>(DecodeStatus::OK));

  Start();
  EXPECT_TRUE(frame());
}

// No output video frame when decoder failed to initialize.
TEST_F(VideoThumbnailDecoderTest, InitializationFailed) {
  auto expected_frame = CreateFrame();
  EXPECT_CALL(*mock_video_decoder(), Initialize_(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<3>(false));

  Start();
  EXPECT_FALSE(frame());
}

// No output video frame when decoder failed to decode.
TEST_F(VideoThumbnailDecoderTest, DecodingFailed) {
  auto expected_frame = CreateFrame();
  EXPECT_CALL(*mock_video_decoder(), Initialize_(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(*mock_video_decoder(), Decode_(_, _))
      .WillOnce(RunOnceCallback<1>(DecodeStatus::DECODE_ERROR));

  Start();
  EXPECT_FALSE(frame());
}

}  // namespace
}  // namespace media
