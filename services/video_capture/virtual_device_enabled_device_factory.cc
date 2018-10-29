// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/virtual_device_enabled_device_factory.h"

#include "base/logging.h"
#include "media/capture/video/video_capture_device_info.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/shared_memory_virtual_device_mojo_adapter.h"
#include "services/video_capture/texture_virtual_device_mojo_adapter.h"

namespace video_capture {

class VirtualDeviceEnabledDeviceFactory::VirtualDeviceEntry {
 public:
  VirtualDeviceEntry(
      const media::VideoCaptureDeviceInfo& device_info,
      std::unique_ptr<SharedMemoryVirtualDeviceMojoAdapter> device,
      std::unique_ptr<mojo::Binding<mojom::SharedMemoryVirtualDevice>>
          producer_binding)
      : device_info_(device_info),
        device_type_(DeviceType::kSharedMemory),
        shared_memory_device_(std::move(device)),
        shared_memory_producer_binding_(std::move(producer_binding)) {}

  VirtualDeviceEntry(const media::VideoCaptureDeviceInfo& device_info,
                     std::unique_ptr<TextureVirtualDeviceMojoAdapter> device,
                     std::unique_ptr<mojo::Binding<mojom::TextureVirtualDevice>>
                         producer_binding)
      : device_info_(device_info),
        device_type_(DeviceType::kTexture),
        texture_device_(std::move(device)),
        texture_producer_binding_(std::move(producer_binding)) {}

  VirtualDeviceEntry(VirtualDeviceEntry&& other) = default;
  VirtualDeviceEntry& operator=(VirtualDeviceEntry&& other) = default;

  bool HasConsumerBinding() { return consumer_binding_ != nullptr; }

  void EstablishConsumerBinding(mojom::DeviceRequest device_request,
                                base::OnceClosure connection_error_handler) {
    switch (device_type_) {
      case DeviceType::kSharedMemory:
        consumer_binding_ = std::make_unique<mojo::Binding<mojom::Device>>(
            shared_memory_device_.get(), std::move(device_request));
        break;
      case DeviceType::kTexture:
        consumer_binding_ = std::make_unique<mojo::Binding<mojom::Device>>(
            texture_device_.get(), std::move(device_request));
        break;
    }
    consumer_binding_->set_connection_error_handler(
        std::move(connection_error_handler));
  }

  void ResetConsumerBinding() { consumer_binding_.reset(); }

  void StopDevice() {
    if (shared_memory_device_)
      shared_memory_device_->Stop();
    else
      texture_device_->Stop();
  }

  media::VideoCaptureDeviceInfo device_info() const { return device_info_; }

 private:
  enum class DeviceType { kSharedMemory, kTexture };

  media::VideoCaptureDeviceInfo device_info_;
  DeviceType device_type_;

  // Only valid for |device_type_ == kSharedMemory|
  std::unique_ptr<SharedMemoryVirtualDeviceMojoAdapter> shared_memory_device_;
  std::unique_ptr<mojo::Binding<mojom::SharedMemoryVirtualDevice>>
      shared_memory_producer_binding_;

  // Only valid for |device_type_ == kTexture|
  std::unique_ptr<TextureVirtualDeviceMojoAdapter> texture_device_;
  std::unique_ptr<mojo::Binding<mojom::TextureVirtualDevice>>
      texture_producer_binding_;

  std::unique_ptr<mojo::Binding<mojom::Device>> consumer_binding_;
};

VirtualDeviceEnabledDeviceFactory::VirtualDeviceEnabledDeviceFactory(
    std::unique_ptr<DeviceFactoryMediaToMojoAdapter> device_factory)
    : device_factory_(std::move(device_factory)), weak_factory_(this) {}

VirtualDeviceEnabledDeviceFactory::~VirtualDeviceEnabledDeviceFactory() =
    default;

void VirtualDeviceEnabledDeviceFactory::SetServiceRef(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  if (service_ref)
    device_factory_->SetServiceRef(service_ref->Clone());
  else
    device_factory_->SetServiceRef(nullptr);
  service_ref_ = std::move(service_ref);
}

void VirtualDeviceEnabledDeviceFactory::GetDeviceInfos(
    GetDeviceInfosCallback callback) {
  device_factory_->GetDeviceInfos(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::OnGetDeviceInfos,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void VirtualDeviceEnabledDeviceFactory::CreateDevice(
    const std::string& device_id,
    mojom::DeviceRequest device_request,
    CreateDeviceCallback callback) {
  auto virtual_device_iter = virtual_devices_by_id_.find(device_id);
  if (virtual_device_iter != virtual_devices_by_id_.end()) {
    VirtualDeviceEntry& device_entry = virtual_device_iter->second;
    if (device_entry.HasConsumerBinding()) {
      // The requested virtual device is already used by another client.
      // Revoke the access for the current client, then bind to the new request.
      device_entry.ResetConsumerBinding();
      device_entry.StopDevice();
    }
    device_entry.EstablishConsumerBinding(
        std::move(device_request),
        base::Bind(&VirtualDeviceEnabledDeviceFactory::
                       OnVirtualDeviceConsumerConnectionErrorOrClose,
                   base::Unretained(this), device_id));
    std::move(callback).Run(mojom::DeviceAccessResultCode::SUCCESS);
    return;
  }

  device_factory_->CreateDevice(device_id, std::move(device_request),
                                std::move(callback));
}

void VirtualDeviceEnabledDeviceFactory::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojom::ProducerPtr producer,
    bool send_buffer_handles_to_producer_as_raw_file_descriptors,
    mojom::SharedMemoryVirtualDeviceRequest virtual_device_request) {
  auto device_id = device_info.descriptor.device_id;
  auto virtual_device_iter = virtual_devices_by_id_.find(device_id);
  if (virtual_device_iter != virtual_devices_by_id_.end()) {
    // Revoke the access for the current producer and consumer by
    // removing it from the list.
    virtual_devices_by_id_.erase(virtual_device_iter);
  }

  producer.set_connection_error_handler(
      base::Bind(&VirtualDeviceEnabledDeviceFactory::
                     OnVirtualDeviceProducerConnectionErrorOrClose,
                 base::Unretained(this), device_id));
  auto device = std::make_unique<SharedMemoryVirtualDeviceMojoAdapter>(
      service_ref_->Clone(), std::move(producer),
      send_buffer_handles_to_producer_as_raw_file_descriptors);
  auto producer_binding =
      std::make_unique<mojo::Binding<mojom::SharedMemoryVirtualDevice>>(
          device.get(), std::move(virtual_device_request));
  producer_binding->set_connection_error_handler(
      base::Bind(&VirtualDeviceEnabledDeviceFactory::
                     OnVirtualDeviceProducerConnectionErrorOrClose,
                 base::Unretained(this), device_id));
  VirtualDeviceEntry device_entry(device_info, std::move(device),
                                  std::move(producer_binding));
  virtual_devices_by_id_.insert(
      std::make_pair(device_id, std::move(device_entry)));
  EmitDevicesChangedEvent();
}

void VirtualDeviceEnabledDeviceFactory::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojom::TextureVirtualDeviceRequest virtual_device_request) {
  auto device_id = device_info.descriptor.device_id;
  auto virtual_device_iter = virtual_devices_by_id_.find(device_id);
  if (virtual_device_iter != virtual_devices_by_id_.end()) {
    // Revoke the access for the current producer and consumer by
    // removing it from the list.
    virtual_devices_by_id_.erase(virtual_device_iter);
  }

  auto device =
      std::make_unique<TextureVirtualDeviceMojoAdapter>(service_ref_->Clone());
  auto producer_binding =
      std::make_unique<mojo::Binding<mojom::TextureVirtualDevice>>(
          device.get(), std::move(virtual_device_request));
  producer_binding->set_connection_error_handler(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         OnVirtualDeviceProducerConnectionErrorOrClose,
                     base::Unretained(this), device_id));
  VirtualDeviceEntry device_entry(device_info, std::move(device),
                                  std::move(producer_binding));
  virtual_devices_by_id_.insert(
      std::make_pair(device_id, std::move(device_entry)));
  EmitDevicesChangedEvent();
}

void VirtualDeviceEnabledDeviceFactory::RegisterVirtualDevicesChangedObserver(
    mojom::DevicesChangedObserverPtr observer) {
  observer.set_connection_error_handler(base::BindOnce(
      &VirtualDeviceEnabledDeviceFactory::OnDevicesChangedObserverDisconnected,
      weak_factory_.GetWeakPtr(), &observer));
  devices_changed_observers_.push_back(std::move(observer));
}

void VirtualDeviceEnabledDeviceFactory::OnGetDeviceInfos(
    GetDeviceInfosCallback callback,
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  std::vector<media::VideoCaptureDeviceInfo> all_device_infos;
  for (const auto& device_entry : virtual_devices_by_id_) {
    all_device_infos.push_back(device_entry.second.device_info());
  }
  all_device_infos.insert(std::end(all_device_infos), std::begin(device_infos),
                          std::end(device_infos));
  std::move(callback).Run(all_device_infos);
}

void VirtualDeviceEnabledDeviceFactory::
    OnVirtualDeviceProducerConnectionErrorOrClose(
        const std::string& device_id) {
  virtual_devices_by_id_.at(device_id).StopDevice();
  virtual_devices_by_id_.erase(device_id);
  EmitDevicesChangedEvent();
}

void VirtualDeviceEnabledDeviceFactory::
    OnVirtualDeviceConsumerConnectionErrorOrClose(
        const std::string& device_id) {
  virtual_devices_by_id_.at(device_id).StopDevice();
}

void VirtualDeviceEnabledDeviceFactory::EmitDevicesChangedEvent() {
  for (auto& observer : devices_changed_observers_)
    observer->OnDevicesChanged();
}

void VirtualDeviceEnabledDeviceFactory::OnDevicesChangedObserverDisconnected(
    mojom::DevicesChangedObserverPtr* observer) {
  auto iter = std::find_if(
      devices_changed_observers_.begin(), devices_changed_observers_.end(),
      [observer](const mojom::DevicesChangedObserverPtr& entry) {
        return &entry == observer;
      });
  if (iter == devices_changed_observers_.end()) {
    DCHECK(false);
    return;
  }
  devices_changed_observers_.erase(iter);
}

}  // namespace video_capture
