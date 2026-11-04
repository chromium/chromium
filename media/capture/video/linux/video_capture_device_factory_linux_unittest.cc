// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_factory_linux.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/capture/video/linux/video_capture_device_factory_webrtc.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr char kV4L2DeviceId[] = "/dev/video0";

// Lists one V4L2 camera.
class FakeV4L2Factory : public VideoCaptureDeviceFactory {
 public:
  FakeV4L2Factory() = default;
  ~FakeV4L2Factory() override = default;

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override {
    return VideoCaptureErrorOrDevice(
        VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
  }

  void GetDevicesInfo(GetDevicesInfoCallback callback) override {
    std::vector<VideoCaptureDeviceInfo> devices_info;
    devices_info.emplace_back(
        VideoCaptureDeviceDescriptor("V4L2 camera", kV4L2DeviceId, "",
                                     VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE,
                                     VideoCaptureControlSupport()));
    std::move(callback).Run(std::move(devices_info));
  }
};

// Finishes PipeWire and Camera portal initialization with a fixed status and
// without any PipeWire cameras.
class FakeWebRtcFactory : public VideoCaptureDeviceFactoryWebRtc {
 public:
  explicit FakeWebRtcFactory(webrtc::VideoCaptureOptions::Status status)
      : status_(status) {}
  ~FakeWebRtcFactory() override = default;

 protected:
  void InitializeOptions() override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FakeWebRtcFactory::OnInitialized,
                                  weak_ptr_factory_.GetWeakPtr(), status_));
  }

 private:
  const webrtc::VideoCaptureOptions::Status status_;
  base::WeakPtrFactory<FakeWebRtcFactory> weak_ptr_factory_{this};
};

class VideoCaptureDeviceFactoryLinuxTest
    : public ::testing::TestWithParam<webrtc::VideoCaptureOptions::Status> {
 protected:
  std::vector<VideoCaptureDeviceInfo> GetDevicesInfo(
      webrtc::VideoCaptureOptions::Status status) {
    VideoCaptureDeviceFactoryLinux factory(
        std::make_unique<FakeV4L2Factory>(),
        std::make_unique<FakeWebRtcFactory>(status));
    std::vector<VideoCaptureDeviceInfo> devices_info;
    base::RunLoop run_loop;
    factory.GetDevicesInfo(base::BindLambdaForTesting(
        [&](std::vector<VideoCaptureDeviceInfo> result) {
          devices_info = std::move(result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return devices_info;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

// V4L2 cameras are listed when PipeWire can't be used (UNAVAILABLE), when the
// Camera portal denies access (DENIED) or is missing or fails (ERROR), and when
// PipeWire lists no cameras (SUCCESS here).
TEST_P(VideoCaptureDeviceFactoryLinuxTest, FallsBackToV4L2) {
  std::vector<VideoCaptureDeviceInfo> devices_info = GetDevicesInfo(GetParam());
  ASSERT_EQ(devices_info.size(), 1u);
  EXPECT_EQ(devices_info[0].descriptor.device_id, kV4L2DeviceId);
  EXPECT_EQ(devices_info[0].descriptor.capture_api,
            VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCaptureDeviceFactoryLinuxTest,
    ::testing::Values(webrtc::VideoCaptureOptions::Status::UNAVAILABLE,
                      webrtc::VideoCaptureOptions::Status::DENIED,
                      webrtc::VideoCaptureOptions::Status::ERROR,
                      webrtc::VideoCaptureOptions::Status::SUCCESS));

}  // namespace

}  // namespace media
