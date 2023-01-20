// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MOCK_VIDEO_CAPTURE_DEVICE_CLIENT_H_
#define MEDIA_CAPTURE_VIDEO_MOCK_VIDEO_CAPTURE_DEVICE_CLIENT_H_

#include "media/capture/video/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

using FakeFrameCapturedCallback =
    base::RepeatingCallback<void(const VideoCaptureFormat&)>;

class MockVideoCaptureDeviceClient : public VideoCaptureDevice::Client {
 public:
  MockVideoCaptureDeviceClient();
  ~MockVideoCaptureDeviceClient() override;

  MOCK_METHOD0(OnCaptureConfigurationChanged, void(void));
  MOCK_METHOD9(OnIncomingCapturedData,
               void(const uint8_t* data,
                    int length,
                    const VideoCaptureFormat& frame_format,
                    const gfx::ColorSpace& color_space,
                    int rotation,
                    bool flip_y,
                    base::TimeTicks reference_time,
                    base::TimeDelta timestamp,
                    int frame_feedback_id));
  MOCK_METHOD6(OnIncomingCapturedGfxBuffer,
               void(gfx::GpuMemoryBuffer* buffer,
                    const VideoCaptureFormat& frame_format,
                    int clockwise_rotation,
                    base::TimeTicks reference_time,
                    base::TimeDelta timestamp,
                    int frame_feedback_id));
  MOCK_METHOD5(OnIncomingCapturedExternalBuffer,
               void(CapturedExternalVideoBuffer buffer,
                    std::vector<CapturedExternalVideoBuffer> scaled_buffers,
                    base::TimeTicks reference_time,
                    base::TimeDelta timestamp,
                    gfx::Rect visible_size));
  MOCK_METHOD4(ReserveOutputBuffer,
               ReserveResult(const gfx::Size&, VideoPixelFormat, int, Buffer*));
  MOCK_METHOD3(OnError,
               void(media::VideoCaptureError error,
                    const base::Location& from_here,
                    const std::string& reason));
  MOCK_METHOD1(OnFrameDropped, void(VideoCaptureFrameDropReason reason));
  MOCK_METHOD0(OnStarted, void(void));
  MOCK_CONST_METHOD0(GetBufferPoolUtilization, double(void));

  void OnIncomingCapturedBuffer(Buffer buffer,
                                const VideoCaptureFormat& format,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp) override;
  void OnIncomingCapturedBufferExt(
      Buffer buffer,
      const VideoCaptureFormat& format,
      const gfx::ColorSpace& color_space,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      gfx::Rect visible_rect,
      const VideoFrameMetadata& additional_metadata) override;

  MOCK_METHOD4(DoOnIncomingCapturedBuffer,
               void(Buffer&,
                    const VideoCaptureFormat&,
                    base::TimeTicks,
                    base::TimeDelta));
  MOCK_METHOD7(DoOnIncomingCapturedBufferExt,
               void(Buffer& buffer,
                    const VideoCaptureFormat& format,
                    const gfx::ColorSpace& color_space,
                    base::TimeTicks reference_time,
                    base::TimeDelta timestamp,
                    gfx::Rect visible_rect,
                    const VideoFrameMetadata& additional_metadata));

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
