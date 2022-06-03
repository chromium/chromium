// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/constants.h"
#include "remoting/codec/frame_processing_time_estimator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using base::TimeTicks;

namespace {

WebrtcVideoEncoder::EncodedFrame CreateEncodedFrame(bool key_frame,
                                                    int size,
                                                    TimeTicks start,
                                                    TimeTicks end) {
  WebrtcVideoEncoder::EncodedFrame result;
  result.key_frame = key_frame;
  result.data.assign(static_cast<size_t>(size), 'A');
  result.stats = std::make_unique<WebrtcVideoEncoder::FrameStats>();
  result.stats->capture_started_time = start;
  result.stats->encode_ended_time = end;
  return result;
}

}  // namespace

TEST(FrameProcessingTimeEstimatorTest, EstimateDeltaAndKeyFrame) {
  FrameProcessingTimeEstimator estimator;
  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start;
  for (int i = 0; i < 10; i++) {
    start = end;
    if (i % 5 == 0) {
      // Fake a key-frame.
      end += base::Milliseconds(10);
      estimator.FinishFrame(CreateEncodedFrame(true, 100, start, end));
    } else {
      // Fake a delta-frame.
      end += base::Milliseconds(1);
      estimator.FinishFrame(CreateEncodedFrame(false, 50, start, end));
    }
  }

  estimator.SetBandwidthKbps(50);
  estimator.SetBandwidthKbps(150);

  EXPECT_EQ(100, estimator.AverageBandwidthKbps());

  EXPECT_EQ(base::Milliseconds(10), estimator.EstimatedProcessingTime(true));
  EXPECT_EQ(base::Milliseconds(1), estimator.EstimatedProcessingTime(false));
  EXPECT_EQ(base::Milliseconds(8), estimator.EstimatedTransitTime(true));
  EXPECT_EQ(base::Milliseconds(4), estimator.EstimatedTransitTime(false));

  EXPECT_EQ(60, estimator.EstimatedFrameSize());

  EXPECT_EQ(base::Microseconds(2800), estimator.EstimatedProcessingTime());
  EXPECT_EQ(base::Microseconds(4800), estimator.EstimatedTransitTime());

  EXPECT_EQ(kTargetFrameRate, estimator.RecentFrameRate());
  EXPECT_EQ(kTargetFrameRate, estimator.PredictedFrameRate());
  EXPECT_EQ(kTargetFrameRate, estimator.EstimatedFrameRate());
}

TEST(FrameProcessingTimeEstimatorTest, NegativeBandwidthShouldBeDropped) {
  FrameProcessingTimeEstimator estimator;
  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start + base::Milliseconds(10);
  estimator.FinishFrame(CreateEncodedFrame(true, 100, start, end));
  estimator.SetBandwidthKbps(100);
  estimator.SetBandwidthKbps(-100);
  EXPECT_EQ(base::Milliseconds(8), estimator.EstimatedTransitTime(true));
}

TEST(FrameProcessingTimeEstimatorTest, ShouldNotReturn0WithOnlyKeyFrames) {
  FrameProcessingTimeEstimator estimator;
  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start + base::Milliseconds(10);
  estimator.FinishFrame(CreateEncodedFrame(true, 100, start, end));
  estimator.SetBandwidthKbps(100);
  EXPECT_EQ(base::Milliseconds(10), estimator.EstimatedProcessingTime(true));
  EXPECT_EQ(base::Milliseconds(8), estimator.EstimatedTransitTime(true));
  EXPECT_EQ(base::Milliseconds(10), estimator.EstimatedProcessingTime(false));
  EXPECT_EQ(base::Milliseconds(8), estimator.EstimatedTransitTime(false));

  EXPECT_EQ(base::Milliseconds(10), estimator.EstimatedProcessingTime());
  EXPECT_EQ(base::Milliseconds(8), estimator.EstimatedTransitTime());
  EXPECT_EQ(100, estimator.EstimatedFrameSize());
  EXPECT_EQ(kTargetFrameRate, estimator.RecentFrameRate());
  EXPECT_EQ(kTargetFrameRate, estimator.PredictedFrameRate());
  EXPECT_EQ(kTargetFrameRate, estimator.EstimatedFrameRate());
}

TEST(FrameProcessingTimeEstimatorTest, ShouldNotReturn0WithOnlyDeltaFrames) {
  FrameProcessingTimeEstimator estimator;
  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start + base::Milliseconds(1);
  estimator.FinishFrame(CreateEncodedFrame(false, 50, start, end));
  estimator.SetBandwidthKbps(100);
  EXPECT_EQ(base::Milliseconds(1), estimator.EstimatedProcessingTime(false));
  EXPECT_EQ(base::Milliseconds(4), estimator.EstimatedTransitTime(false));
  EXPECT_EQ(base::Milliseconds(1), estimator.EstimatedProcessingTime(true));
  EXPECT_EQ(base::Milliseconds(4), estimator.EstimatedTransitTime(true));

  EXPECT_EQ(base::Milliseconds(1), estimator.EstimatedProcessingTime());
  EXPECT_EQ(base::Milliseconds(4), estimator.EstimatedTransitTime());
  EXPECT_EQ(50, estimator.EstimatedFrameSize());
  EXPECT_EQ(kTargetFrameRate, estimator.RecentFrameRate());
  EXPECT_EQ(kTargetFrameRate, estimator.PredictedFrameRate());
  EXPECT_EQ(kTargetFrameRate, estimator.EstimatedFrameRate());
}

TEST(FrameProcessingTimeEstimatorTest, ShouldReturnDefaultValueWithoutRecords) {
  FrameProcessingTimeEstimator estimator;
  EXPECT_EQ(base::TimeDelta(), estimator.EstimatedProcessingTime(true));
  EXPECT_EQ(base::TimeDelta(), estimator.EstimatedProcessingTime(false));
  EXPECT_EQ(base::TimeDelta(), estimator.EstimatedProcessingTime());
  EXPECT_EQ(0, estimator.EstimatedFrameSize());
  EXPECT_EQ(kTargetFrameRate, estimator.RecentFrameRate());
  EXPECT_EQ(1, estimator.PredictedFrameRate());
  EXPECT_EQ(1, estimator.EstimatedFrameRate());

  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start;
  for (int i = 0; i < 10; i++) {
    start = end;
    if (i % 5 == 0) {
      // Fake a key-frame.
      end += base::Milliseconds(100);
      estimator.FinishFrame(CreateEncodedFrame(true, 100, start, end));
    } else {
      // Fake a delta-frame.
      end += base::Milliseconds(50);
      estimator.FinishFrame(CreateEncodedFrame(false, 50, start, end));
    }
  }
  EXPECT_EQ(base::Milliseconds(static_cast<double>(500) / 9),
            estimator.RecentAverageFrameInterval());
  EXPECT_EQ(19, estimator.RecentFrameRate());
  EXPECT_EQ(1, estimator.PredictedFrameRate());
  EXPECT_EQ(1, estimator.EstimatedFrameRate());
}

TEST(FrameProcessingTimeEstimatorTest,
     ShouldEstimateFrameRateFromProcessingTime) {
  // Processing times are much longer than transit times, so the estimation of
  // the frame rate should depend on the processing time.
  FrameProcessingTimeEstimator estimator;
  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start;
  for (int i = 0; i < 10; i++) {
    start = end;
    if (i % 5 == 0) {
      // Fake a key-frame.
      end += base::Milliseconds(100);
      estimator.FinishFrame(CreateEncodedFrame(true, 100, start, end));
    } else {
      // Fake a delta-frame.
      end += base::Milliseconds(50);
      estimator.FinishFrame(CreateEncodedFrame(false, 50, start, end));
    }
  }
  estimator.SetBandwidthKbps(100);

  EXPECT_EQ(base::Milliseconds(60), estimator.EstimatedProcessingTime());
  EXPECT_EQ(base::Milliseconds(static_cast<double>(500) / 9),
            estimator.RecentAverageFrameInterval());
  EXPECT_EQ(19, estimator.RecentFrameRate());
  EXPECT_EQ(17, estimator.PredictedFrameRate());
  EXPECT_EQ(17, estimator.EstimatedFrameRate());
}

TEST(FrameProcessingTimeEstimatorTest,
     ShouldEstimateFrameRateFromTransitTime) {
  // Transit times are much longer than processing times, so the estimation of
  // the frame rate should depend on the transit time.
  FrameProcessingTimeEstimator estimator;
  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start;
  for (int i = 0; i < 10; i++) {
    start = end;
    if (i % 5 == 0) {
      // Fake a key-frame.
      end += base::Milliseconds(10);
      estimator.FinishFrame(CreateEncodedFrame(true, 100, start, end));
    } else {
      // Fake a delta-frame.
      end += base::Milliseconds(1);
      estimator.FinishFrame(CreateEncodedFrame(false, 50, start, end));
    }
  }
  estimator.SetBandwidthKbps(10);

  EXPECT_EQ(base::Milliseconds(48), estimator.EstimatedTransitTime());
  EXPECT_EQ(kTargetFrameRate, estimator.RecentFrameRate());
  EXPECT_EQ(21, estimator.PredictedFrameRate());
  EXPECT_EQ(21, estimator.EstimatedFrameRate());
}

TEST(FrameProcessingTimeEstimatorTest,
     ShouldNotReturnNegativeEstimatedFrameRate) {
  // Both processing times and transit times are extremely long, estimator
  // should return 1.
  FrameProcessingTimeEstimator estimator;
  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start;
  for (int i = 0; i < 10; i++) {
    start = end;
    if (i % 5 == 0) {
      // Fake a key-frame.
      end += base::Milliseconds(10000);
      estimator.FinishFrame(CreateEncodedFrame(true, 1000, start, end));
    } else {
      // Fake a delta-frame.
      end += base::Milliseconds(5000);
      estimator.FinishFrame(CreateEncodedFrame(false, 500, start, end));
    }
  }
  estimator.SetBandwidthKbps(1);

  EXPECT_EQ(base::Milliseconds(6000), estimator.EstimatedProcessingTime());
  EXPECT_EQ(base::Milliseconds(4800), estimator.EstimatedTransitTime());
  EXPECT_EQ(1, estimator.RecentFrameRate());
  EXPECT_EQ(1, estimator.PredictedFrameRate());
  EXPECT_EQ(1, estimator.EstimatedFrameRate());
}

TEST(FrameProcessingTimeEstimatorTest,
     RecentAverageFrameIntervalShouldConsiderDelay) {
  FrameProcessingTimeEstimator estimator;
  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start;
  for (int i = 0; i < 10; i++) {
    end += base::Milliseconds(50);
    start = end;
    if (i % 5 == 0) {
      // Fake a key-frame.
      end += base::Milliseconds(10);
      estimator.FinishFrame(CreateEncodedFrame(true, 1000, start, end));
    } else {
      // Fake a delta-frame.
      end += base::Milliseconds(5);
      estimator.FinishFrame(CreateEncodedFrame(false, 500, start, end));
    }
  }
  estimator.SetBandwidthKbps(1000);
  EXPECT_EQ(base::Milliseconds(6), estimator.EstimatedProcessingTime());
  EXPECT_EQ(base::Milliseconds(static_cast<double>(50 * 9 + 10 + 5 * 8) / 9),
            estimator.RecentAverageFrameInterval());
  EXPECT_EQ(19, estimator.RecentFrameRate());
  // Processing time & transit time are both pretty low, we should be able to
  // reach 30 FPS if capturing delay has been reduced.
  EXPECT_EQ(kTargetFrameRate, estimator.PredictedFrameRate());
  EXPECT_EQ(19, estimator.EstimatedFrameRate());
}

}  // namespace remoting
