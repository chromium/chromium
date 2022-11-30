// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

TEST(VideoCaptureMetricsTest, TestLogCaptureDeviceMetrics) {
  base::HistogramTester histogram_tester;
  std::vector<media::VideoCaptureDeviceInfo> devices_info;
  // First device
  VideoCaptureDeviceInfo first_device;
  first_device.supported_formats = {
      // NV12 QQVGA at 30fps, 15fps
      {{160, 120}, 30.0, media::PIXEL_FORMAT_NV12},
      {{160, 120}, 15.0, media::PIXEL_FORMAT_NV12},
      // NV12 VGA
      {{640, 480}, 30.0, media::PIXEL_FORMAT_NV12},
      // UYVY VGA
      {{640, 480}, 30.0, media::PIXEL_FORMAT_UYVY},
      // MJPEG 4K
      {{3840, 2160}, 30.0, media::PIXEL_FORMAT_MJPEG},
      // Odd resolution
      {{844, 400}, 30.0, media::PIXEL_FORMAT_NV12},
      // HD at unknown pixel format
      {{1280, 720}, 30.0, media::PIXEL_FORMAT_UNKNOWN}};
  devices_info.push_back(first_device);
  VideoCaptureDeviceInfo second_device;
  second_device.supported_formats = {
      // UYVY VGA to test that we get 2 UYVY and 2 VGA in metrics.
      {{640, 480}, 30.0, media::PIXEL_FORMAT_UYVY}};
  devices_info.push_back(second_device);

  LogCaptureDeviceMetrics(devices_info);

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Media.VideoCapture.Device.SupportedPixelFormat"),
              testing::UnorderedElementsAre(
                  base::Bucket(media::PIXEL_FORMAT_NV12, 1),
                  base::Bucket(media::PIXEL_FORMAT_UYVY, 2),
                  base::Bucket(media::PIXEL_FORMAT_MJPEG, 1),
                  base::Bucket(media::PIXEL_FORMAT_UNKNOWN, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Media.VideoCapture.Device.SupportedResolution"),
              testing::UnorderedElementsAre(
                  base::Bucket(0 /*other*/, 1), base::Bucket(1 /*qqvga*/, 1),
                  base::Bucket(6 /*vga*/, 2), base::Bucket(23 /*4k_UHD*/, 1),
                  base::Bucket(18 /*hd*/, 1)));
}

}  // namespace test
}  // namespace media