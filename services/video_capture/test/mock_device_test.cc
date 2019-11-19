// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/test/mock_device_test.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/capture/video/video_capture_system_impl.h"

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace video_capture {

MockDeviceTest::MockDeviceTest() = default;

MockDeviceTest::~MockDeviceTest() = default;

void MockDeviceTest::SetUp() {
  task_environment_ =
      std::make_unique<base::test::SingleThreadTaskEnvironment>();
  auto mock_device_factory = std::make_unique<media::MockDeviceFactory>();
  // We keep a pointer to the MockDeviceFactory as a member so that we can
  // invoke its AddMockDevice(). Ownership of the MockDeviceFactory is moved
  // to the DeviceFactoryMediaToMojoAdapter.
  mock_device_factory_ = mock_device_factory.get();
  auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
      std::move(mock_device_factory));
#if defined(OS_CHROMEOS)
  mock_device_factory_adapter_ =
      std::make_unique<DeviceFactoryMediaToMojoAdapter>(
          std::move(video_capture_system), base::DoNothing(),
          base::ThreadTaskRunnerHandle::Get());
#else
  mock_device_factory_adapter_ =
      std::make_unique<DeviceFactoryMediaToMojoAdapter>(
          std::move(video_capture_system));
#endif  // defined(OS_CHROMEOS)

  mock_factory_receiver_ =
      std::make_unique<mojo::Receiver<mojom::DeviceFactory>>(
          mock_device_factory_adapter_.get(),
          factory_.BindNewPipeAndPassReceiver());

  media::VideoCaptureDeviceDescriptor mock_descriptor;
  mock_descriptor.device_id = "MockDeviceId";
  mock_device_factory_->AddMockDevice(&mock_device_, mock_descriptor);

  // Obtain the mock device from the factory
  base::RunLoop wait_loop;
  EXPECT_CALL(device_infos_receiver_, Run(_))
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  factory_->GetDeviceInfos(device_infos_receiver_.Get());
  // We must wait for the response to GetDeviceInfos before calling
  // CreateDevice.
  wait_loop.Run();
  factory_->CreateDevice(mock_descriptor.device_id,
                         device_remote_.BindNewPipeAndPassReceiver(),
                         base::DoNothing());

  requested_settings_.requested_format.frame_size = gfx::Size(800, 600);
  requested_settings_.requested_format.frame_rate = 15;
  requested_settings_.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_RESOLUTION;
  requested_settings_.power_line_frequency =
      media::PowerLineFrequency::FREQUENCY_DEFAULT;

  mock_video_frame_handler_ = std::make_unique<MockVideoFrameHandler>(
      mock_subscriber_.InitWithNewPipeAndPassReceiver());
}

}  // namespace video_capture
