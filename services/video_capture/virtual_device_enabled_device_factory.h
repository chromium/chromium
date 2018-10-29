// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_

#include <map>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"

namespace video_capture {

class DeviceFactoryMediaToMojoAdapter;

// Decorator that adds support for virtual devices to a given
// mojom::DeviceFactory.
class VirtualDeviceEnabledDeviceFactory : public mojom::DeviceFactory {
 public:
  explicit VirtualDeviceEnabledDeviceFactory(
      std::unique_ptr<DeviceFactoryMediaToMojoAdapter> factory);
  ~VirtualDeviceEnabledDeviceFactory() override;

  void SetServiceRef(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);

  // mojom::DeviceFactory implementation.
  void GetDeviceInfos(GetDeviceInfosCallback callback) override;
  void CreateDevice(const std::string& device_id,
                    mojom::DeviceRequest device_request,
                    CreateDeviceCallback callback) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojom::ProducerPtr producer,
      bool send_buffer_handles_to_producer_as_raw_file_descriptors,
      mojom::SharedMemoryVirtualDeviceRequest virtual_device) override;
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojom::TextureVirtualDeviceRequest virtual_device) override;
  void RegisterVirtualDevicesChangedObserver(
      mojom::DevicesChangedObserverPtr observer) override;

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
      mojom::DevicesChangedObserverPtr* observer);

  std::map<std::string, VirtualDeviceEntry> virtual_devices_by_id_;
  const std::unique_ptr<DeviceFactoryMediaToMojoAdapter> device_factory_;
  std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  std::vector<mojom::DevicesChangedObserverPtr> devices_changed_observers_;

  base::WeakPtrFactory<VirtualDeviceEnabledDeviceFactory> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(VirtualDeviceEnabledDeviceFactory);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_
