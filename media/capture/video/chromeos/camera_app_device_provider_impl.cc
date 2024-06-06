// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_app_device_provider_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/webui/camera_app_ui/document_scanner_service_host.h"
#include "base/task/bind_post_task.h"

namespace media {

CameraAppDeviceProviderImpl::CameraAppDeviceProviderImpl(
    ConnectToBridgeCallback connect_to_bridge_callback,
    DeviceIdMappingCallback mapping_callback)
    : connect_to_bridge_callback_(std::move(connect_to_bridge_callback)),
      mapping_callback_(std::move(mapping_callback)),
      weak_ptr_factory_(this) {
  ash::DocumentScannerServiceHost::GetInstance()->Start();
  ConnectToCameraAppDeviceBridge();
}

CameraAppDeviceProviderImpl::~CameraAppDeviceProviderImpl() = default;

void CameraAppDeviceProviderImpl::Bind(
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void CameraAppDeviceProviderImpl::GetCameraAppDevice(
    const std::string& source_id,
    GetCameraAppDeviceCallback callback) {
  mapping_callback_.Run(
      source_id,
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &CameraAppDeviceProviderImpl::GetCameraAppDeviceWithDeviceId,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void CameraAppDeviceProviderImpl::GetCameraAppDeviceWithDeviceId(
    GetCameraAppDeviceCallback callback,
    const std::optional<std::string>& device_id) {
  if (!device_id.has_value()) {
    std::move(callback).Run(
        cros::mojom::GetCameraAppDeviceStatus::kErrorInvalidId,
        mojo::NullRemote());
    return;
  }

  bridge_->GetCameraAppDevice(*device_id, std::move(callback));
}

void CameraAppDeviceProviderImpl::IsSupported(IsSupportedCallback callback) {
  bridge_->IsSupported(std::move(callback));
}

void CameraAppDeviceProviderImpl::IsDeviceInUse(
    const std::string& source_id,
    IsDeviceInUseCallback callback) {
  mapping_callback_.Run(
      source_id, base::BindPostTaskToCurrentDefault(base::BindOnce(
                     &CameraAppDeviceProviderImpl::IsDeviceInUseWithDeviceId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void CameraAppDeviceProviderImpl::IsDeviceInUseWithDeviceId(
    IsDeviceInUseCallback callback,
    const std::optional<std::string>& device_id) {
  if (!device_id.has_value()) {
    std::move(callback).Run(false);
    return;
  }
  bridge_->IsDeviceInUse(*device_id, std::move(callback));
}

void CameraAppDeviceProviderImpl::ConnectToCameraAppDeviceBridge() {
  bridge_.reset();
  connect_to_bridge_callback_.Run(bridge_.BindNewPipeAndPassReceiver());
  bridge_.set_disconnect_handler(base::BindOnce(
      &CameraAppDeviceProviderImpl::ConnectToCameraAppDeviceBridge,
      weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace media
