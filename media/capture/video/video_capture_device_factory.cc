// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device_factory.h"

#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/file_video_capture_device_factory.h"

namespace media {

VideoCaptureDeviceFactory::VideoCaptureDeviceFactory() {
  thread_checker_.DetachFromThread();
}

VideoCaptureDeviceFactory::~VideoCaptureDeviceFactory() = default;

void VideoCaptureDeviceFactory::GetCameraLocationsAsync(
    std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
    DeviceDescriptorsCallback result_callback) {
  NOTIMPLEMENTED();
}

#if defined(OS_CHROMEOS)
bool VideoCaptureDeviceFactory::IsSupportedCameraAppDeviceBridge() {
  return false;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace media
