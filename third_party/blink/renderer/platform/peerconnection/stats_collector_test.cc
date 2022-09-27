// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/platform/peerconnection/stats_collector.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
constexpr float kMinProcessingTimeMs = 1.0f;
constexpr float kExpectedP99ProcessingTimeMs = 12.0f;
constexpr float kP99ToleranceMs = 0.5f;
constexpr media::VideoCodecProfile kCodecProfile =
    media::VideoCodecProfile::VP9PROFILE_PROFILE0;
constexpr int kHdWidth = 1280;
constexpr int kHdHeight = 720;
constexpr int kFullHdWidth = 1920;
constexpr int kFullHdHeight = 1080;
constexpr int kFramerate = 30;
constexpr int kFramesPerMinute = kFramerate * 60;
constexpr int kKeyframeInterval = 25;

class StatsCollectorTest : public ::testing::Test {
 protected:
  StatsCollectorTest()
      : mock_now_(base::TimeTicks::Now()),
        stats_collector_(
            /*is_decode=*/true,
            kCodecProfile,
            base::BindRepeating(&StatsCollectorTest::StoreProcessingStatsCB,
                                base::Unretained(this))) {
    stats_collector_.StartStatsCollection();
  }

  void StoreProcessingStatsCB(const StatsCollector::StatsKey& stats_key,
                              const StatsCollector::VideoStats& video_stats) {
    ++stats_callbacks_;
    last_stats_key_ = stats_key;
    last_video_stats_ = video_stats;
  }

  void ProcessFrames(int width,
                     int height,
                     bool is_hw_accelerated,
                     int frames,
                     int key_frame_interval,
                     int frame_rate) {
    int pixel_size = width * height;
    for (int i = 0; i < frames; ++i) {
      bool is_keyframe = i % key_frame_interval == 0;
      // Create a distribution with the specified 90th percentile.
      float processing_time_ms =
          i % 100 < 90 ? kMinProcessingTimeMs : kExpectedP99ProcessingTimeMs;

      mock_now_ += base::Milliseconds(1000 / frame_rate);
      if (!stats_collector_.stats_collection_finished()) {
        stats_collector_.AddProcessingTime(pixel_size, is_hw_accelerated,
                                           processing_time_ms, is_keyframe,
                                           mock_now_);
      }
    }
  }

  base::TimeTicks mock_now_;

  StatsCollector stats_collector_;

  int stats_callbacks_{0};
  StatsCollector::StatsKey last_stats_key_;
  StatsCollector::VideoStats last_video_stats_;
};

TEST_F(StatsCollectorTest, OneCallbackAfterMinNumberOfFrames) {
  constexpr int kFrames = StatsCollector::kMinSamplesThreshold + 10;
  EXPECT_EQ(stats_callbacks_, 0);
  ProcessFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false, kFrames,
                kKeyframeInterval, kFramerate);
  // Verify that there's been one stats callback and that the numbers are
  // reasonable.
  EXPECT_EQ(stats_callbacks_, 1);
  EXPECT_TRUE(last_stats_key_.is_decode);
  EXPECT_EQ(last_stats_key_.codec_profile, kCodecProfile);
  EXPECT_EQ(last_stats_key_.pixel_size, kHdWidth * kHdHeight);
  EXPECT_FALSE(last_stats_key_.hw_accelerated);
  EXPECT_GE(last_video_stats_.frame_count,
            StatsCollector::kMinSamplesThreshold);
  EXPECT_LT(last_video_stats_.frame_count, kFrames);
  EXPECT_NEAR(last_video_stats_.key_frame_count,
              last_video_stats_.frame_count / kKeyframeInterval, 1);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);
}

TEST_F(StatsCollectorTest, AtLeastOneCallbackEveryMinute) {
  constexpr int kMinutesToRun = 10;
  EXPECT_EQ(stats_callbacks_, 0);
  int last_stats_callbacks = stats_callbacks_;
  int frames_processed = 0;
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    ProcessFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                  kFramesPerMinute, kKeyframeInterval, kFramerate);
    frames_processed += kFramesPerMinute;
    // Verify that the counter are incremented.
    EXPECT_GT(stats_callbacks_, last_stats_callbacks);
    last_stats_callbacks = stats_callbacks_;
    EXPECT_TRUE(last_stats_key_.is_decode);
    EXPECT_EQ(last_stats_key_.codec_profile, kCodecProfile);
    EXPECT_EQ(last_stats_key_.pixel_size, kHdWidth * kHdHeight);
    EXPECT_FALSE(last_stats_key_.hw_accelerated);
    EXPECT_GE(last_video_stats_.frame_count,
              frames_processed - kFramesPerMinute / 2);
    EXPECT_LT(last_video_stats_.frame_count, frames_processed);
    EXPECT_NEAR(last_video_stats_.key_frame_count,
                last_video_stats_.frame_count / kKeyframeInterval, 1);
    EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
                kExpectedP99ProcessingTimeMs, kP99ToleranceMs);
  }
}

TEST_F(StatsCollectorTest, NewReportIfResolutionChanges) {
  constexpr int kNumberOfFramesDuringTenSeconds = kFramerate * 10;
  EXPECT_EQ(stats_callbacks_, 0);
  int last_stats_callbacks = stats_callbacks_;
  ProcessFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                kNumberOfFramesDuringTenSeconds, kKeyframeInterval, kFramerate);
  // One frame with a different resolution.
  ProcessFrames(kFullHdWidth, kFullHdHeight,
                /*is_hw_accelerated=*/false, 1, kKeyframeInterval, kFramerate);

  // Verify that the counter are incremented.
  EXPECT_GT(stats_callbacks_, last_stats_callbacks);
  last_stats_callbacks = stats_callbacks_;
  EXPECT_EQ(last_stats_key_.pixel_size, kHdWidth * kHdHeight);
  EXPECT_GE(last_video_stats_.frame_count, 100);
  EXPECT_LE(last_video_stats_.frame_count, kNumberOfFramesDuringTenSeconds);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);

  // Continue with new resolution and expect another report.
  ProcessFrames(kFullHdWidth, kFullHdHeight, /*is_hw_accelerated=*/false,
                kNumberOfFramesDuringTenSeconds, kKeyframeInterval, kFramerate);
  EXPECT_GT(stats_callbacks_, last_stats_callbacks);
  EXPECT_EQ(last_stats_key_.pixel_size, kFullHdWidth * kFullHdHeight);
  EXPECT_GE(last_video_stats_.frame_count, 100);
  EXPECT_LE(last_video_stats_.frame_count, kNumberOfFramesDuringTenSeconds);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);
}

TEST_F(StatsCollectorTest, NewReportIfHwAccelerationChanges) {
  constexpr int kNumberOfFramesDuringTenSeconds = kFramerate * 10;
  EXPECT_EQ(stats_callbacks_, 0);
  int last_stats_callbacks = stats_callbacks_;
  ProcessFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                kNumberOfFramesDuringTenSeconds, kKeyframeInterval, kFramerate);
  // One frame with HW acceleration.
  ProcessFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/true, 1,
                kKeyframeInterval, kFramerate);

  // Verify that the counter are incremented.
  EXPECT_GT(stats_callbacks_, last_stats_callbacks);
  last_stats_callbacks = stats_callbacks_;
  EXPECT_EQ(last_stats_key_.pixel_size, kHdWidth * kHdHeight);
  EXPECT_FALSE(last_stats_key_.hw_accelerated);
  EXPECT_GE(last_video_stats_.frame_count, 100);
  EXPECT_LE(last_video_stats_.frame_count, kNumberOfFramesDuringTenSeconds);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);

  // Continue with new resolution and expect another report.
  ProcessFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/true,
                kNumberOfFramesDuringTenSeconds, kKeyframeInterval, kFramerate);
  EXPECT_GT(stats_callbacks_, last_stats_callbacks);
  EXPECT_EQ(last_stats_key_.pixel_size, kHdWidth * kHdHeight);
  EXPECT_TRUE(last_stats_key_.hw_accelerated);
  EXPECT_GE(last_video_stats_.frame_count, 100);
  EXPECT_LE(last_video_stats_.frame_count, kNumberOfFramesDuringTenSeconds);
  EXPECT_NEAR(last_video_stats_.p99_processing_time_ms,
              kExpectedP99ProcessingTimeMs, kP99ToleranceMs);
}

TEST_F(StatsCollectorTest, NoCollectionAfter40000Frames) {
  constexpr int kMinutesToRun = 10;
  constexpr int kFrames = 40000;
  EXPECT_EQ(stats_callbacks_, 0);
  ProcessFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false, kFrames,
                kKeyframeInterval, kFramerate);
  EXPECT_GT(stats_callbacks_, 0);
  int last_stats_callbacks = stats_callbacks_;

  // Run for a few minutes and verify that no new callbacks are made.
  for (int minute = 0; minute < kMinutesToRun; ++minute) {
    ProcessFrames(kHdWidth, kHdHeight, /*is_hw_accelerated=*/false,
                  kFramesPerMinute, kKeyframeInterval, kFramerate);
    // The expectation could be relaxed to allow for one callback to happen.
    EXPECT_EQ(stats_callbacks_, last_stats_callbacks);
  }
}

}  // namespace
}  // namespace blink
