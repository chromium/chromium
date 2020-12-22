// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"

#include <string>

#include "base/command_line.h"
#include "media/base/media_switches.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "media/capture/video/chromeos/video_capture_device_chromeos_halv3.h"

namespace media {

CameraAppDeviceBridgeImpl::CameraAppDeviceBridgeImpl() {}

CameraAppDeviceBridgeImpl::~CameraAppDeviceBridgeImpl() = default;

void CameraAppDeviceBridgeImpl::SetIsSupported(bool is_supported) {
  is_supported_ = is_supported;
}

void CameraAppDeviceBridgeImpl::BindReceiver(
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CameraAppDeviceBridgeImpl::OnDeviceClosed(const std::string& device_id) {
  auto it = camera_app_devices_.find(device_id);
  if (it != camera_app_devices_.end()) {
    camera_app_devices_.erase(it);
  }
}

void CameraAppDeviceBridgeImpl::SetCameraInfoGetter(
    CameraInfoGetter camera_info_getter) {
  camera_info_getter_ = std::move(camera_info_getter);
}

void CameraAppDeviceBridgeImpl::UnsetCameraInfoGetter() {
  camera_info_getter_ = {};
}

CameraAppDeviceImpl* CameraAppDeviceBridgeImpl::GetCameraAppDevice(
    const std::string& device_id) {
  auto it = camera_app_devices_.find(device_id);
  if (it != camera_app_devices_.end()) {
    return it->second.get();
  }
  return CreateCameraAppDevice(device_id);
}

void CameraAppDeviceBridgeImpl::GetCameraAppDevice(
    const std::string& device_id,
    GetCameraAppDeviceCallback callback) {
  DCHECK(is_supported_);

  mojo::PendingRemote<cros::mojom::CameraAppDevice> device;
  GetCameraAppDevice(device_id)->BindReceiver(
      device.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(cros::mojom::GetCameraAppDeviceStatus::SUCCESS,
                          std::move(device));
}

media::CameraAppDeviceImpl* CameraAppDeviceBridgeImpl::CreateCameraAppDevice(
    const std::string& device_id) {
  DCHECK(camera_info_getter_);
  auto device_info = camera_info_getter_.Run(device_id);
  auto device_impl = std::make_unique<media::CameraAppDeviceImpl>(
      device_id, std::move(device_info));
  auto result = camera_app_devices_.emplace(device_id, std::move(device_impl));
  return result.first->second.get();
}

void CameraAppDeviceBridgeImpl::IsSupported(IsSupportedCallback callback) {
  std::move(callback).Run(is_supported_);
}

}  // namespace media
