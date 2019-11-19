// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_app_device_provider_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "media/base/bind_to_current_loop.h"

namespace media {

CameraAppDeviceProviderImpl::CameraAppDeviceProviderImpl(
    mojo::PendingRemote<cros::mojom::CameraAppDeviceBridge> bridge,
    DeviceIdMappingCallback mapping_callback)
    : bridge_(std::move(bridge)),
      mapping_callback_(std::move(mapping_callback)),
      weak_ptr_factory_(this) {}

CameraAppDeviceProviderImpl::~CameraAppDeviceProviderImpl() = default;

void CameraAppDeviceProviderImpl::GetCameraAppDevice(
    const std::string& source_id,
    GetCameraAppDeviceCallback callback) {
  mapping_callback_.Run(
      source_id,
      media::BindToCurrentLoop(base::BindOnce(
          &CameraAppDeviceProviderImpl::GetCameraAppDeviceWithDeviceId,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void CameraAppDeviceProviderImpl::GetCameraAppDeviceWithDeviceId(
    GetCameraAppDeviceCallback callback,
    const base::Optional<std::string>& device_id) {
  if (!device_id.has_value()) {
    std::move(callback).Run(
        cros::mojom::GetCameraAppDeviceStatus::ERROR_INVALID_ID,
        mojo::NullRemote());
    return;
  }

  bridge_->GetCameraAppDevice(*device_id, std::move(callback));
}

void CameraAppDeviceProviderImpl::IsSupported(IsSupportedCallback callback) {
  bridge_->IsSupported(std::move(callback));
}

}  // namespace media