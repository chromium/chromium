// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_video_capture_device_test.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/video/video_capture_system_impl.h"

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace video_capture {

MockVideoCaptureDeviceTest::MockVideoCaptureDeviceTest() = default;

MockVideoCaptureDeviceTest::~MockVideoCaptureDeviceTest() = default;

void MockVideoCaptureDeviceTest::SetUp() {
  task_environment_ =
      std::make_unique<base::test::SingleThreadTaskEnvironment>();
  auto mock_device_factory = std::make_unique<media::MockDeviceFactory>();
  // We keep a pointer to the MockDeviceFactory as a member so that we can
  // invoke its AddMockDevice(). Ownership of the MockDeviceFactory is moved
  // to the DeviceFactoryImpl.
  mock_device_factory_ = mock_device_factory.get();
  auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
      std::move(mock_device_factory));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  mock_device_factory_adapter_ = std::make_unique<DeviceFactoryImpl>(
      std::move(video_capture_system), base::DoNothing(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
#else
  mock_device_factory_adapter_ =
      std::make_unique<DeviceFactoryImpl>(std::move(video_capture_system));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  media::VideoCaptureDeviceDescriptor mock_descriptor;
  mock_descriptor.device_id = "MockDeviceId";
  mock_device_factory_->AddMockDevice(&mock_device_, mock_descriptor);

  // Obtain the mock device from the factory
  base::RunLoop wait_loop;
  EXPECT_CALL(device_infos_receiver_, Run(_))
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  mock_device_factory_adapter_->GetDeviceInfos(device_infos_receiver_.Get());
  // We must wait for the response to GetDeviceInfos before calling
  // CreateDevice.
  wait_loop.Run();
  base::RunLoop wait_loop_create_device;
  mock_device_factory_adapter_->CreateDevice(
      mock_descriptor.device_id,
      base::BindLambdaForTesting([&](DeviceFactory::DeviceInfo info) {
        ASSERT_EQ(info.result_code, media::VideoCaptureError::kNone);
        device_ = info.device;
        wait_loop_create_device.Quit();
      }));

  wait_loop_create_device.Run();
  requested_settings_.requested_format.frame_size = gfx::Size(800, 600);
  requested_settings_.requested_format.frame_rate = 15;
  requested_settings_.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_RESOLUTION;
  requested_settings_.power_line_frequency =
      media::PowerLineFrequency::kDefault;

  mock_video_frame_handler_ = std::make_unique<MockVideoFrameHandler>(
      mock_subscriber_.InitWithNewPipeAndPassReceiver());
}

}  // namespace video_capture
