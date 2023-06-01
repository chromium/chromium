// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_factory_linux.h"

#include "media/capture/video/linux/video_capture_device_factory_v4l2.h"
#include "media/capture/video/video_capture_metrics.h"

namespace media {

VideoCaptureDeviceFactoryLinux::VideoCaptureDeviceFactoryLinux(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : factory_(
          std::make_unique<VideoCaptureDeviceFactoryV4L2>(ui_task_runner)) {}

VideoCaptureDeviceFactoryLinux::~VideoCaptureDeviceFactoryLinux() = default;

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryLinux::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LogCaptureDeviceHashedModelId(device_descriptor);
  return factory_->CreateDevice(device_descriptor);
}

void VideoCaptureDeviceFactoryLinux::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  factory_->GetDevicesInfo(std::move(callback));
}

}  // namespace media
