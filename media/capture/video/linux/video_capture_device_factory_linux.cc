// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_factory_linux.h"

#include "base/feature_list.h"
#include "media/capture/capture_switches.h"
#include "media/capture/video/linux/video_capture_device_factory_v4l2.h"
#if defined(WEBRTC_USE_PIPEWIRE)
#include "media/capture/video/linux/video_capture_device_factory_webrtc.h"
#endif  // defined(WEBRTC_USE_PIPEWIRE)

namespace media {

VideoCaptureDeviceFactoryLinux::VideoCaptureDeviceFactoryLinux(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : factory_(std::make_unique<VideoCaptureDeviceFactoryV4L2>(ui_task_runner))
#if defined(WEBRTC_USE_PIPEWIRE)
      ,
      webrtc_factory_(std::make_unique<VideoCaptureDeviceFactoryWebRtc>())
#endif  // defined(WEBRTC_USE_PIPEWIRE)
{
}

#if defined(WEBRTC_USE_PIPEWIRE)
VideoCaptureDeviceFactoryLinux::VideoCaptureDeviceFactoryLinux(
    std::unique_ptr<VideoCaptureDeviceFactory> factory,
    std::unique_ptr<VideoCaptureDeviceFactoryWebRtc> webrtc_factory)
    : factory_(std::move(factory)),
      webrtc_factory_(std::move(webrtc_factory)) {}
#endif  // defined(WEBRTC_USE_PIPEWIRE)

VideoCaptureDeviceFactoryLinux::~VideoCaptureDeviceFactoryLinux() = default;

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryLinux::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
#if defined(WEBRTC_USE_PIPEWIRE)
  if (webrtc_factory_->IsAvailable() &&
      device_descriptor.capture_api ==
          VideoCaptureApi::WEBRTC_LINUX_PIPEWIRE_SINGLE_PLANE) {
    CHECK(base::FeatureList::IsEnabled(features::kWebRtcPipeWireCamera));
    return webrtc_factory_->CreateDevice(device_descriptor);
  }
#endif  // defined(WEBRTC_USE_PIPEWIRE)
  if (device_descriptor.capture_api ==
      VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE) {
    return factory_->CreateDevice(device_descriptor);
  }
  return VideoCaptureErrorOrDevice(
      VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
}

void VideoCaptureDeviceFactoryLinux::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
#if defined(WEBRTC_USE_PIPEWIRE)
  if (webrtc_factory_->IsAvailable() ||
      webrtc_factory_->IsPortalRetryPending()) {
    webrtc_factory_->GetDevicesInfo(
        base::BindOnce(&VideoCaptureDeviceFactoryLinux::OnGetDevicesInfo,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
#endif  // defined(WEBRTC_USE_PIPEWIRE)
  factory_->GetDevicesInfo(std::move(callback));
}

#if defined(WEBRTC_USE_PIPEWIRE)
void VideoCaptureDeviceFactoryLinux::OnGetDevicesInfo(
    GetDevicesInfoCallback callback,
    std::vector<VideoCaptureDeviceInfo> devices_info) {
  // IsAvailable() can change from true to false during device enumeration,
  // e.g. when the Camera portal is missing, fails or denies access. Check again
  // to see if the V4L2 factory is needed. Also fall back when PipeWire lists no
  // cameras, so that cameras V4L2 can open don't disappear.
  if (webrtc_factory_->IsAvailable() && !devices_info.empty()) {
    std::move(callback).Run(std::move(devices_info));
  } else {
    factory_->GetDevicesInfo(std::move(callback));
  }
}
#endif  // defined(WEBRTC_USE_PIPEWIRE)

}  // namespace media
