// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/create_video_capture_device_factory.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/file_video_capture_device_factory.h"

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "media/capture/video/linux/video_capture_device_factory_linux.h"
#elif defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#include "media/capture/video/linux/video_capture_device_factory_linux.h"
#elif defined(OS_WIN)
#include "media/capture/video/win/video_capture_device_factory_win.h"
#elif defined(OS_MACOSX)
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#elif defined(OS_ANDROID)
#include "media/capture/video/android/video_capture_device_factory_android.h"
#elif defined(OS_FUCHSIA)
#include "media/capture/video/fuchsia/video_capture_device_factory_fuchsia.h"
#endif

namespace media {

namespace {

// Returns null if the corresponding switch is off.
std::unique_ptr<VideoCaptureDeviceFactory>
CreateFakeVideoCaptureDeviceFactory() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // Use a Fake or File Video Device Factory if the command line flags are
  // present, otherwise use the normal, platform-dependent, device factory.
  if (command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream)) {
    if (command_line->HasSwitch(switches::kUseFileForFakeVideoCapture)) {
      return std::make_unique<FileVideoCaptureDeviceFactory>();
    } else {
      std::vector<FakeVideoCaptureDeviceSettings> config;
      FakeVideoCaptureDeviceFactory::ParseFakeDevicesConfigFromOptionsString(
          command_line->GetSwitchValueASCII(
              switches::kUseFakeDeviceForMediaStream),
          &config);
      auto result = std::make_unique<FakeVideoCaptureDeviceFactory>();
      result->SetToCustomDevicesConfig(config);
      return std::move(result);
    }
  } else {
    return nullptr;
  }
}

#if defined(OS_CHROMEOS)
std::unique_ptr<VideoCaptureDeviceFactory>
CreateChromeOSVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    media::CameraAppDeviceBridgeImpl* camera_app_device_bridge) {
  // On Chrome OS we have to support two use cases:
  //
  // 1. For devices that have the camera HAL v3 service running on Chrome OS,
  //    we use the HAL v3 capture device which VideoCaptureDeviceFactoryChromeOS
  //    provides.
  // 2. Existing devices that use UVC cameras need to use the V4L2 capture
  //    device which VideoCaptureDeviceFacotoryLinux provides; there are also
  //    some special devices that may never be able to implement a camera HAL
  //    v3.
  if (ShouldUseCrosCameraService()) {
    return std::make_unique<VideoCaptureDeviceFactoryChromeOS>(
        ui_task_runner, camera_app_device_bridge);
  } else {
    return std::make_unique<VideoCaptureDeviceFactoryLinux>(ui_task_runner);
  }
}
#endif  // defined(OS_CHROMEOS)

std::unique_ptr<VideoCaptureDeviceFactory>
CreatePlatformSpecificVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  return std::make_unique<VideoCaptureDeviceFactoryLinux>(ui_task_runner);
#elif defined(OS_CHROMEOS)
  return CreateChromeOSVideoCaptureDeviceFactory(ui_task_runner, {});
#elif defined(OS_WIN)
  return std::make_unique<VideoCaptureDeviceFactoryWin>();
#elif defined(OS_MACOSX)
  return std::make_unique<VideoCaptureDeviceFactoryMac>();
#elif defined(OS_ANDROID)
  return std::make_unique<VideoCaptureDeviceFactoryAndroid>();
#elif defined(OS_FUCHSIA)
  return std::make_unique<VideoCaptureDeviceFactoryFuchsia>();
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

}  // anonymous namespace

std::unique_ptr<VideoCaptureDeviceFactory> CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  auto fake_device_factory = CreateFakeVideoCaptureDeviceFactory();
  if (fake_device_factory) {
    return fake_device_factory;
  } else {
    // |ui_task_runner| is needed for the Linux ChromeOS factory to retrieve
    // screen rotations.
    return CreatePlatformSpecificVideoCaptureDeviceFactory(ui_task_runner);
  }
}

#if defined(OS_CHROMEOS)
std::unique_ptr<VideoCaptureDeviceFactory> CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    media::CameraAppDeviceBridgeImpl* camera_app_device_bridge) {
  auto fake_device_factory = CreateFakeVideoCaptureDeviceFactory();
  if (fake_device_factory) {
    return fake_device_factory;
  } else {
    // |ui_task_runner| is needed for the Linux ChromeOS factory to retrieve
    // screen rotations.
    return CreateChromeOSVideoCaptureDeviceFactory(ui_task_runner,
                                                   camera_app_device_bridge);
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace media
