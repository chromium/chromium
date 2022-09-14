// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fuchsia/video_capture_device_factory_fuchsia.h"

#include "base/fuchsia/test_component_context_for_process.h"
#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/fuchsia/camera/fake_fuchsia_camera.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class VideoCaptureDeviceFactoryFuchsiaTest : public testing::Test {
 protected:
  std::vector<VideoCaptureDeviceInfo> GetDevicesInfo() {
    std::vector<VideoCaptureDeviceInfo> devices_info;
    base::RunLoop run_loop;
    device_factory_.GetDevicesInfo(base::BindLambdaForTesting(
        [&devices_info, &run_loop](std::vector<VideoCaptureDeviceInfo> result) {
          devices_info = std::move(result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return devices_info;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::TestComponentContextForProcess test_context_;

  FakeCameraDeviceWatcher fake_device_watcher_{
      test_context_.additional_services()};

  VideoCaptureDeviceFactoryFuchsia device_factory_;
};

TEST_F(VideoCaptureDeviceFactoryFuchsiaTest, EnumerateDevices) {
  auto devices_info = GetDevicesInfo();

  EXPECT_EQ(devices_info.size(), 1U);
}

TEST_F(VideoCaptureDeviceFactoryFuchsiaTest, EnumerateDevicesAfterDisconnect) {
  auto devices_info = GetDevicesInfo();
  EXPECT_EQ(devices_info.size(), 1U);
  devices_info.clear();

  // Disconnect DeviceWatcher and run the run loop so |device_factory_| can
  // handle the disconnect.
  fake_device_watcher_.DisconnectClients();
  base::RunLoop().RunUntilIdle();

  // Try enumerating devices again. DeviceWatcher is expected to be reconnected.
  devices_info = GetDevicesInfo();

  EXPECT_EQ(devices_info.size(), 1U);
}

// Verify that device removal is handled correctly if device info requests are
// still pending. See crbug.com/1185899 .
TEST_F(VideoCaptureDeviceFactoryFuchsiaTest, RemoveWhileEnumerating) {
  std::vector<VideoCaptureDeviceInfo> devices_info;

  // Set handler for the default device's GetIdentifier() which stops the
  // RunLoop but doesn't respond to GetIdentifier().
  base::RunLoop get_identifier_run_loop;
  fake_device_watcher_.devices().begin()->second->SetGetIdentifierHandler(
      base::BindLambdaForTesting(
          [&get_identifier_run_loop](
              fuchsia::camera3::Device::GetIdentifierCallback callback) {
            get_identifier_run_loop.Quit();
          }));

  base::RunLoop get_devices_run_loop;
  device_factory_.GetDevicesInfo(base::BindLambdaForTesting(
      [&get_devices_run_loop,
       &devices_info](std::vector<VideoCaptureDeviceInfo> result) {
        devices_info = std::move(result);
        get_devices_run_loop.Quit();
      }));

  get_identifier_run_loop.Run();

  // Remove the first device. This will unblock GetDevicesInfo() (blocked on
  // GetIdentifier() for the device being removed). The result is not dropped
  // immediately to ensure that the Device connection is not dropped.
  auto stream_and_device = fake_device_watcher_.RemoveDevice(
      fake_device_watcher_.devices().begin()->first);

  get_devices_run_loop.Run();
  EXPECT_TRUE(devices_info.empty());
}

class TestDeviceChangeObserver
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  TestDeviceChangeObserver() = default;
  ~TestDeviceChangeObserver() override = default;

  // DevicesChangedObserver implementation.
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) final {
    EXPECT_EQ(device_type, base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
    ++num_events_;
  }

  size_t num_events() { return num_events_; }

 private:
  size_t num_events_ = 0;
};

TEST_F(VideoCaptureDeviceFactoryFuchsiaTest, DeviceChangeEvent) {
  base::SystemMonitor system_monitor;
  TestDeviceChangeObserver test_observer;
  system_monitor.AddDevicesChangedObserver(&test_observer);

  // DevicesChanged event should not be produced when the list of devices is
  // fetched for the first time.
  auto devices_info = GetDevicesInfo();
  EXPECT_EQ(test_observer.num_events(), 0U);

  // Remove the first camera device. The factory is expected to notify
  // SystemMonitor about the change.
  fake_device_watcher_.RemoveDevice(
      fake_device_watcher_.devices().begin()->first);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(test_observer.num_events(), 1U);
}

}  // namespace media
