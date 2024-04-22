// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_LACROS_DEVICE_FACTORY_ADAPTER_LACROS_H_
#define SERVICES_VIDEO_CAPTURE_LACROS_DEVICE_FACTORY_ADAPTER_LACROS_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device_factory.h"

namespace video_capture {

class DeviceProxyLacros;

// A proxy which forwards the requests to the actual
// video_capture::DeviceFactory in Ash-Chrome.
class DeviceFactoryAdapterLacros : public DeviceFactory {
 public:
  DeviceFactoryAdapterLacros(
      mojo::PendingRemote<crosapi::mojom::VideoCaptureDeviceFactory>
          device_factory_ash,
      base::OnceClosure cleanup_callback);
  DeviceFactoryAdapterLacros(const DeviceFactoryAdapterLacros&) = delete;
  DeviceFactoryAdapterLacros& operator=(const DeviceFactoryAdapterLacros&) =
      delete;
  ~DeviceFactoryAdapterLacros() override;

 private:
  // DeviceFactory implementation.
  void GetDeviceInfos(GetDeviceInfosCallback callback) override;
  void CreateDevice(const std::string& device_id,
                    CreateDeviceCallback callback) override;
  void StopDevice(const std::string device_id) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingRemote<mojom::Producer> producer,
      mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
          virtual_device_receiver) override;
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<mojom::TextureVirtualDevice>
          virtual_device_receiver) override;
  void AddGpuMemoryBufferVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<mojom::GpuMemoryBufferVirtualDevice>
          virtual_device_receiver) override;
  void RegisterVirtualDevicesChangedObserver(
      mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
      bool raise_event_if_virtual_devices_already_present) override;

  void WrapNewDeviceInProxy(
      CreateDeviceCallback callback,
      const std::string& device_id,
      mojo::PendingRemote<crosapi::mojom::VideoCaptureDevice> proxy_remote,
      crosapi::mojom::DeviceAccessResultCode code);

  void OnClientConnectionErrorOrClose(std::string device_id);

  mojo::Remote<crosapi::mojom::VideoCaptureDeviceFactory> device_factory_ash_;

  // The key is the device id used in blink::MediaStreamDevice.
  base::flat_map<std::string, std::unique_ptr<DeviceProxyLacros>> devices_;

  base::WeakPtrFactory<DeviceFactoryAdapterLacros> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_LACROS_DEVICE_FACTORY_ADAPTER_LACROS_H_
