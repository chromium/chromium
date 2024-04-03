// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/platform/peerconnection/stats_collecting_encoder.h"

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/make_ref_counted.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/api/video/video_bitrate_allocation.h"
#include "third_party/webrtc/api/video/video_frame.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"

namespace blink {

namespace {
constexpr float kMinEncodingTimeMs = 1.0f;
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

class FakeVideoFrameBuffer : public webrtc::VideoFrameBuffer {
 public:
  FakeVideoFrameBuffer(int width, int height)
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
          rtc::make_ref_counted<FakeVideoFrameBuffer>(width, height))
      .set_rtp_timestamp(timestamp)
      .build();
}

class MockEncoder : public webrtc::VideoEncoder {
 public:
  explicit MockEncoder(base::test::TaskEnvironment* task_environment,
                       int* spatial_layers,
                       bool* is_hw_accelerated)
      : task_environment_(task_environment),
        spatial_layers_(spatial_layers),
        is_hw_accelerated_(is_hw_accelerated) {}

  // Implementation of webrtc::VideoEncoder.
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t Encode(
      const webrtc::VideoFrame& frame,
      const std::vector<webrtc::VideoFrameType>* frame_types) override {
    webrtc::EncodedImage encoded_frame;
    encoded_frame.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_frame._frameType = frame_types && !frame_types->empty()
                                   ? frame_types->at(0)
                                   : webrtc::VideoFrameType::kVideoFrameDelta;

    base::TimeDelta encode_time = base::Milliseconds(
        frame_counter_++ % 100 < 90 ? kMinEncodingTimeMs
                                    : kExpectedP99ProcessingTimeMs);
    task_environment_->AdvanceClock(encode_time);
    for (int i = 0; i < *spatial_layers_; ++i) {
      encoded_frame._encodedWidth =
          frame.width() / (1 << (*spatial_layers_ - i - 1));
      encoded_frame._encodedHeight =
          frame.height() / (1 << (*spatial_layers_ - i - 1));
      encoded_frame.SetSpatialIndex(i);
      callback_->OnEncodedImage(encoded_frame, /*codec_specific_info=*/nullptr);
    }

    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }
  EncoderInfo GetEncoderInfo() const override {
    EncoderInfo info;
    info.is_hardware_accelerated = *is_hw_accelerated_;
    return info;
  }

  MOCK_METHOD1(SetFecControllerOverride,
               void(webrtc::FecControllerOverride* fec_controller_override));
  MOCK_METHOD2(InitEncode,
               int(const webrtc::VideoCodec* codec_settings,
                   const webrtc::VideoEncoder::Settings& settings));
  MOCK_METHOD1(SetRates, void(const RateControlParameters& parameters));
  MOCK_METHOD1(OnPacketLossRateUpdate, void(float packet_loss_rate));
  MOCK_METHOD1(OnRttUpdate, void(int64_t rtt_ms));
  MOCK_METHOD1(OnLossNotification,
               void(const LossNotification& loss_notification));

 private:
  int frame_counter_ = 0;
  raw_ptr<base::test::TaskEnvironment> task_environment_;
  raw_ptr<int> spatial_layers_;
  const raw_ptr<bool> is_hw_accelerated_;
  raw_ptr<webrtc::EncodedImageCallback> callback_;
};

class FakeEncodedImageCallback : public webrtc::EncodedImageCallback {
 public:
  Result OnEncodedImage(
      const webrtc::EncodedImage& encoded_image,
      const webrtc::CodecSpecificInfo* codec_specific_info) override {
    ++frame_counter_;
    return {Result::OK, encoded_image.RtpTimestamp()};
  }
  void OnDroppedFrame(DropReason reason) override { ; }
  int get_frame_counter() const { return frame_counter_; }

 private:
  int frame_counter_ = 0;
};

class StatsCollectingEncoderTest : public ::testing::Test {
 public:
  StatsCollectingEncoderTest()
      : internal_encoder_(new MockEncoder(&task_environment_,
                                          &spatial_layers_,
                                          &is_hw_accelerated_)),
        stats_encoder_(kFormatVp9,
                       std::unique_ptr<MockEncoder>(internal_encoder_),
                       base::BindRepeating(
                           &StatsCollectingEncoderTest::StoreProcessingStatsCB,
                           base::Unretained(this))) {
    stats_encoder_.RegisterEncodeCompleteCallback(&encoded_image_callback_);
  }

  void TearDown() override {
    internal_encoder_ = nullptr;
    stats_encoder_.Release();
  }

  void StoreProcessingStatsCB(const StatsCollector::StatsKey& stats_key,
                              const StatsCollector::VideoStats& video_stats) {
    ++stats_callbacks_;
    last_stats_key_ = stats_key;
    last_video_stats_ = video_stats;
  }

  void CreateAndEncodeFrames(int width,
                             int height,
                             int spatial_layers,
                             bool is_hw_accelerated,
                             int frames,
                             int key_frame_interval,
                             int frame_rate) {
    CreateAndEncodeFrames(&stats_encoder_, width, height, spatial_layers,
                          is_hw_accelerated, frames, key_frame_interval,
                          frame_rate);
  }

  void CreateAndEncodeFrames(StatsCollectingEncoder* encoder,
                             int width,
                             int height,
                             int spatial_layers,
                             bool is_hw_accelerated,
                             int frames,
                             int key_frame_interval,
                             int frame_rate) {
    spatial_layers_ = spatial_layers;
    is_hw_accelerated_ = is_hw_accelerated;
    base::TimeDelta frame_interval = base::Microseconds(1000000 / frame_rate);
    for (int i = 0; i < frames; ++i) {
      base::TimeTicks begin_encode = base::TimeTicks::Now();
      webrtc::VideoFrame video_frame =
          CreateMockFrame(width, height, 90000 * frame_counter_ / frame_rate);
      std::vector<webrtc::VideoFrameType> video_frame_types = {
          frame_counter_ % key_frame_interval == 0
              ? webrtc::VideoFrameType::kVideoFrameKey
              : webrtc::VideoFrameType::kVideoFrameDelta};
      ++frame_counter_;

      encoder->Encode(video_frame, &video_frame_types);
      base::TimeTicks end_encode = base::TimeTicks::Now();
      if (begin_encode + frame_interval > end_encode) {
        task_environment_.AdvanceClock(frame_interval + begin_encode -
                                       end_encode);
      }
    }
  }

 protected:
  // Needed to mock time.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  int spatial_layers_{1};
  bool is_hw_accelerated_{false};
  FakeEncodedImageCallback encoded_image_callback_;
  raw_ptr<MockEncoder> internal_encoder_;
  StatsCollectingEncoder stats_encoder_;

  uint32_t frame_counter_{0};

  int stats_callbacks_{0};
  StatsCollector::StatsKey last_stats_key_;
  StatsCollector::VideoStats last_video_stats_;
};

TEST_F(StatsCollectingEncoderTest, StoreProcessingStatsCallbackHdSw) {
  // P99 not meaningful for less than 100 frames.
  constexpr int kMinimumNumberOfFrames = 100;
  constexpr int kFrames = 200;
  EXPECT_EQ(stats_callbacks_, 0);
  CreateAndEncodeFrames(kHdWidth, kHdHeight, /*spatial_layers=*/1,
                        /*is_hw_accelerated=*/false, kFrames, kKeyframeInterval,
                        kFramerate);
  // Verify that there's been one stats callback and that the numbers are
  // reasonable.
  EXPECT_EQ(stats_callbacks_, 1);
  EXPECT_FALSE(last_stats_key_.is_decode);
  EXPECT_EQ(last_stats_key_.codec_profile, kCodecProfile);
  EXPECT_EQ(last_stats_key_.pixel_size, kHdWidth * kHdHeight);
  EXPECT_FALSE(last_stats_key_.hw_accelerated);
  EXPECT_GE(last_video_stats_.frame_count, kMinimumNumberOfFrames);
  EXPECT_LT(last_video_stats_.frame_count, kFrames);
  EXPECT_NEAR(last_video_stats_.key_frame_count,
              last_video_stats_.frame_count / kKeyframeInterval, 1);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);
  EXPECT_EQ(encoded_image_callback_.get_frame_counter(), kFrames);
}

TEST_F(StatsCollectingEncoderTest, StoreProcessingStatsCallbackFullHdHw) {
  // P99 not meaningful for less than 100 frames.
  constexpr int kMinimumNumberOfFrames = 100;
  constexpr int kFrames = 200;
  EXPECT_EQ(stats_callbacks_, 0);
  CreateAndEncodeFrames(kFullHdWidth, kFullHdHeight, /*spatial_layers=*/1,
                        /*is_hw_accelerated=*/true, kFrames, kKeyframeInterval,
                        kFramerate);
  // Verify that there's been one stats callback and that the numbers are
  // reasonable.
  EXPECT_EQ(stats_callbacks_, 1);
  EXPECT_FALSE(last_stats_key_.is_decode);
  EXPECT_EQ(last_stats_key_.codec_profile, kCodecProfile);
  EXPECT_EQ(last_stats_key_.pixel_size, kFullHdWidth * kFullHdHeight);
  EXPECT_TRUE(last_stats_key_.hw_accelerated);
  EXPECT_GE(last_video_stats_.frame_count, kMinimumNumberOfFrames);
  EXPECT_LT(last_video_stats_.frame_count, kFrames);
  EXPECT_NEAR(last_video_stats_.key_frame_count,
              last_video_stats_.frame_count / kKeyframeInterval, 1);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);
  EXPECT_EQ(encoded_image_callback_.get_frame_counter(), kFrames);
}

TEST_F(StatsCollectingEncoderTest,
       StoreProcessingStatsCallbackFullHdSwSpatialLayers) {
  // P99 not meaningful for less than 100 frames.
  constexpr int kMinimumNumberOfFrames = 100;
  constexpr int kFrames = 200;
  constexpr int kSpatialLayers = 3;
  EXPECT_EQ(stats_callbacks_, 0);
  CreateAndEncodeFrames(kFullHdWidth, kFullHdHeight, kSpatialLayers,
                        /*is_hw_accelerated=*/true, kFrames, kKeyframeInterval,
                        kFramerate);
  // Verify that there's been one stats callback and that the numbers are
  // reasonable.
  EXPECT_EQ(stats_callbacks_, 1);
  EXPECT_FALSE(last_stats_key_.is_decode);
  EXPECT_EQ(last_stats_key_.codec_profile, kCodecProfile);
  EXPECT_EQ(last_stats_key_.pixel_size, kFullHdWidth * kFullHdHeight);
  EXPECT_TRUE(last_stats_key_.hw_accelerated);
  EXPECT_GE(last_video_stats_.frame_count, kMinimumNumberOfFrames);
  EXPECT_LT(last_video_stats_.frame_count, kFrames);
  EXPECT_NEAR(last_video_stats_.key_frame_count,
              last_video_stats_.frame_count / kKeyframeInterval, 1);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);
  EXPECT_EQ(encoded_image_callback_.get_frame_counter(),
            kFrames * kSpatialLayers);
}

TEST_F(StatsCollectingEncoderTest,
       CollectionStopsIfThereAreMultipleEncodersActive) {
  constexpr int kMinutesToRun = 10;
  EXPECT_EQ(stats_callbacks_, 0);
  CreateAndEncodeFrames(kHdWidth, kHdHeight, /*spatial_layers=*/1,
                        /*is_hw_accelerated=*/false, kFramesPerMinute,
                        kKeyframeInterval, kFramerate);
  int last_stats_callbacks = stats_callbacks_;

  // Create another encoder.
  FakeEncodedImageCallback second_encoded_image_callback;
  int second_encoder_spatial_layers = 1;
  bool second_encoder_is_hw_accelerated = false;
  StatsCollectingEncoder second_stats_encoder(
      kFormatVp9,
      std::make_unique<MockEncoder>(&task_environment_,
                                    &second_encoder_spatial_layers,
                                    &second_encoder_is_hw_accelerated),
      base::DoNothing());
  second_stats_encoder.RegisterEncodeCompleteCallback(
      &second_encoded_image_callback);
  // Encode one frame to make it an active encoder.
  CreateAndEncodeFrames(
      &second_stats_encoder, kHdWidth, kHdHeight, /*spatial_layers=*/1,
      /*is_hw_accelerated=*/false, 1, kKeyframeInterval, kFramerate);

  // Run for a few minutes and verify that no new callbacks are made.
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    CreateAndEncodeFrames(kHdWidth, kHdHeight, /*spatial_layers=*/1,
                          /*is_hw_accelerated=*/false, kFramesPerMinute,
                          kKeyframeInterval, kFramerate);
    // The expectation could be relaxed to allow for one callback to happen.
    EXPECT_EQ(stats_callbacks_, last_stats_callbacks);
  }
  second_stats_encoder.Release();
}

TEST_F(StatsCollectingEncoderTest, CollectionStartsAgainIfOneEncoderIsActive) {
  constexpr int kMinutesToRun = 10;
  EXPECT_EQ(stats_callbacks_, 0);

  // Create another encoder.
  FakeEncodedImageCallback second_encoded_image_callback;
  int second_encoder_spatial_layers = 1;
  bool second_encoder_is_hw_accelerated = false;
  StatsCollectingEncoder second_stats_encoder(
      kFormatVp9,
      std::make_unique<MockEncoder>(&task_environment_,
                                    &second_encoder_spatial_layers,
                                    &second_encoder_is_hw_accelerated),
      base::DoNothing());
  second_stats_encoder.RegisterEncodeCompleteCallback(
      &second_encoded_image_callback);
  // Encode one frame to make it an active encoder.
  CreateAndEncodeFrames(
      &second_stats_encoder, kHdWidth, kHdHeight, /*spatial_layers=*/1,
      /*is_hw_accelerated=*/false, 1, kKeyframeInterval, kFramerate);

  int last_stats_callbacks = stats_callbacks_;
  // Run for a few minutes and verify that no callbacks are made.
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    CreateAndEncodeFrames(kHdWidth, kHdHeight, /*spatial_layers=*/1,
                          /*is_hw_accelerated=*/false, kFramesPerMinute,
                          kKeyframeInterval, kFramerate);
    EXPECT_EQ(stats_callbacks_, last_stats_callbacks);
  }

  // Release the other encoder and verify that collection starts again.
  second_stats_encoder.Release();

  // Run for a few minutes and verify that no callbacks are made.
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    CreateAndEncodeFrames(kHdWidth, kHdHeight, /*spatial_layers=*/1,
                          /*is_hw_accelerated=*/false, kFramesPerMinute,
                          kKeyframeInterval, kFramerate);
    EXPECT_GT(stats_callbacks_, last_stats_callbacks);
    last_stats_callbacks = stats_callbacks_;
  }
}

TEST_F(StatsCollectingEncoderTest, NoCollectionAfter40000Frames) {
  constexpr int kMinutesToRun = 10;
  constexpr int kFrames = 40000;
  EXPECT_EQ(stats_callbacks_, 0);
  CreateAndEncodeFrames(kHdWidth, kHdHeight, /*spatial_layers=*/1,
                        /*is_hw_accelerated=*/false, kFrames, kKeyframeInterval,
                        kFramerate);
  EXPECT_GT(stats_callbacks_, 0);
  int last_stats_callbacks = stats_callbacks_;

  // Run for a few minutes and verify that no new callbacks are made.
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    CreateAndEncodeFrames(kHdWidth, kHdHeight, /*spatial_layers=*/1,
                          /*is_hw_accelerated=*/false, kFramesPerMinute,
                          kKeyframeInterval, kFramerate);
    // The expectation could be relaxed to allow for one callback to happen.
    EXPECT_EQ(stats_callbacks_, last_stats_callbacks);
  }
}

TEST_F(StatsCollectingEncoderTest, MethodCallsForwardedToInternalEncoder) {
  // We don't are about the object itself, only that the pointer is forwarded
  // correctly.
  webrtc::FecControllerOverride* fec_controller_override =
      reinterpret_cast<webrtc::FecControllerOverride*>(0x123456);
  EXPECT_CALL(*internal_encoder_,
              SetFecControllerOverride(fec_controller_override));
  stats_encoder_.SetFecControllerOverride(fec_controller_override);

  webrtc::VideoCodec* codec_settings_ptr =
      reinterpret_cast<webrtc::VideoCodec*>(0x123456);
  webrtc::VideoEncoder::Capabilities capabilities(/*loss_notification=*/true);
  webrtc::VideoEncoder::Settings kSettings{capabilities, /*number_of_cores=*/2,
                                           /*max_payload_size=*/2400};
  constexpr int kExpectedInitReturnValue = 13;
  EXPECT_CALL(*internal_encoder_, InitEncode)
      .WillOnce([codec_settings_ptr, &kSettings](
                    const webrtc::VideoCodec* codec_settings,
                    const webrtc::VideoEncoder::Settings& settings) {
        EXPECT_EQ(codec_settings_ptr, codec_settings);
        EXPECT_EQ(kSettings.capabilities.loss_notification,
                  settings.capabilities.loss_notification);
        EXPECT_EQ(kSettings.number_of_cores, settings.number_of_cores);
        EXPECT_EQ(kSettings.max_payload_size, settings.max_payload_size);
        return kExpectedInitReturnValue;
      });
  EXPECT_EQ(stats_encoder_.InitEncode(codec_settings_ptr, kSettings),
            kExpectedInitReturnValue);

  const webrtc::VideoBitrateAllocation kVideoBitrateAllocation;
  const webrtc::VideoEncoder::RateControlParameters kParameters = {
      kVideoBitrateAllocation, /*framerate_fps=*/24.0};
  EXPECT_CALL(*internal_encoder_, SetRates(kParameters));
  stats_encoder_.SetRates(kParameters);

  constexpr float kPacketLossRate = 0.012;
  EXPECT_CALL(*internal_encoder_, OnPacketLossRateUpdate(kPacketLossRate));
  stats_encoder_.OnPacketLossRateUpdate(kPacketLossRate);

  constexpr int64_t kRttMs = 12345;
  EXPECT_CALL(*internal_encoder_, OnRttUpdate(kRttMs));
  stats_encoder_.OnRttUpdate(kRttMs);

  const webrtc::VideoEncoder::LossNotification kLossNotification{
      /*timestamp_of_last_decodable=*/7u, /*timestamp_of_last_received=*/9u};
  EXPECT_CALL(*internal_encoder_, OnLossNotification)
      .WillOnce(
          [&kLossNotification](
              const webrtc::VideoEncoder::LossNotification& loss_notification) {
            EXPECT_EQ(loss_notification.timestamp_of_last_decodable,
                      kLossNotification.timestamp_of_last_decodable);
            EXPECT_EQ(loss_notification.timestamp_of_last_received,
                      kLossNotification.timestamp_of_last_received);
          });
  stats_encoder_.OnLossNotification(kLossNotification);
}

}  // namespace
}  // namespace blink
