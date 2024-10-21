// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MOCK_VIDEO_CAPTURE_DEVICE_CLIENT_H_
#define MEDIA_CAPTURE_VIDEO_MOCK_VIDEO_CAPTURE_DEVICE_CLIENT_H_

#include <memory>

#include "media/capture/video/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

using FakeFrameCapturedCallback =
    base::RepeatingCallback<void(const VideoCaptureFormat&)>;

class MockVideoCaptureDeviceClient : public VideoCaptureDevice::Client {
 public:
  MockVideoCaptureDeviceClient();
  ~MockVideoCaptureDeviceClient() override;

  MOCK_METHOD(void, OnCaptureConfigurationChanged, (), (override));
  MOCK_METHOD(void,
              OnIncomingCapturedData,
              (const uint8_t* data,
               int length,
               const VideoCaptureFormat& frame_format,
               const gfx::ColorSpace& color_space,
               int rotation,
               bool flip_y,
               base::TimeTicks reference_time,
               base::TimeDelta timestamp,
               std::optional<base::TimeTicks> capture_begin_time,
               int frame_feedback_id),
              (override));
  MOCK_METHOD(void,
              OnIncomingCapturedGfxBuffer,
              (gfx::GpuMemoryBuffer * buffer,
               const VideoCaptureFormat& frame_format,
               int clockwise_rotation,
               base::TimeTicks reference_time,
               base::TimeDelta timestamp,
               std::optional<base::TimeTicks> capture_begin_time,
               int frame_feedback_id),
              (override));
  MOCK_METHOD(void,
              OnIncomingCapturedExternalBuffer,
              (CapturedExternalVideoBuffer buffer,
               base::TimeTicks reference_time,
               base::TimeDelta timestamp,
               std::optional<base::TimeTicks> capture_begin_time,
               const gfx::Rect& visible_rect),
              (override));
  MOCK_METHOD(ReserveResult,
              ReserveOutputBuffer,
              (const gfx::Size&, VideoPixelFormat, int, Buffer*, int*, int*),
              (override));
  MOCK_METHOD(void,
              OnError,
              (media::VideoCaptureError error,
               const base::Location& from_here,
               const std::string& reason),
              (override));
  MOCK_METHOD(void,
              OnFrameDropped,
              (VideoCaptureFrameDropReason reason),
              (override));
  MOCK_METHOD(void, OnStarted, (), (override));
  MOCK_METHOD(double, GetBufferPoolUtilization, (), (const override));

  void OnIncomingCapturedBuffer(
      Buffer buffer,
      const VideoCaptureFormat& format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time) override;
  void OnIncomingCapturedBufferExt(
      Buffer buffer,
      const VideoCaptureFormat& format,
      const gfx::ColorSpace& color_space,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time,
      gfx::Rect visible_rect,
      const VideoFrameMetadata& additional_metadata) override;

  MOCK_METHOD(
      void,
      DoOnIncomingCapturedBuffer,
      (Buffer&, const VideoCaptureFormat&, base::TimeTicks, base::TimeDelta),
      ());
  MOCK_METHOD(void,
              DoOnIncomingCapturedBufferExt,
              (Buffer & buffer,
               const VideoCaptureFormat& format,
               const gfx::ColorSpace& color_space,
               base::TimeTicks reference_time,
               base::TimeDelta timestamp,
               gfx::Rect visible_rect,
               const VideoFrameMetadata& additional_metadata),
              ());

  static std::unique_ptr<MockVideoCaptureDeviceClient>
  CreateMockClientWithBufferAllocator(
      FakeFrameCapturedCallback frame_captured_callback);

 protected:
  FakeFrameCapturedCallback fake_frame_captured_callback_;
};

using NiceMockVideoCaptureDeviceClient =
    ::testing::NiceMock<MockVideoCaptureDeviceClient>;

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MOCK_VIDEO_CAPTURE_DEVICE_CLIENT_H_
