// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_TEST_MOCK_DEVICE_TEST_H_
#define SERVICES_VIDEO_CAPTURE_TEST_MOCK_DEVICE_TEST_H_

#include "base/test/mock_callback.h"
#include "media/capture/video/mock_device.h"
#include "media/capture/video/mock_device_factory.h"
#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {
class SingleThreadTaskEnvironment;
}
}

namespace video_capture {

// Reusable test setup for testing with a single mock device.
class MockDeviceTest : public ::testing::Test {
 public:
  MockDeviceTest();
  ~MockDeviceTest() override;

  void SetUp() override;

 protected:
  media::MockDeviceFactory* mock_device_factory_;
  std::unique_ptr<DeviceFactoryMediaToMojoAdapter> mock_device_factory_adapter_;

  mojo::Remote<mojom::DeviceFactory> factory_;
  std::unique_ptr<mojo::Receiver<mojom::DeviceFactory>> mock_factory_receiver_;
  base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
      device_infos_receiver_;

  media::MockDevice mock_device_;
  std::unique_ptr<MockVideoFrameHandler> mock_video_frame_handler_;
  mojo::Remote<mojom::Device> device_remote_;
  mojo::PendingRemote<mojom::VideoFrameHandler> mock_subscriber_;
  media::VideoCaptureParams requested_settings_;

 private:
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_TEST_MOCK_DEVICE_TEST_H_
