// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/ash/video_capture_device_factory_ash.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "services/video_capture/ash/video_capture_device_ash.h"

namespace crosapi {

VideoCaptureDeviceFactoryAsh::VideoCaptureDeviceFactoryAsh(
    video_capture::DeviceFactory* device_factory)
    : device_factory_(device_factory) {}

VideoCaptureDeviceFactoryAsh::~VideoCaptureDeviceFactoryAsh() = default;

void VideoCaptureDeviceFactoryAsh::GetDeviceInfos(
    GetDeviceInfosCallback callback) {
  device_factory_->GetDeviceInfos(std::move(callback));
}

void VideoCaptureDeviceFactoryAsh::CreateDevice(
    const std::string& device_id,
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDevice> device_receiver,
    CreateDeviceCallback callback) {
  device_factory_->CreateDevice(
      device_id, base::BindOnce(&VideoCaptureDeviceFactoryAsh::OnDeviceCreated,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                std::move(device_receiver), device_id));
}

void VideoCaptureDeviceFactoryAsh::OnDeviceCreated(
    CreateDeviceCallback callback,
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDevice> device_receiver,
    const std::string& device_id,
    video_capture::DeviceFactory::DeviceInfo device_info) {
  crosapi::mojom::DeviceAccessResultCode crosapi_result_code;
  switch (device_info.result_code) {
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice:
      crosapi_result_code =
          crosapi::mojom::DeviceAccessResultCode::NOT_INITIALIZED;
      break;
    case media::VideoCaptureError::kNone:
      crosapi_result_code = crosapi::mojom::DeviceAccessResultCode::SUCCESS;
      break;
    default:
      crosapi_result_code =
          crosapi::mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND;
  }

  if (device_info.result_code == media::VideoCaptureError::kNone) {
    auto device_proxy = std::make_unique<VideoCaptureDeviceAsh>(
        std::move(device_receiver), device_info.device,
        base::BindOnce(
            &VideoCaptureDeviceFactoryAsh::OnClientConnectionErrorOrClose,
            weak_factory_.GetWeakPtr(), device_id));

    devices_.emplace(device_id, std::move(device_proxy));
  }

  std::move(callback).Run(crosapi_result_code);
}

void VideoCaptureDeviceFactoryAsh::OnClientConnectionErrorOrClose(
    const std::string& device_id) {
  devices_.erase(device_id);
  device_factory_->StopDevice(device_id);
}

}  // namespace crosapi
