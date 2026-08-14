// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/mac/video_capture_metrics_mac.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

TEST(VideoCaptureMetricsMacTest, LogReactionEffectsGesturesState) {
  base::HistogramTester histogram_tester;
  LogReactionEffectsGesturesState();
  histogram_tester.ExpectTotalCount(
      "Media.VideoCapture.Mac.Device.ReactionEffectsGesturesState", 1);
}

}  // namespace

}  // namespace media
