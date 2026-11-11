// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_factory_linux.h"

#include <unistd.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/config/linux/dbus/buildflags.h"
#include "media/capture/capture_switches.h"
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
    ++get_devices_info_count_;
    std::vector<VideoCaptureDeviceInfo> devices_info;
    devices_info.emplace_back(
        VideoCaptureDeviceDescriptor("V4L2 camera", kV4L2DeviceId, "",
                                     VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE,
                                     VideoCaptureControlSupport()));
    std::move(callback).Run(std::move(devices_info));
  }

  int get_devices_info_count() const { return get_devices_info_count_; }

 private:
  int get_devices_info_count_ = 0;
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

#if BUILDFLAG(USE_DBUS)
// Replaces the Camera portal with a fixed result and records the PipeWire fd
// that WebRTC gets.
class FakeCameraPortalFactory : public VideoCaptureDeviceFactoryWebRtc {
 public:
  explicit FakeCameraPortalFactory(
      std::optional<dbus_xdg::CameraPortalError> error)
      : error_(error) {}
  ~FakeCameraPortalFactory() override = default;

  // The fd passed to WebRTC, or nullopt if WebRTC wasn't initialized.
  const std::optional<int>& pipewire_fd() const { return pipewire_fd_; }
  // The fd that the fake portal returned.
  int portal_fd() const { return portal_fd_; }
  // How often the Camera portal was opened.
  int portal_open_count() const { return portal_open_count_; }
  void set_error(std::optional<dbus_xdg::CameraPortalError> error) {
    error_ = error;
  }

 protected:
  void OpenCameraPortal(dbus_xdg::CameraPortalCallback callback) override {
    ++portal_open_count_;
    dbus_xdg::CameraPortalResult result =
        base::unexpected(dbus_xdg::CameraPortalError::kFailed);
    if (error_) {
      result = base::unexpected(*error_);
    } else {
      base::ScopedFD read_end;
      base::ScopedFD write_end;
      CHECK(base::CreatePipe(&read_end, &write_end));
      portal_fd_ = read_end.get();
      result = std::move(read_end);
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  }

  void InitOptions(int pipewire_fd) override {
    pipewire_fd_ = pipewire_fd;
    // Take ownership like WebRTC does.
    base::ScopedFD fd(pipewire_fd);
    OnInitialized(webrtc::VideoCaptureOptions::Status::ERROR);
  }

 private:
  std::optional<dbus_xdg::CameraPortalError> error_;
  std::optional<int> pipewire_fd_;
  int portal_fd_ = -1;
  int portal_open_count_ = 0;
};

class VideoCaptureDeviceFactoryWebRtcCameraPortalTest : public testing::Test {
 protected:
  void GetDevicesInfo(VideoCaptureDeviceFactory& factory) {
    base::RunLoop run_loop;
    factory.GetDevicesInfo(base::BindLambdaForTesting(
        [&](std::vector<VideoCaptureDeviceInfo> result) { run_loop.Quit(); }));
    run_loop.Run();
  }

  base::test::ScopedFeatureList feature_list_{features::kWebRtcPipeWireCamera};
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// The PipeWire fd from the Camera portal connection is passed to WebRTC.
TEST_F(VideoCaptureDeviceFactoryWebRtcCameraPortalTest, PassesFdToWebRtc) {
  FakeCameraPortalFactory factory(std::nullopt);
  GetDevicesInfo(factory);
  ASSERT_TRUE(factory.pipewire_fd());
  EXPECT_NE(*factory.pipewire_fd(), -1);
  EXPECT_EQ(*factory.pipewire_fd(), factory.portal_fd());
}

// When the user denies access, WebRTC doesn't open the portal again.
TEST_F(VideoCaptureDeviceFactoryWebRtcCameraPortalTest, DeniedSkipsWebRtc) {
  FakeCameraPortalFactory factory(dbus_xdg::CameraPortalError::kDenied);
  GetDevicesInfo(factory);
  EXPECT_FALSE(factory.pipewire_fd());
  EXPECT_FALSE(factory.IsAvailable());
}

// When the dedicated connection fails, WebRTC opens the portal itself.
TEST_F(VideoCaptureDeviceFactoryWebRtcCameraPortalTest,
       FailureFallsBackToWebRtcPortal) {
  FakeCameraPortalFactory factory(dbus_xdg::CameraPortalError::kFailed);
  GetDevicesInfo(factory);
  ASSERT_TRUE(factory.pipewire_fd());
  EXPECT_EQ(*factory.pipewire_fd(), -1);
  EXPECT_FALSE(factory.IsPortalRetryPending());
}

// After a denial, the next enumeration opens the portal once more, e.g. after
// GNOME Shell refused the dialog because no window was focused yet.
TEST_F(VideoCaptureDeviceFactoryWebRtcCameraPortalTest,
       DeniedRetriesPortalOnce) {
  FakeCameraPortalFactory factory(dbus_xdg::CameraPortalError::kDenied);
  GetDevicesInfo(factory);
  EXPECT_EQ(factory.portal_open_count(), 1);
  EXPECT_FALSE(factory.IsAvailable());
  EXPECT_TRUE(factory.IsPortalRetryPending());

  factory.set_error(std::nullopt);
  GetDevicesInfo(factory);
  EXPECT_EQ(factory.portal_open_count(), 2);
  EXPECT_FALSE(factory.IsPortalRetryPending());
  ASSERT_TRUE(factory.pipewire_fd());
  EXPECT_EQ(*factory.pipewire_fd(), factory.portal_fd());
}

// A denial of the retry is final.
TEST_F(VideoCaptureDeviceFactoryWebRtcCameraPortalTest,
       DeniedRetryIsNotRepeated) {
  FakeCameraPortalFactory factory(dbus_xdg::CameraPortalError::kDenied);
  GetDevicesInfo(factory);
  GetDevicesInfo(factory);
  EXPECT_EQ(factory.portal_open_count(), 2);
  EXPECT_FALSE(factory.IsPortalRetryPending());

  GetDevicesInfo(factory);
  EXPECT_EQ(factory.portal_open_count(), 2);
  EXPECT_FALSE(factory.IsAvailable());
}

// The Linux factory asks the WebRTC factory again while a retry is pending,
// and uses V4L2 when the retry is denied too.
TEST_F(VideoCaptureDeviceFactoryWebRtcCameraPortalTest,
       LinuxFactoryRetriesPortal) {
  auto webrtc_factory = std::make_unique<FakeCameraPortalFactory>(
      dbus_xdg::CameraPortalError::kDenied);
  FakeCameraPortalFactory* webrtc_factory_ptr = webrtc_factory.get();
  auto v4l2_factory = std::make_unique<FakeV4L2Factory>();
  FakeV4L2Factory* v4l2_factory_ptr = v4l2_factory.get();
  VideoCaptureDeviceFactoryLinux factory(std::move(v4l2_factory),
                                         std::move(webrtc_factory));

  GetDevicesInfo(factory);
  EXPECT_EQ(webrtc_factory_ptr->portal_open_count(), 1);
  EXPECT_EQ(v4l2_factory_ptr->get_devices_info_count(), 1);

  GetDevicesInfo(factory);
  EXPECT_EQ(webrtc_factory_ptr->portal_open_count(), 2);
  EXPECT_EQ(v4l2_factory_ptr->get_devices_info_count(), 2);

  GetDevicesInfo(factory);
  EXPECT_EQ(webrtc_factory_ptr->portal_open_count(), 2);
  EXPECT_EQ(v4l2_factory_ptr->get_devices_info_count(), 3);
}

// Concurrent GetDevicesInfo() calls during an in-flight portal request share
// that request and both receive a callback.
TEST_F(VideoCaptureDeviceFactoryWebRtcCameraPortalTest,
       ConcurrentGetDevicesInfoQueuesCallbacks) {
  FakeCameraPortalFactory factory(std::nullopt);
  int callbacks_run = 0;
  base::RunLoop run_loop;
  auto on_done = [&](std::vector<VideoCaptureDeviceInfo>) {
    if (++callbacks_run == 2) {
      run_loop.Quit();
    }
  };
  factory.GetDevicesInfo(base::BindLambdaForTesting(on_done));
  factory.GetDevicesInfo(base::BindLambdaForTesting(on_done));
  run_loop.Run();

  EXPECT_EQ(callbacks_run, 2);
  EXPECT_EQ(factory.portal_open_count(), 1);
}
#endif  // BUILDFLAG(USE_DBUS)

}  // namespace

}  // namespace media
