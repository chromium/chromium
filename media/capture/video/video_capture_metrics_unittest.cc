// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/mojom/image_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::UnorderedElementsAre;

namespace media {

mojom::PhotoStatePtr MakePhotoState(bool blur,
                                    bool face_framing,
                                    bool eye_gaze) {
  mojom::PhotoStatePtr result = mojo::CreateEmptyPhotoState();
  result->supported_background_blur_modes = {mojom::BackgroundBlurMode::BLUR};
  result->supported_face_framing_modes = {mojom::MeteringMode::SINGLE_SHOT};
  result->supported_eye_gaze_correction_modes = {
      mojom::EyeGazeCorrectionMode::ON};
  if (blur) {
    result->background_blur_mode = mojom::BackgroundBlurMode::BLUR;
  }
  if (face_framing) {
    result->current_face_framing_mode = mojom::MeteringMode::SINGLE_SHOT;
  }
  if (eye_gaze) {
    result->current_eye_gaze_correction_mode = mojom::EyeGazeCorrectionMode::ON;
  }
  return result;
}

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
              UnorderedElementsAre(Bucket(media::PIXEL_FORMAT_NV12, 1),
                                   Bucket(media::PIXEL_FORMAT_UYVY, 2),
                                   Bucket(media::PIXEL_FORMAT_MJPEG, 1),
                                   Bucket(media::PIXEL_FORMAT_UNKNOWN, 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Media.VideoCapture.Device.SupportedResolution"),
      UnorderedElementsAre(Bucket(0 /*other*/, 1), Bucket(1 /*qqvga*/, 1),
                           Bucket(6 /*vga*/, 2), Bucket(23 /*4k_UHD*/, 1),
                           Bucket(18 /*hd*/, 1)));
}

TEST(VideoCaptureMetricsTest, TestLogCaptureDeviceEffects) {
  base::HistogramTester histogram_tester;

  // No effects supported.
  LogCaptureDeviceEffects(mojo::CreateEmptyPhotoState());
  // All effects supported, none are enabled.
  LogCaptureDeviceEffects(MakePhotoState(false, false, false));
  LogCaptureDeviceEffects(MakePhotoState(true /*blur*/, false, false));
  LogCaptureDeviceEffects(MakePhotoState(false, true /*framing*/, false));
  LogCaptureDeviceEffects(MakePhotoState(false, false, true /*eye gaze*/));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Media.VideoCapture.Device.Effect2.BackgroundBlur"),
              UnorderedElementsAre(Bucket(0, 1), Bucket(1, 3), Bucket(2, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Media.VideoCapture.Device.Effect2.EyeGazeCorrection"),
              UnorderedElementsAre(Bucket(0, 1), Bucket(1, 3), Bucket(2, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Media.VideoCapture.Device.Effect2.FaceFraming"),
              UnorderedElementsAre(Bucket(0, 1), Bucket(1, 3), Bucket(2, 1)));
}

}  // namespace media
