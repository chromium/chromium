// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/lacros/device_factory_adapter_lacros.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/lacros/device_proxy_lacros.h"

namespace video_capture {

DeviceFactoryAdapterLacros::DeviceFactoryAdapterLacros(
    mojo::PendingRemote<crosapi::mojom::VideoCaptureDeviceFactory>
        device_factory_ash,
    base::OnceClosure cleanup_callback)
    : device_factory_ash_(std::move(device_factory_ash)) {
  device_factory_ash_.set_disconnect_handler(std::move(cleanup_callback));
}

DeviceFactoryAdapterLacros::~DeviceFactoryAdapterLacros() = default;

void DeviceFactoryAdapterLacros::GetDeviceInfos(
    GetDeviceInfosCallback callback) {
  DCHECK(device_factory_ash_.is_bound());
  device_factory_ash_->GetDeviceInfos(std::move(callback));
}

void DeviceFactoryAdapterLacros::CreateDevice(const std::string& device_id,
                                              CreateDeviceCallback callback) {
  DCHECK(device_factory_ash_.is_bound());
  mojo::PendingRemote<crosapi::mojom::VideoCaptureDevice> proxy_remote;
  auto proxy_receiver = proxy_remote.InitWithNewPipeAndPassReceiver();
  auto wrapped_callback =
      base::BindOnce(&DeviceFactoryAdapterLacros::WrapNewDeviceInProxy,
                     weak_factory_.GetWeakPtr(), std::move(callback), device_id,
                     std::move(proxy_remote));

  device_factory_ash_->CreateDevice(device_id, std::move(proxy_receiver),
                                    std::move(wrapped_callback));
}

void DeviceFactoryAdapterLacros::WrapNewDeviceInProxy(
    CreateDeviceCallback callback,
    const std::string& device_id,
    mojo::PendingRemote<crosapi::mojom::VideoCaptureDevice> proxy_remote,
    crosapi::mojom::DeviceAccessResultCode code) {
  media::VideoCaptureError video_capture_result_code;
  switch (code) {
    case crosapi::mojom::DeviceAccessResultCode::NOT_INITIALIZED:
      video_capture_result_code = media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice;
      break;
    case crosapi::mojom::DeviceAccessResultCode::SUCCESS:
      video_capture_result_code = media::VideoCaptureError::kNone;
      break;
    case crosapi::mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND:
      video_capture_result_code = media::VideoCaptureError::
          kServiceDeviceLauncherServiceRespondedWithDeviceNotFound;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unexpected device access result code";
  }

  if (video_capture_result_code != media::VideoCaptureError::kNone) {
    DeviceInfo info{nullptr, video_capture_result_code};
    std::move(callback).Run(std::move(info));
    return;
  }
  // Since |device_proxy| is owned by this instance and the cleanup callback
  // is only called within the lifetime of |device_proxy|, it should be safe
  // to use base::Unretained(this) here.
  auto device_proxy = std::make_unique<DeviceProxyLacros>(
      std::nullopt, std::move(proxy_remote),
      base::BindOnce(
          &DeviceFactoryAdapterLacros::OnClientConnectionErrorOrClose,
          base::Unretained(this), device_id));
  DeviceInfo info{device_proxy.get(), media::VideoCaptureError::kNone};
  devices_.emplace(device_id, std::move(device_proxy));
  std::move(callback).Run(std::move(info));
}

void DeviceFactoryAdapterLacros::StopDevice(const std::string device_id) {
  OnClientConnectionErrorOrClose(device_id);
}

void DeviceFactoryAdapterLacros::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<mojom::Producer> producer,
    mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  NOTREACHED_IN_MIGRATION();
}

void DeviceFactoryAdapterLacros::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  NOTREACHED_IN_MIGRATION();
}

void DeviceFactoryAdapterLacros::AddGpuMemoryBufferVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::GpuMemoryBufferVirtualDevice>
        virtual_device_receiver) {
  NOTREACHED_IN_MIGRATION();
}

void DeviceFactoryAdapterLacros::RegisterVirtualDevicesChangedObserver(
    mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
    bool raise_event_if_virtual_devices_already_present) {
  NOTREACHED_IN_MIGRATION();
}

void DeviceFactoryAdapterLacros::OnClientConnectionErrorOrClose(
    std::string device_id) {
  devices_.erase(device_id);
}

}  // namespace video_capture
