// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/platform/peerconnection/stats_collecting_decoder.h"

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/make_ref_counted.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/api/video/video_frame.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/api/video_codecs/video_decoder.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"

namespace blink {

namespace {
constexpr float kMinDecodingTimeMs = 1.0f;
constexpr float kExpectedP99ProcessingTimeMs = 12.0f;
constexpr float kP99ToleranceMs = 0.5f;
const webrtc::SdpVideoFormat kFormatVp9{"VP9"};
constexpr media::VideoCodecProfile kCodecProfile =
    media::VideoCodecProfile::VP9PROFILE_PROFILE0;
constexpr int kHdWidth = 1280;
constexpr int kHdHeight = 720;
constexpr int kFullHdWidth = 1920;
constexpr int kFullHdHeight = 1080;
constexpr int kFramerate = 30;
constexpr int kFramesPerMinute = kFramerate * 60;
constexpr int kKeyframeInterval = 25;

class MockVideoFrameBuffer : public webrtc::VideoFrameBuffer {
 public:
  MockVideoFrameBuffer(int width, int height)
      : width_(width), height_(height) {}
  Type type() const override { return Type::kNative; }
  int width() const override { return width_; }
  int height() const override { return height_; }

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(width_, height_);
    webrtc::I420Buffer::SetBlack(buffer.get());
    return buffer;
  }

 private:
  int width_;
  int height_;
};

webrtc::VideoFrame CreateMockFrame(int width, int height, uint32_t timestamp) {
  return webrtc::VideoFrame::Builder()
      .set_video_frame_buffer(
          rtc::make_ref_counted<MockVideoFrameBuffer>(width, height))
      .set_rtp_timestamp(timestamp)
      .build();
}

class MockDecoder : public webrtc::VideoDecoder {
 public:
  explicit MockDecoder(bool* is_hw_accelerated)
      : is_hw_accelerated_(is_hw_accelerated) {}

  // Implementation of webrtc::VideoDecoder.
  bool Configure(const Settings& settings) override { return true; }
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    webrtc::VideoFrame video_frame =
        CreateMockFrame(input_image._encodedWidth, input_image._encodedHeight,
                        input_image.RtpTimestamp());
    callback_->Decoded(video_frame, std::nullopt, std::nullopt);
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }

  DecoderInfo GetDecoderInfo() const override {
    DecoderInfo info;
    info.is_hardware_accelerated = *is_hw_accelerated_;
    return info;
  }

 private:
  const raw_ptr<bool> is_hw_accelerated_;
  raw_ptr<webrtc::DecodedImageCallback> callback_;
};

class MockDecodedImageCallback : public webrtc::DecodedImageCallback {
 public:
  MockDecodedImageCallback(float min_decode_time_ms, float p90_decode_time_ms)
      : min_decode_time_ms_(min_decode_time_ms),
        p90_decode_time_ms_(p90_decode_time_ms) {}

  // Implementation of webrtc::DecodedImageCallback.
  int32_t Decoded(webrtc::VideoFrame& decodedImage) override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }
  void Decoded(webrtc::VideoFrame& decodedImage,
               std::optional<int32_t> decode_time_ms,
               std::optional<uint8_t> qp) override {
    // Set the processing time. Start time is set to a fixed nonzero time since
    // we're only interested in the delta.
    webrtc::Timestamp start_time = webrtc::Timestamp::Seconds(1234);
    webrtc::TimeDelta decode_time = webrtc::TimeDelta::Millis(
        frame_counter_ % 100 < 90 ? min_decode_time_ms_ : p90_decode_time_ms_);
    decodedImage.set_processing_time({start_time, start_time + decode_time});

    ++frame_counter_;
  }

 private:
  int frame_counter_{0};
  float min_decode_time_ms_;
  float p90_decode_time_ms_;
};

class StatsCollectingDecoderTest : public ::testing::Test {
 protected:
  StatsCollectingDecoderTest()
      : decoded_image_callback_(kMinDecodingTimeMs,
                                kExpectedP99ProcessingTimeMs),
        stats_decoder_(kFormatVp9,
                       std::make_unique<MockDecoder>(&is_hw_accelerated_),
                       base::BindRepeating(
                           &StatsCollectingDecoderTest::StoreProcessingStatsCB,
                           base::Unretained(this))) {
    stats_decoder_.RegisterDecodeCompleteCallback(&decoded_image_callback_);
  }

  void TearDown() override { stats_decoder_.Release(); }

  void StoreProcessingStatsCB(const StatsCollector::StatsKey& stats_key,
                              const StatsCollector::VideoStats& video_stats) {
    ++stats_callbacks_;
    last_stats_key_ = stats_key;
    last_video_stats_ = video_stats;
  }

  void CreateAndDecodeFrames(int width,
                             int height,
                             bool is_hw_accelerated,
                             int frames,
                             int key_frame_interval,
                             int frame_rate) {
    CreateAndDecodeFrames(&stats_decoder_, width, height, is_hw_accelerated,
                          frames, key_frame_interval, frame_rate);
  }

  void CreateAndDecodeFrames(StatsCollectingDecoder* decoder,
                             int width,
                             int height,
                             bool is_hw_accelerated,
                             int frames,
                             int key_frame_interval,
                             int frame_rate) {
    is_hw_accelerated_ = is_hw_accelerated;
    for (int i = 0; i < frames; ++i) {
      webrtc::EncodedImage encoded_frame;
      encoded_frame._encodedWidth = width;
      encoded_frame._encodedHeight = height;
      encoded_frame.SetRtpTimestamp(
          90000 * frame_counter /
          frame_rate);  // RTP timestamp using 90 kHz clock.
      encoded_frame._frameType = frame_counter % key_frame_interval == 0
                                     ? webrtc::VideoFrameType::kVideoFrameKey
                                     : webrtc::VideoFrameType::kVideoFrameDelta;
      ++frame_counter;
      base::TimeDelta delta = base::Milliseconds(1000 / frame_rate);
      task_environment_.AdvanceClock(delta);
      decoder->Decode(encoded_frame, /*missing_frames=*/false,
                      /*render_time_ms=*/0);
    }
  }

  // Needed to mock time.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  bool is_hw_accelerated_{false};
  MockDecodedImageCallback decoded_image_callback_;
  StatsCollectingDecoder stats_decoder_;

  uint32_t frame_counter{0};

  int stats_callbacks_{0};
  StatsCollector::StatsKey last_stats_key_;
  StatsCollector::VideoStats last_video_stats_;
};

TEST_F(StatsCollectingDecoderTest, StoreProcessingStatsCallbackHdSw) {
  // P99 not meaningful for less than 100 frames.
  constexpr int kMinimumNumberOfFrames = 100;
  constexpr int kFrames = 200;
  EXPECT_EQ(stats_callbacks_, 0);
  CreateAndDecodeFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                        kFrames, kKeyframeInterval, kFramerate);
  // Verify that there's been one stats callback and that the numbers are
  // reasonable.
  EXPECT_EQ(stats_callbacks_, 1);
  EXPECT_TRUE(last_stats_key_.is_decode);
  EXPECT_EQ(last_stats_key_.codec_profile, kCodecProfile);
  EXPECT_EQ(last_stats_key_.pixel_size, kHdWidth * kHdHeight);
  EXPECT_FALSE(last_stats_key_.hw_accelerated);
  EXPECT_GE(last_video_stats_.frame_count, kMinimumNumberOfFrames);
  EXPECT_LT(last_video_stats_.frame_count, kFrames);
  EXPECT_NEAR(last_video_stats_.key_frame_count,
              last_video_stats_.frame_count / kKeyframeInterval, 1);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);
}

TEST_F(StatsCollectingDecoderTest, StoreProcessingStatsCallbackFullHdHw) {
  // P99 not meaningful for less than 100 frames.
  constexpr int kMinimumNumberOfFrames = 100;
  constexpr int kFrames = 200;
  EXPECT_EQ(stats_callbacks_, 0);
  CreateAndDecodeFrames(kFullHdWidth, kFullHdHeight, /*is_hw_accelerated=*/true,
                        kFrames, kKeyframeInterval, kFramerate);
  // Verify that there's been one stats callback and that the numbers are
  // reasonable.
  EXPECT_EQ(stats_callbacks_, 1);
  EXPECT_TRUE(last_stats_key_.is_decode);
  EXPECT_EQ(last_stats_key_.codec_profile, kCodecProfile);
  EXPECT_EQ(last_stats_key_.pixel_size, kFullHdWidth * kFullHdHeight);
  EXPECT_TRUE(last_stats_key_.hw_accelerated);
  EXPECT_GE(last_video_stats_.frame_count, kMinimumNumberOfFrames);
  EXPECT_LT(last_video_stats_.frame_count, kFrames);
  EXPECT_NEAR(last_video_stats_.key_frame_count,
              last_video_stats_.frame_count / kKeyframeInterval, 1);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);
}

TEST_F(StatsCollectingDecoderTest,
       CollectionStopsIfThereAreMultipleDecodersActive) {
  constexpr int kMinutesToRun = 10;
  EXPECT_EQ(stats_callbacks_, 0);
  CreateAndDecodeFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                        kFramesPerMinute, kKeyframeInterval, kFramerate);
  int last_stats_callbacks = stats_callbacks_;

  // Create another decoder.
  MockDecodedImageCallback second_decoded_image_callback(
      kMinDecodingTimeMs, kExpectedP99ProcessingTimeMs);
  bool second_decoder_is_hw_accelerated = false;
  StatsCollectingDecoder second_stats_decoder(
      kFormatVp9,
      std::make_unique<MockDecoder>(&second_decoder_is_hw_accelerated),
      base::DoNothing());
  second_stats_decoder.RegisterDecodeCompleteCallback(
      &second_decoded_image_callback);
  // Decode one frame to make it an active decoder.
  CreateAndDecodeFrames(&second_stats_decoder, kHdWidth, kHdHeight,
                        /*is_hw_accelerated=*/false, 1, kKeyframeInterval,
                        kFramerate);

  // Run for a few minutes and verify that no new callbacks are made.
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    CreateAndDecodeFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                          kFramesPerMinute, kKeyframeInterval, kFramerate);
    // The expectation could be relaxed to allow for one callback to happen.
    EXPECT_EQ(stats_callbacks_, last_stats_callbacks);
  }
  second_stats_decoder.Release();
}

TEST_F(StatsCollectingDecoderTest, CollectionStartsAgainIfOneDecoderIsActive) {
  constexpr int kMinutesToRun = 10;
  EXPECT_EQ(stats_callbacks_, 0);

  // Create another decoder.
  MockDecodedImageCallback second_decoded_image_callback(
      kMinDecodingTimeMs, kExpectedP99ProcessingTimeMs);
  bool second_decoder_is_hw_accelerated = false;
  StatsCollectingDecoder second_stats_decoder(
      kFormatVp9,
      std::make_unique<MockDecoder>(&second_decoder_is_hw_accelerated),
      base::DoNothing());
  second_stats_decoder.RegisterDecodeCompleteCallback(
      &second_decoded_image_callback);
  // Decode one frame to make it an active decoder.
  CreateAndDecodeFrames(&second_stats_decoder, kHdWidth, kHdHeight,
                        /*is_hw_accelerated=*/false, 1, kKeyframeInterval,
                        kFramerate);

  int last_stats_callbacks = stats_callbacks_;
  // Run for a few minutes and verify that no callbacks are made.
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    CreateAndDecodeFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                          kFramesPerMinute, kKeyframeInterval, kFramerate);
    EXPECT_EQ(stats_callbacks_, last_stats_callbacks);
  }

  // Release the other decoder and verify that collection starts again.
  second_stats_decoder.Release();

  // Run for a few minutes and verify that no callbacks are made.
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    CreateAndDecodeFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                          kFramesPerMinute, kKeyframeInterval, kFramerate);
    EXPECT_GT(stats_callbacks_, last_stats_callbacks);
    last_stats_callbacks = stats_callbacks_;
  }
}

TEST_F(StatsCollectingDecoderTest, NoCollectionAfter40000Frames) {
  constexpr int kMinutesToRun = 10;
  constexpr int kFrames = 40000;
  EXPECT_EQ(stats_callbacks_, 0);
  CreateAndDecodeFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                        kFrames, kKeyframeInterval, kFramerate);
  EXPECT_GT(stats_callbacks_, 0);
  int last_stats_callbacks = stats_callbacks_;

  // Run for a few minutes and verify that no new callbacks are made.
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    CreateAndDecodeFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                          kFramesPerMinute, kKeyframeInterval, kFramerate);
    // The expectation could be relaxed to allow for one callback to happen.
    EXPECT_EQ(stats_callbacks_, last_stats_callbacks);
  }
}

}  // namespace
}  // namespace blink
