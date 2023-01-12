// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_

#include <map>
#include <utility>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device_factory.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"

namespace video_capture {

// Decorator that adds support for virtual devices to a given DeviceFactory.
class VirtualDeviceEnabledDeviceFactory : public DeviceFactory {
 public:
  explicit VirtualDeviceEnabledDeviceFactory(
      std::unique_ptr<DeviceFactory> factory);

  VirtualDeviceEnabledDeviceFactory(const VirtualDeviceEnabledDeviceFactory&) =
      delete;
  VirtualDeviceEnabledDeviceFactory& operator=(
      const VirtualDeviceEnabledDeviceFactory&) = delete;

  ~VirtualDeviceEnabledDeviceFactory() override;

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
#if BUILDFLAG(IS_WIN)
  void OnGpuInfoUpdate(const CHROME_LUID& luid) override;
#endif

 private:
  class VirtualDeviceEntry;

  void OnGetDeviceInfos(
      GetDeviceInfosCallback callback,
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos);

  void OnVirtualDeviceProducerConnectionErrorOrClose(
      const std::string& device_id);
  void OnVirtualDeviceConsumerConnectionErrorOrClose(
      const std::string& device_id);
  void EmitDevicesChangedEvent();
  void OnDevicesChangedObserverDisconnected(
      mojom::DevicesChangedObserver* observer);

  std::map<std::string, VirtualDeviceEntry> virtual_devices_by_id_;
  const std::unique_ptr<DeviceFactory> device_factory_;
  std::vector<mojo::Remote<mojom::DevicesChangedObserver>>
      devices_changed_observers_;

  base::WeakPtrFactory<VirtualDeviceEnabledDeviceFactory> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_
