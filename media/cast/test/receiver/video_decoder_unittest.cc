// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "media/base/mock_filters.h"
#include "media/cast/cast_config.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/encoding/vpx_encoder.h"
#include "media/cast/test/receiver/video_decoder.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/standalone_cast_environment.h"
#include "media/cast/test/utility/video_utility.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"

namespace media {
namespace cast {

namespace {

const int kStartingWidth = 360;
const int kStartingHeight = 240;
const int kFrameRate = 10;

FrameSenderConfig GetVideoSenderConfigForTest() {
  FrameSenderConfig config = GetDefaultVideoSenderConfig();
  config.max_frame_rate = kFrameRate;
  return config;
}

}  // namespace

class VideoDecoderTest : public ::testing::TestWithParam<Codec> {
 public:
  VideoDecoderTest()
      : cast_environment_(new StandaloneCastEnvironment()),
        vp8_encoder_(
            GetVideoSenderConfigForTest(),
            std::make_unique<media::MockVideoEncoderMetricsProvider>()),
        cond_(&lock_) {
    vp8_encoder_.Initialize();
  }

  VideoDecoderTest(const VideoDecoderTest&) = delete;
  VideoDecoderTest& operator=(const VideoDecoderTest&) = delete;

  virtual ~VideoDecoderTest() {
    // Make sure all threads have stopped before the environment goes away.
    cast_environment_->Shutdown();
  }

 protected:
  void SetUp() final {
    video_decoder_ =
        std::make_unique<VideoDecoder>(cast_environment_, GetParam());
    CHECK_EQ(STATUS_INITIALIZED, video_decoder_->InitializationResult());

    next_frame_size_ = gfx::Size(kStartingWidth, kStartingHeight);
    next_frame_timestamp_ = base::TimeDelta();
    last_frame_id_ = FrameId::first();
    seen_a_decoded_frame_ = false;

    total_video_frames_feed_in_ = 0;
    total_video_frames_decoded_ = 0;
  }

  void SetNextFrameSize(const gfx::Size& size) { next_frame_size_ = size; }

  // Called from the unit test thread to create another EncodedFrame and push it
  // into the decoding pipeline.
  void FeedMoreVideo(int num_dropped_frames) {
    DCHECK(!cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

    // Prepare a simulated VideoFrame to feed into the VideoEncoder.
    const scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateFrame(
        PIXEL_FORMAT_I420, next_frame_size_, gfx::Rect(next_frame_size_),
        next_frame_size_, next_frame_timestamp_);
    const base::TimeTicks reference_time =
        base::TimeTicks::UnixEpoch() + next_frame_timestamp_;
    next_frame_timestamp_ += base::Seconds(1) / kFrameRate;
    PopulateVideoFrame(video_frame.get(), 0);

    // Encode |frame| into |encoded_frame->data|.
    std::unique_ptr<SenderEncodedFrame> encoded_frame(new SenderEncodedFrame());
    // Test only supports VP8, currently.
    CHECK_EQ(Codec::kVideoVp8, GetParam());
    vp8_encoder_.Encode(video_frame, reference_time, encoded_frame.get());
    // Rewrite frame IDs for testing purposes.
    encoded_frame->frame_id = last_frame_id_ + 1 + num_dropped_frames;
    if (encoded_frame->dependency ==
        openscreen::cast::EncodedFrame::Dependency::kKeyFrame)
      encoded_frame->referenced_frame_id = encoded_frame->frame_id;
    else
      encoded_frame->referenced_frame_id = encoded_frame->frame_id - 1;
    last_frame_id_ = encoded_frame->frame_id;
    ASSERT_EQ(reference_time, encoded_frame->reference_time);

    ++total_video_frames_feed_in_;

    // Post a task to decode the encoded frame.
    cast_environment_->PostTask(
        CastEnvironment::MAIN, FROM_HERE,
        base::BindOnce(&VideoDecoder::DecodeFrame,
                       base::Unretained(video_decoder_.get()),
                       std::move(encoded_frame),
                       base::BindRepeating(&VideoDecoderTest::OnDecodedFrame,
                                           base::Unretained(this), video_frame,
                                           num_dropped_frames == 0)));
  }

  // Blocks the caller until all video that has been feed in has been decoded.
  void WaitForAllVideoToBeDecoded() {
    DCHECK(!cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
    base::AutoLock auto_lock(lock_);
    while (total_video_frames_decoded_ < total_video_frames_feed_in_)
      cond_.Wait();
    EXPECT_EQ(total_video_frames_feed_in_, total_video_frames_decoded_);
  }

 private:
  // Called by |vp8_decoder_| to deliver each frame of decoded video.
  void OnDecodedFrame(scoped_refptr<VideoFrame> expected_video_frame,
                      bool should_be_continuous,
                      scoped_refptr<VideoFrame> video_frame,
                      bool is_continuous) {
    DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

    // A NULL |video_frame| indicates a decode error, which we don't expect.
    ASSERT_TRUE(video_frame);

    // Did the decoder detect whether frames were dropped?
    EXPECT_EQ(should_be_continuous, is_continuous);

    // Does the video data seem to be intact?
    EXPECT_EQ(expected_video_frame->coded_size().width(),
              video_frame->coded_size().width());
    EXPECT_EQ(expected_video_frame->coded_size().height(),
              video_frame->coded_size().height());
    EXPECT_LT(40.0, I420PSNR(*expected_video_frame, *video_frame));

    // Signal the main test thread that more video was decoded.
    base::AutoLock auto_lock(lock_);
    ++total_video_frames_decoded_;
    cond_.Signal();
  }

  const scoped_refptr<StandaloneCastEnvironment> cast_environment_;
  std::unique_ptr<VideoDecoder> video_decoder_;
  gfx::Size next_frame_size_;
  base::TimeDelta next_frame_timestamp_;
  FrameId last_frame_id_;
  bool seen_a_decoded_frame_;

  VpxEncoder vp8_encoder_;

  // Unlike |total_video_frames_decoded_|, this is only read/written on a single
  // thread.
  int total_video_frames_feed_in_;

  base::Lock lock_;
  base::ConditionVariable cond_;
  int total_video_frames_decoded_;  // Protected by |lock_|.
};

TEST_P(VideoDecoderTest, DecodesFrames) {
  const int kNumFrames = 3;
  for (int i = 0; i < kNumFrames; ++i)
    FeedMoreVideo(0);
  WaitForAllVideoToBeDecoded();
}

TEST_P(VideoDecoderTest, RecoversFromDroppedFrames) {
  // Feed 20 frames and expect 20 to be decoded.  At random points, drop one or
  // more frames.
  FeedMoreVideo(0);
  FeedMoreVideo(2);  // Two frames dropped.
  FeedMoreVideo(0);
  FeedMoreVideo(0);
  FeedMoreVideo(1);  // One frame dropped.
  FeedMoreVideo(0);
  FeedMoreVideo(0);
  FeedMoreVideo(0);
  FeedMoreVideo(1);  // One frame dropped.
  FeedMoreVideo(0);
  FeedMoreVideo(0);
  FeedMoreVideo(0);
  FeedMoreVideo(0);
  FeedMoreVideo(3);  // Three frames dropped.
  FeedMoreVideo(0);
  FeedMoreVideo(0);
  FeedMoreVideo(10);  // Ten frames dropped.
  FeedMoreVideo(0);
  FeedMoreVideo(1);  // One frame dropped.
  FeedMoreVideo(0);
  WaitForAllVideoToBeDecoded();
}

TEST_P(VideoDecoderTest, DecodesFramesOfVaryingSizes) {
  std::vector<gfx::Size> frame_sizes;
  frame_sizes.push_back(gfx::Size(128, 72));
  frame_sizes.push_back(gfx::Size(64, 36));    // Shrink both dimensions.
  frame_sizes.push_back(gfx::Size(30, 20));    // Shrink both dimensions again.
  frame_sizes.push_back(gfx::Size(20, 30));    // Same area.
  frame_sizes.push_back(gfx::Size(60, 40));    // Grow both dimensions.
  frame_sizes.push_back(gfx::Size(58, 40));    // Shrink only one dimension.
  frame_sizes.push_back(gfx::Size(58, 38));    // Shrink the other dimension.
  frame_sizes.push_back(gfx::Size(32, 18));    // Shrink both dimensions again.
  frame_sizes.push_back(gfx::Size(34, 18));    // Grow only one dimension.
  frame_sizes.push_back(gfx::Size(34, 20));    // Grow the other dimension.
  frame_sizes.push_back(gfx::Size(192, 108));  // Grow both dimensions again.

  // Encode one frame at each size.
  for (const auto& frame_size : frame_sizes) {
    SetNextFrameSize(frame_size);
    FeedMoreVideo(0);
  }

  // Encode 3 frames at each size.
  for (const auto& frame_size : frame_sizes) {
    SetNextFrameSize(frame_size);
    const int kNumFrames = 3;
    for (int i = 0; i < kNumFrames; ++i)
      FeedMoreVideo(0);
  }

  WaitForAllVideoToBeDecoded();
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoDecoderTest,
                         ::testing::Values(Codec::kVideoVp8));

}  // namespace cast
}  // namespace media
