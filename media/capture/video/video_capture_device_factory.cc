// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device_factory.h"

#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/file_video_capture_device_factory.h"

namespace media {

VideoCaptureErrorOrDevice::VideoCaptureErrorOrDevice(
    std::unique_ptr<VideoCaptureDevice> video_device)
    : device_(std::move(video_device)), error_code_(VideoCaptureError::kNone) {}

VideoCaptureErrorOrDevice::VideoCaptureErrorOrDevice(VideoCaptureError err_code)
    : error_code_(err_code) {
  DCHECK_NE(error_code_, VideoCaptureError::kNone);
}

VideoCaptureErrorOrDevice::~VideoCaptureErrorOrDevice() = default;

VideoCaptureErrorOrDevice::VideoCaptureErrorOrDevice(
    VideoCaptureErrorOrDevice&& other)
    : device_(std::move(other.device_)), error_code_(other.error_code_) {}

std::unique_ptr<VideoCaptureDevice> VideoCaptureErrorOrDevice::ReleaseDevice() {
  DCHECK_EQ(error_code_, VideoCaptureError::kNone);

  error_code_ = VideoCaptureError::kVideoCaptureDeviceAlreadyReleased;
  return std::move(device_);
}

VideoCaptureDeviceFactory::VideoCaptureDeviceFactory() {
  thread_checker_.DetachFromThread();
}

VideoCaptureDeviceFactory::~VideoCaptureDeviceFactory() = default;

#if BUILDFLAG(IS_WIN)
scoped_refptr<DXGIDeviceManager>
VideoCaptureDeviceFactory::GetDxgiDeviceManager() {
  return nullptr;
}

void VideoCaptureDeviceFactory::OnGpuInfoUpdate(const CHROME_LUID& luid) {}
#endif

}  // namespace media
