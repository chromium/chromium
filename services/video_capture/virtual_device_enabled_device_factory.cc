// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/virtual_device_enabled_device_factory.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device_factory.h"
#include "services/video_capture/gpu_memory_buffer_virtual_device_mojo_adapter.h"
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

  VirtualDeviceEntry(
      const media::VideoCaptureDeviceInfo& device_info,
      std::unique_ptr<GpuMemoryBufferVirtualDeviceMojoAdapter> device,
      std::unique_ptr<mojo::Receiver<mojom::GpuMemoryBufferVirtualDevice>>
          producer_receiver)
      : device_info_(device_info),
        device_type_(DeviceType::kGpuMemoryBuffer),
        gmb_device_(std::move(device)),
        gmb_producer_receiver_(std::move(producer_receiver)) {}

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
      case DeviceType::kGpuMemoryBuffer:
        consumer_receiver_ = std::make_unique<mojo::Receiver<mojom::Device>>(
            gmb_device_.get(), std::move(device_receiver));
        break;
    }
    consumer_receiver_->set_disconnect_handler(
        std::move(connection_error_handler));
  }

  void ResetConsumerReceiver() { consumer_receiver_.reset(); }

  Device* GetDevice() {
    if (shared_memory_device_)
      return shared_memory_device_.get();
    else if (texture_device_)
      return texture_device_.get();
    else
      return gmb_device_.get();
  }

  void StopDevice() {
    if (shared_memory_device_)
      shared_memory_device_->Stop();
    else if (texture_device_)
      texture_device_->Stop();
    else
      gmb_device_->Stop();
  }

  media::VideoCaptureDeviceInfo device_info() const { return device_info_; }

 private:
  enum class DeviceType { kSharedMemory, kTexture, kGpuMemoryBuffer };

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

  // Only valid for |device_type_ == kGpuMemoryBuffer|
  std::unique_ptr<GpuMemoryBufferVirtualDeviceMojoAdapter> gmb_device_;
  std::unique_ptr<mojo::Receiver<mojom::GpuMemoryBufferVirtualDevice>>
      gmb_producer_receiver_;

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
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void VirtualDeviceEnabledDeviceFactory::CreateDevice(
    const std::string& device_id,
    CreateDeviceCallback callback) {
  device_factory_->CreateDevice(
      device_id,
      base::BindOnce(
          &VirtualDeviceEnabledDeviceFactory::OnDeviceFactoryDeviceCreated,
          weak_factory_.GetWeakPtr(), device_id, std::move(callback)));
}

void VirtualDeviceEnabledDeviceFactory::OnDeviceFactoryDeviceCreated(
    std::string device_id,
    CreateDeviceCallback outer_callback,
    DeviceInfo info) {
  if (info.result_code !=
      media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound) {
    // Sever any old virtual device connection since underlying device creation
    // was successful or failed with another error than not found.
    virtual_devices_by_id_.erase(device_id);
    std::move(outer_callback).Run(info);
    return;
  }

  // The underlying factory failed to create the device.
  // Fall back to a registered virtual device if one exists.
  auto virtual_device_iter = virtual_devices_by_id_.find(device_id);
  if (virtual_device_iter != virtual_devices_by_id_.end()) {
    VirtualDeviceEntry& device_entry = virtual_device_iter->second;
    DeviceInfo virtual_device_info{device_entry.GetDevice(),
                                   media::VideoCaptureError::kNone};
    std::move(outer_callback).Run(std::move(virtual_device_info));
    return;
  }

  // No virtual device to fall back to, return the original failure.
  std::move(outer_callback).Run(info);
}

void VirtualDeviceEnabledDeviceFactory::StopDevice(
    const std::string device_id) {
  auto virtual_device_iter = virtual_devices_by_id_.find(device_id);
  if (virtual_device_iter != virtual_devices_by_id_.end()) {
    virtual_device_iter->second.StopDevice();
    return;
  }
  device_factory_->StopDevice(device_id);
}

void VirtualDeviceEnabledDeviceFactory::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<mojom::Producer> producer_pending_remote,
    mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  device_factory_->GetDeviceInfos(base::BindOnce(
      &VirtualDeviceEnabledDeviceFactory::OnGetDeviceInfosForVirtualDevice,
      weak_factory_.GetWeakPtr(), device_info.descriptor.device_id,
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         CompleteAddSharedMemoryVirtualDevice,
                     weak_factory_.GetWeakPtr(), device_info,
                     std::move(producer_pending_remote),
                     std::move(virtual_device_receiver))));
}

void VirtualDeviceEnabledDeviceFactory::CompleteAddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<mojom::Producer> producer_pending_remote,
    mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  auto device_id = device_info.descriptor.device_id;
  if (!PrepareVirtualDeviceId(device_id)) {
    return;
  }

  mojo::Remote<mojom::Producer> producer(std::move(producer_pending_remote));
  producer.set_disconnect_handler(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         OnVirtualDeviceProducerConnectionErrorOrClose,
                     base::Unretained(this), device_id));
  auto device = std::make_unique<SharedMemoryVirtualDeviceMojoAdapter>(
      std::move(producer));
  auto producer_receiver =
      std::make_unique<mojo::Receiver<mojom::SharedMemoryVirtualDevice>>(
          device.get(), std::move(virtual_device_receiver));
  producer_receiver->set_disconnect_handler(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         OnVirtualDeviceProducerConnectionErrorOrClose,
                     base::Unretained(this), device_id));
  CompleteRegisteringVirtualDevice(
      device_id, VirtualDeviceEntry(device_info, std::move(device),
                                    std::move(producer_receiver)));
}

void VirtualDeviceEnabledDeviceFactory::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  device_factory_->GetDeviceInfos(base::BindOnce(
      &VirtualDeviceEnabledDeviceFactory::OnGetDeviceInfosForVirtualDevice,
      weak_factory_.GetWeakPtr(), device_info.descriptor.device_id,
      base::BindOnce(
          &VirtualDeviceEnabledDeviceFactory::CompleteAddTextureVirtualDevice,
          weak_factory_.GetWeakPtr(), device_info,
          std::move(virtual_device_receiver))));
}

void VirtualDeviceEnabledDeviceFactory::CompleteAddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  auto device_id = device_info.descriptor.device_id;
  if (!PrepareVirtualDeviceId(device_id)) {
    return;
  }

  auto device = std::make_unique<TextureVirtualDeviceMojoAdapter>();
  auto producer_receiver =
      std::make_unique<mojo::Receiver<mojom::TextureVirtualDevice>>(
          device.get(), std::move(virtual_device_receiver));
  producer_receiver->set_disconnect_handler(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         OnVirtualDeviceProducerConnectionErrorOrClose,
                     base::Unretained(this), device_id));
  CompleteRegisteringVirtualDevice(
      device_id, VirtualDeviceEntry(device_info, std::move(device),
                                    std::move(producer_receiver)));
}

void VirtualDeviceEnabledDeviceFactory::AddGpuMemoryBufferVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::GpuMemoryBufferVirtualDevice>
        virtual_device_receiver) {
  device_factory_->GetDeviceInfos(base::BindOnce(
      &VirtualDeviceEnabledDeviceFactory::OnGetDeviceInfosForVirtualDevice,
      weak_factory_.GetWeakPtr(), device_info.descriptor.device_id,
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         CompleteAddGpuMemoryBufferVirtualDevice,
                     weak_factory_.GetWeakPtr(), device_info,
                     std::move(virtual_device_receiver))));
}

void VirtualDeviceEnabledDeviceFactory::CompleteAddGpuMemoryBufferVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::GpuMemoryBufferVirtualDevice>
        virtual_device_receiver) {
  auto device_id = device_info.descriptor.device_id;
  if (!PrepareVirtualDeviceId(device_id)) {
    return;
  }

  auto device = std::make_unique<GpuMemoryBufferVirtualDeviceMojoAdapter>();
  auto producer_receiver =
      std::make_unique<mojo::Receiver<mojom::GpuMemoryBufferVirtualDevice>>(
          device.get(), std::move(virtual_device_receiver));
  producer_receiver->set_disconnect_handler(
      base::BindOnce(&VirtualDeviceEnabledDeviceFactory::
                         OnVirtualDeviceProducerConnectionErrorOrClose,
                     base::Unretained(this), device_id));
  CompleteRegisteringVirtualDevice(
      device_id, VirtualDeviceEntry(device_info, std::move(device),
                                    std::move(producer_receiver)));
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

void VirtualDeviceEnabledDeviceFactory::OnGetDeviceInfosForVirtualDevice(
    std::string device_id,
    base::OnceClosure registration_closure,
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  if (std::ranges::any_of(device_infos, [&device_id](const auto& info) {
        return info.descriptor.device_id == device_id;
      })) {
    // Matches a hardware camera! Reject by returning early and letting the
    // bound Mojo receiver/remote drop.
    return;
  }
  std::move(registration_closure).Run();
}

bool VirtualDeviceEnabledDeviceFactory::PrepareVirtualDeviceId(
    const std::string& device_id) {
  auto virtual_device_iter = virtual_devices_by_id_.find(device_id);
  if (virtual_device_iter != virtual_devices_by_id_.end()) {
    // Revoke the access for the current producer and consumer by
    // removing it from the list.
    virtual_devices_by_id_.erase(virtual_device_iter);
  }
  return true;
}

void VirtualDeviceEnabledDeviceFactory::CompleteRegisteringVirtualDevice(
    const std::string& device_id,
    VirtualDeviceEntry device_entry) {
  virtual_devices_by_id_.insert(
      std::make_pair(device_id, std::move(device_entry)));
  EmitDevicesChangedEvent();
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

#if BUILDFLAG(IS_WIN)
void VirtualDeviceEnabledDeviceFactory::OnGpuInfoUpdate(
    const CHROME_LUID& luid) {
  device_factory_->OnGpuInfoUpdate(luid);
}
#endif

}  // namespace video_capture
