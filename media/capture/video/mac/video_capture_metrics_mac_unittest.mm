// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/mac/video_capture_metrics_mac.h"

#import <AVFoundation/AVFoundation.h>
#include <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/test/metrics/histogram_tester.h"
#include "media/base/video_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace media {

namespace {

TEST(VideoCaptureMetricsMacTest, NoMetricsLoggedIfNullRequestedCaptureFormat) {
  base::HistogramTester histogram_tester;
  LogFirstCapturedVideoFrame(nullptr, nullptr);
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Media."),
              testing::IsEmpty());
}

TEST(VideoCaptureMetricsMacTest, LogRequestedPixelFormat) {
  base::HistogramTester histogram_tester;

  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> requested_format;
  OSStatus status = CMVideoFormatDescriptionCreate(
      kCFAllocatorDefault,
      kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange /*NV12*/, 320, 180,
      nullptr, requested_format.InitializeInto());
  ASSERT_EQ(0, status);
  id capture_format = OCMClassMock([AVCaptureDeviceFormat class]);
  OCMStub([capture_format formatDescription]).andReturn(requested_format.get());

  LogFirstCapturedVideoFrame(capture_format, nullptr);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Media.VideoCapture.Mac.Device.RequestedPixelFormat"),
              testing::UnorderedElementsAre(
                  base::Bucket(VideoPixelFormat::PIXEL_FORMAT_NV12, 1)));
}

TEST(VideoCaptureMetricsMacTest, LogFirstFrameWhenAsRequested) {
  base::HistogramTester histogram_tester;

  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> requested_format;
  OSStatus status = CMVideoFormatDescriptionCreate(
      kCFAllocatorDefault,
      kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange /*NV12*/, 320, 180,
      nullptr, requested_format.InitializeInto());
  ASSERT_EQ(0, status);
  id capture_format = OCMClassMock([AVCaptureDeviceFormat class]);
  OCMStub([capture_format formatDescription]).andReturn(requested_format.get());

  // First frame equal.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> first_frame;
  status = CMSampleBufferCreate(kCFAllocatorDefault, nullptr, false, nullptr,
                                nullptr, requested_format.get(), 0, 0, nullptr,
                                0, nullptr, first_frame.InitializeInto());
  ASSERT_EQ(0, status);

  LogFirstCapturedVideoFrame(capture_format, first_frame.get());

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Media.VideoCapture.Mac.Device.RequestedPixelFormat"),
              testing::UnorderedElementsAre(
                  base::Bucket(VideoPixelFormat::PIXEL_FORMAT_NV12, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Media.VideoCapture.Mac.Device.CapturedWithRequestedPixelFormat"),
      testing::UnorderedElementsAre(base::Bucket(1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Media.VideoCapture.Mac.Device.CapturedWithRequestedResolution"),
      testing::UnorderedElementsAre(base::Bucket(4, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Media.VideoCapture.Mac.Device.CapturedIOSurface"),
              testing::UnorderedElementsAre(base::Bucket(0, 1)));
}

TEST(VideoCaptureMetricsMacTest, LogReactionEffectsGesturesState) {
  base::HistogramTester histogram_tester;
  LogReactionEffectsGesturesState();
  histogram_tester.ExpectTotalCount(
      "Media.VideoCapture.Mac.Device.ReactionEffectsGesturesState", 1);
}

}  // namespace

}  // namespace media
