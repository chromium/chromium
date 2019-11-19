// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device.h"

#include <utility>

#include "base/guid.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/webusb_descriptors.h"

namespace device {

UsbDevice::Observer::~Observer() = default;

void UsbDevice::Observer::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {}

UsbDevice::UsbDevice(uint32_t bus_number, uint32_t port_number) {
  device_info_ = mojom::UsbDeviceInfo::New();
  device_info_->guid = base::GenerateGUID();
  device_info_->bus_number = bus_number;
  device_info_->port_number = port_number;
}

UsbDevice::UsbDevice(mojom::UsbDeviceInfoPtr device_info)
    : device_info_(std::move(device_info)) {
  DCHECK(device_info_);
  device_info_->guid = base::GenerateGUID();
  ActiveConfigurationChanged(device_info_->active_configuration);
}

UsbDevice::UsbDevice(uint16_t usb_version,
                     uint8_t device_class,
                     uint8_t device_subclass,
                     uint8_t device_protocol,
                     uint16_t vendor_id,
                     uint16_t product_id,
                     uint16_t device_version,
                     const base::string16& manufacturer_string,
                     const base::string16& product_string,
                     const base::string16& serial_number,
                     uint32_t bus_number,
                     uint32_t port_number) {
  device_info_ = mojom::UsbDeviceInfo::New();
  device_info_->guid = base::GenerateGUID();
  device_info_->usb_version_major = usb_version >> 8;
  device_info_->usb_version_minor = usb_version >> 4 & 0xf;
  device_info_->usb_version_subminor = usb_version & 0xf;
  device_info_->class_code = device_class;
  device_info_->subclass_code = device_subclass;
  device_info_->protocol_code = device_protocol;
  device_info_->vendor_id = vendor_id;
  device_info_->product_id = product_id;
  device_info_->device_version_major = device_version >> 8;
  device_info_->device_version_minor = device_version >> 4 & 0xf;
  device_info_->device_version_subminor = device_version & 0xf;
  device_info_->manufacturer_name = manufacturer_string;
  device_info_->product_name = product_string;
  device_info_->serial_number = serial_number;
  device_info_->bus_number = bus_number;
  device_info_->port_number = port_number;
}

UsbDevice::~UsbDevice() = default;

uint16_t UsbDevice::usb_version() const {
  return GetUsbVersion(*device_info_);
}

uint16_t UsbDevice::device_version() const {
  return GetDeviceVersion(*device_info_);
}

const mojom::UsbConfigurationInfo* UsbDevice::GetActiveConfiguration() const {
  for (const auto& config : configurations()) {
    if (config->configuration_value == device_info_->active_configuration)
      return config.get();
  }
  return nullptr;
}

void UsbDevice::CheckUsbAccess(ResultCallback callback) {
  // By default assume that access to the device is allowed. This is implemented
  // on Chrome OS by checking with permission_broker.
  std::move(callback).Run(true);
}

void UsbDevice::RequestPermission(ResultCallback callback) {
  // By default assume that access to the device is allowed. This is implemented
  // on Android by calling android.hardware.usb.UsbManger.requestPermission.
  std::move(callback).Run(true);
}

bool UsbDevice::permission_granted() const {
  return true;
}

void UsbDevice::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void UsbDevice::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void UsbDevice::ActiveConfigurationChanged(int configuration_value) {
  device_info_->active_configuration = configuration_value;
}

void UsbDevice::NotifyDeviceRemoved() {
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(this);
}

void UsbDevice::OnDisconnect() {
  // Swap out the handle list as HandleClosed() will try to modify it.
  std::list<UsbDeviceHandle*> handles;
  handles.swap(handles_);
  for (auto* handle : handles_)
    handle->Close();
}

void UsbDevice::HandleClosed(UsbDeviceHandle* handle) {
  handles_.remove(handle);
}

}  // namespace device
