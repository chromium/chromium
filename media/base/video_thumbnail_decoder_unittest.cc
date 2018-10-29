// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_thumbnail_decoder.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        kCodecVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420, COLOR_SPACE_UNSPECIFIED,
        VIDEO_ROTATION_0, gfx::Size(1, 1), gfx::Rect(1, 1), gfx::Size(1, 1),
        EmptyExtraData(), Unencrypted());

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
  const scoped_refptr<VideoFrame>& frame() { return frame_; }

 private:
  void OnFrameDecoded(scoped_refptr<VideoFrame> frame) {
    frame_ = std::move(frame);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

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
  EXPECT_CALL(*mock_video_decoder(), Initialize(_, _, _, _, _, _))
      .WillOnce(DoAll(RunCallback<3>(true), RunCallback<4>(expected_frame)));
  EXPECT_CALL(*mock_video_decoder(), Decode(_, _))
      .Times(2)
      .WillRepeatedly(RunCallback<1>(DecodeStatus::OK));

  Start();
  EXPECT_TRUE(frame());
}

// No output video frame when decoder failed to initialize.
TEST_F(VideoThumbnailDecoderTest, InitializationFailed) {
  auto expected_frame = CreateFrame();
  EXPECT_CALL(*mock_video_decoder(), Initialize(_, _, _, _, _, _))
      .WillOnce(RunCallback<3>(false));

  Start();
  EXPECT_FALSE(frame());
}

// No output video frame when decoder failed to decode.
TEST_F(VideoThumbnailDecoderTest, DecodingFailed) {
  auto expected_frame = CreateFrame();
  EXPECT_CALL(*mock_video_decoder(), Initialize(_, _, _, _, _, _))
      .WillOnce(RunCallback<3>(true));
  EXPECT_CALL(*mock_video_decoder(), Decode(_, _))
      .WillOnce(RunCallback<1>(DecodeStatus::DECODE_ERROR));

  Start();
  EXPECT_FALSE(frame());
}

}  // namespace
}  // namespace media
