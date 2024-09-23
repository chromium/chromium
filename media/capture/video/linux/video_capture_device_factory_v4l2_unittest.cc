// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_factory_v4l2.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/capture/video/linux/fake_device_provider.h"
#include "media/capture/video/linux/fake_v4l2_impl.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;

namespace media {

class VideoCaptureDeviceFactoryV4L2Test
    : public ::testing::TestWithParam<VideoCaptureDeviceDescriptor> {
 public:
  VideoCaptureDeviceFactoryV4L2Test() {}
  ~VideoCaptureDeviceFactoryV4L2Test() override = default;

  void SetUp() override {
    factory_ = std::make_unique<VideoCaptureDeviceFactoryV4L2>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    scoped_refptr<FakeV4L2Impl> fake_v4l2(new FakeV4L2Impl());
    fake_v4l2_ = fake_v4l2.get();
    auto fake_device_provider = std::make_unique<FakeDeviceProvider>();
    fake_device_provider_ = fake_device_provider.get();
    factory_->SetV4L2EnvironmentForTesting(std::move(fake_v4l2),
                                           std::move(fake_device_provider));
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoCaptureDeviceFactoryV4L2> factory_;
  raw_ptr<FakeV4L2Impl> fake_v4l2_;
  raw_ptr<FakeDeviceProvider> fake_device_provider_;
};

TEST_P(VideoCaptureDeviceFactoryV4L2Test, EnumerateSingleFakeV4L2DeviceUsing) {
  // Setup
  const VideoCaptureDeviceDescriptor& descriptor = GetParam();
  fake_device_provider_->AddDevice(descriptor);
  fake_v4l2_->AddDevice(descriptor.device_id, FakeV4L2DeviceConfig(descriptor));

  // Exercise
  std::vector<media::VideoCaptureDeviceInfo> devices_info;
  base::RunLoop run_loop;
  factory_->GetDevicesInfo(base::BindLambdaForTesting(
      [&devices_info,
       &run_loop](std::vector<media::VideoCaptureDeviceInfo> result) {
        devices_info = std::move(result);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verification
  ASSERT_EQ(1u, devices_info.size());
  EXPECT_EQ(descriptor.device_id, devices_info[0].descriptor.device_id);
  EXPECT_EQ(descriptor.display_name(),
            devices_info[0].descriptor.display_name());
  EXPECT_EQ(descriptor.control_support().pan,
            devices_info[0].descriptor.control_support().pan);
  EXPECT_EQ(descriptor.control_support().tilt,
            devices_info[0].descriptor.control_support().tilt);
  EXPECT_EQ(descriptor.control_support().zoom,
            devices_info[0].descriptor.control_support().zoom);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCaptureDeviceFactoryV4L2Test,
    ::testing::Values(
        VideoCaptureDeviceDescriptor("Fake Device 0",
                                     "/dev/video0",
                                     VideoCaptureApi::UNKNOWN,
                                     /*control_support=*/{false, false, false}),
        VideoCaptureDeviceDescriptor("Fake Device 0",
                                     "/dev/video0",
                                     VideoCaptureApi::UNKNOWN,
                                     /*control_support=*/{false, false, true}),
        VideoCaptureDeviceDescriptor("Fake Device 0",
                                     "/dev/video0",
                                     VideoCaptureApi::UNKNOWN,
                                     /*control_support=*/{true, true, true})));

TEST_F(VideoCaptureDeviceFactoryV4L2Test,
       ReceiveFramesFromSinglePlaneFakeDevice) {
  // Setup
  const std::string stub_display_name = "Fake Device 0";
  const std::string stub_device_id = "/dev/video0";
  VideoCaptureDeviceDescriptor descriptor(
      stub_display_name, stub_device_id,
      VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE);
  fake_device_provider_->AddDevice(descriptor);
  fake_v4l2_->AddDevice(stub_device_id, FakeV4L2DeviceConfig(descriptor));

  // Exercise
  auto device = factory_->CreateDevice(descriptor).ReleaseDevice();
  VideoCaptureParams arbitrary_params;
  arbitrary_params.requested_format.frame_size = gfx::Size(1280, 720);
  arbitrary_params.requested_format.frame_rate = 30.0f;
  arbitrary_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  auto client = std::make_unique<NiceMockVideoCaptureDeviceClient>();
  MockVideoCaptureDeviceClient* client_ptr = client.get();

  base::RunLoop wait_loop;
  static const int kFrameToReceive = 3;
  EXPECT_CALL(*client_ptr, OnIncomingCapturedData)
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop]() {
        static int received_frame_count = 0;
        received_frame_count++;
        if (received_frame_count == kFrameToReceive) {
          wait_loop.Quit();
        }
      }));

  device->AllocateAndStart(arbitrary_params, std::move(client));
  wait_loop.Run();

  device->StopAndDeAllocate();
}

}  // namespace media
