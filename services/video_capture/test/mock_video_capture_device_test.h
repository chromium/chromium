// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_TEST_MOCK_VIDEO_CAPTURE_DEVICE_TEST_H_
#define SERVICES_VIDEO_CAPTURE_TEST_MOCK_VIDEO_CAPTURE_DEVICE_TEST_H_

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "media/capture/video/mock_device.h"
#include "media/capture/video/mock_device_factory.h"
#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device_factory_impl.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {
class SingleThreadTaskEnvironment;
}
}  // namespace base

namespace video_capture {

// Reusable test setup for testing with a single mock device.
class MockVideoCaptureDeviceTest : public ::testing::Test {
 public:
  MockVideoCaptureDeviceTest();
  ~MockVideoCaptureDeviceTest() override;

  void SetUp() override;

 protected:
  raw_ptr<media::MockDeviceFactory, DanglingUntriaged> mock_device_factory_;
  std::unique_ptr<DeviceFactoryImpl> mock_device_factory_adapter_;

  base::MockCallback<DeviceFactory::GetDeviceInfosCallback>
      device_infos_receiver_;

  media::MockDevice mock_device_;
  std::unique_ptr<MockVideoFrameHandler> mock_video_frame_handler_;
  raw_ptr<Device> device_;
  mojo::PendingRemote<mojom::VideoFrameHandler> mock_subscriber_;
  media::VideoCaptureParams requested_settings_;

 private:
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_TEST_MOCK_VIDEO_CAPTURE_DEVICE_TEST_H_
