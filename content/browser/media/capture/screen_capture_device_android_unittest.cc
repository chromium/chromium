// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_device_android.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtMost;

namespace content {
namespace {

const int kFrameRate = 30;

class MockDeviceClient : public media::VideoCaptureDevice::Client {
 public:
  MOCK_METHOD9(OnIncomingCapturedData,
               void(const uint8_t* data,
                    int length,
                    const media::VideoCaptureFormat& frame_format,
                    const gfx::ColorSpace& color_space,
                    int rotation,
                    bool flip_y,
                    base::TimeTicks reference_time,
                    base::TimeDelta tiemstamp,
                    int frame_feedback_id));
  MOCK_METHOD6(OnIncomingCapturedGfxBuffer,
               void(gfx::GpuMemoryBuffer* buffer,
                    const media::VideoCaptureFormat& frame_format,
                    int clockwise_rotation,
                    base::TimeTicks reference_time,
                    base::TimeDelta timestamp,
                    int frame_feedback_id));
  MOCK_METHOD0(DoReserveOutputBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedVideoFrame, void(void));
  MOCK_METHOD0(DoResurrectLastOutputBuffer, void(void));
  MOCK_METHOD3(OnError,
               void(media::VideoCaptureError error,
                    const base::Location& from_here,
                    const std::string& reason));
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason reason));
  MOCK_CONST_METHOD0(GetBufferPoolUtilization, double(void));
  MOCK_METHOD0(OnStarted, void(void));

  // Trampoline methods to workaround GMOCK problems with std::unique_ptr<>.
  media::VideoCaptureDevice::Client::ReserveResult ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      int frame_feedback_id,
      Buffer* buffer) override {
    EXPECT_EQ(media::PIXEL_FORMAT_I420, format);
    DoReserveOutputBuffer();
    return media::VideoCaptureDevice::Client::ReserveResult::kSucceeded;
  }
  void OnIncomingCapturedBuffer(Buffer buffer,
                                const media::VideoCaptureFormat& frame_format,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp) override {
    DoOnIncomingCapturedBuffer();
  }
  void OnIncomingCapturedBufferExt(
      Buffer buffer,
      const media::VideoCaptureFormat& format,
      const gfx::ColorSpace& color_space,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      gfx::Rect visible_rect,
      const media::VideoFrameMetadata& additional_metadata) override {
    DoOnIncomingCapturedVideoFrame();
  }
};

class ScreenCaptureDeviceAndroidTest : public testing::Test {
 public:
  ScreenCaptureDeviceAndroidTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureDeviceAndroidTest);
};

TEST_F(ScreenCaptureDeviceAndroidTest, ConstructionDestruction) {
  std::unique_ptr<media::VideoCaptureDevice> capture_device =
      std::make_unique<ScreenCaptureDeviceAndroid>();
}

// Place holder. Currently user input result is required to start
// MediaProjection, so we can't start a unittest that really starts capture.
TEST_F(ScreenCaptureDeviceAndroidTest, DISABLED_StartAndStop) {
  std::unique_ptr<media::VideoCaptureDevice> capture_device =
      std::make_unique<ScreenCaptureDeviceAndroid>();
  ASSERT_TRUE(capture_device);

  std::unique_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
  // |STARTED| is reported asynchronously, which may not be received if capture
  // is stopped immediately.
  EXPECT_CALL(*client, OnStarted()).Times(AtMost(1));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_device->AllocateAndStart(capture_params, std::move(client));
  capture_device->StopAndDeAllocate();
}

}  // namespace
}  // namespace Content
