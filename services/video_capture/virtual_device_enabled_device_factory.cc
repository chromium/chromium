// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/virtual_device_enabled_device_factory.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device_factory.h"
#include "services/video_capture/shared_memory_virtual_device_mojo_adapter.h"
#include "services/video_capture/texture_virtual_device_mojo_adapter.h"

namespace video_capture {

class VirtualDeviceEnabledDeviceFactory::VirtualDeviceEntry {
 public:
  VirtualDeviceEntry(
      const media::VideoCaptureDeviceInfo& device_info,
      std::unique_ptr<SharedMemoryVirtualDeviceMojoAdapter> device,
      std::unique_ptr<mojo::Receiver<mojom::SharedMemoryVirtualDevice>>
          producer_receiver)
      : device_info_(device_info),
        device_type_(DeviceType::kSharedMemory),
        shared_memory_device_(std::move(device)),
        shared_memory_producer_receiver_(std::move(producer_receiver)) {}

  VirtualDeviceEntry(
      const media::VideoCaptureDeviceInfo& device_info,
      std::unique_ptr<TextureVirtualDeviceMojoAdapter> device,
      std::unique_ptr<mojo::Receiver<mojom::TextureVirtualDevice>>
          producer_receiver)
      : device_info_(device_info),
        device_type_(DeviceType::kTexture),
        texture_device_(std::move(device)),
        texture_producer_receiver_(std::move(producer_receiver)) {}

  VirtualDeviceEntry(VirtualDeviceEntry&& other) = default;
  VirtualDeviceEntry& operator=(VirtualDeviceEntry&& other) = default;

  bool HasConsumerBinding() { return consumer_receiver_ != nullptr; }

  void BindConsumerReceiver(
      mojo::PendingReceiver<mojom::Device> device_receiver,
      base::OnceClosure connection_error_handler) {
    switch (device_type_) {
      case DeviceType::kSharedMemory:
        consumer_receiver_ = std::make_unique<mojo::Receiver<mojom::Device>>(
            shared_memory_device_.get(), std::move(device_receiver));
        break;
      case DeviceType::kTexture:
        consumer_receiver_ = std::make_unique<mojo::Receiver<mojom::Device>>(
            texture_device_.get(), std::move(device_receiver));
        break;
    }
    consumer_receiver_->set_disconnect_handler(
        std::move(connection_error_handler));
  }

  void ResetConsumerReceiver() { consumer_receiver_.reset(); }

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
  std::unique_ptr<mojo::Receiver<mojom::SharedMemoryVirtualDevice>>
      shared_memory_producer_receiver_;

  // Only valid for |device_type_ == kTexture|
  std::unique_ptr<TextureVirtualDeviceMojoAdapter> texture_device_;
  std::unique_ptr<mojo::Receiver<mojom::TextureVirtualDevice>>
      texture_producer_receiver_;

  std::unique_ptr<mojo::Receiver<mojom::Device>> consumer_receiver_;
};

VirtualDeviceEnabledDeviceFactory::VirtualDeviceEnabledDeviceFactory(
    std::unique_ptr<DeviceFactory> device_factory)
    : device_factory_(std::move(device_factory)) {}

VirtualDeviceEnabledDeviceFactory::~VirtualDeviceEnabledDeviceFactory() =
    default;

void VirtualDeviceEnabledDeviceFactory::GetDeviceInfos(
    GetDeviceInfosCallback callback) {
  device_factory_->GetDeviceInfos(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::OnGetDeviceInfos,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void VirtualDeviceEnabledDeviceFactory::CreateDevice(
    const std::string& device_id,
    mojo::PendingReceiver<mojom::Device> device_receiver,
    CreateDeviceCallback callback) {
  auto virtual_device_iter = virtual_devices_by_id_.find(device_id);
  if (virtual_device_iter != virtual_devices_by_id_.end()) {
    VirtualDeviceEntry& device_entry = virtual_device_iter->second;
    if (device_entry.HasConsumerBinding()) {
      // The requested virtual device is already used by another client.
      // Revoke the access for the current client, then bind to the new
      // receiver.
      device_entry.ResetConsumerReceiver();
      device_entry.StopDevice();
    }
    device_entry.BindConsumerReceiver(
        std::move(device_receiver),
        base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                           OnVirtualDeviceConsumerConnectionErrorOrClose,
                       base::Unretained(this), device_id));
    std::move(callback).Run(mojom::DeviceAccessResultCode::SUCCESS);
    return;
  }

  device_factory_->CreateDevice(device_id, std::move(device_receiver),
                                std::move(callback));
}

void VirtualDeviceEnabledDeviceFactory::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<mojom::Producer> producer_pending_remote,
    bool send_buffer_handles_to_producer_as_raw_file_descriptors,
    mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  auto device_id = device_info.descriptor.device_id;
  auto virtual_device_iter = virtual_devices_by_id_.find(device_id);
  if (virtual_device_iter != virtual_devices_by_id_.end()) {
    // Revoke the access for the current producer and consumer by
    // removing it from the list.
    virtual_devices_by_id_.erase(virtual_device_iter);
  }

  mojo::Remote<mojom::Producer> producer(std::move(producer_pending_remote));
  producer.set_disconnect_handler(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         OnVirtualDeviceProducerConnectionErrorOrClose,
                     base::Unretained(this), device_id));
  auto device = std::make_unique<SharedMemoryVirtualDeviceMojoAdapter>(
      std::move(producer),
      send_buffer_handles_to_producer_as_raw_file_descriptors);
  auto producer_receiver =
      std::make_unique<mojo::Receiver<mojom::SharedMemoryVirtualDevice>>(
          device.get(), std::move(virtual_device_receiver));
  producer_receiver->set_disconnect_handler(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         OnVirtualDeviceProducerConnectionErrorOrClose,
                     base::Unretained(this), device_id));
  VirtualDeviceEntry device_entry(device_info, std::move(device),
                                  std::move(producer_receiver));
  virtual_devices_by_id_.insert(
      std::make_pair(device_id, std::move(device_entry)));
  EmitDevicesChangedEvent();
}

void VirtualDeviceEnabledDeviceFactory::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  auto device_id = device_info.descriptor.device_id;
  auto virtual_device_iter = virtual_devices_by_id_.find(device_id);
  if (virtual_device_iter != virtual_devices_by_id_.end()) {
    // Revoke the access for the current producer and consumer by
    // removing it from the list.
    virtual_devices_by_id_.erase(virtual_device_iter);
  }

  auto device = std::make_unique<TextureVirtualDeviceMojoAdapter>();
  auto producer_receiver =
      std::make_unique<mojo::Receiver<mojom::TextureVirtualDevice>>(
          device.get(), std::move(virtual_device_receiver));
  producer_receiver->set_disconnect_handler(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         OnVirtualDeviceProducerConnectionErrorOrClose,
                     base::Unretained(this), device_id));
  VirtualDeviceEntry device_entry(device_info, std::move(device),
                                  std::move(producer_receiver));
  virtual_devices_by_id_.insert(
      std::make_pair(device_id, std::move(device_entry)));
  EmitDevicesChangedEvent();
}

void VirtualDeviceEnabledDeviceFactory::RegisterVirtualDevicesChangedObserver(
    mojo::PendingRemote<mojom::DevicesChangedObserver> observer_pending_remote,
    bool raise_event_if_virtual_devices_already_present) {
  mojo::Remote<mojom::DevicesChangedObserver> observer(
      std::move(observer_pending_remote));
  observer.set_disconnect_handler(base::BindOnce(
      &VirtualDeviceEnabledDeviceFactory::OnDevicesChangedObserverDisconnected,
      weak_factory_.GetWeakPtr(), observer.get()));
  if (!virtual_devices_by_id_.empty() &&
      raise_event_if_virtual_devices_already_present) {
    observer->OnDevicesChanged();
  }
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
    mojom::DevicesChangedObserver* observer) {
  for (auto iter = devices_changed_observers_.begin();
       iter != devices_changed_observers_.end(); ++iter) {
    if (iter->get() == observer) {
      devices_changed_observers_.erase(iter);
      break;
    }
  }
}

}  // namespace video_capture
