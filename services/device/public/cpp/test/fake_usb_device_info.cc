// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_usb_device_info.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "services/device/public/cpp/usb/usb_utils.h"

namespace device {

void FakeUsbDeviceInfo::Observer::OnDeviceRemoved(
    scoped_refptr<FakeUsbDeviceInfo> device) {}

FakeUsbDeviceInfo::FakeUsbDeviceInfo(uint16_t usb_version,
                                     uint8_t device_class,
                                     uint8_t device_subclass,
                                     uint8_t device_protocol,
                                     uint16_t device_version,
                                     uint16_t vendor_id,
                                     uint16_t product_id,
                                     uint32_t bus_number,
                                     uint32_t port_number,
                                     const std::string& manufacturer_string,
                                     const std::string& product_string,
                                     const std::string& serial_number) {
  device_info_.guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  device_info_.usb_version_major = usb_version >> 8;
  device_info_.usb_version_minor = usb_version >> 4 & 0xf;
  device_info_.usb_version_subminor = usb_version & 0xf;
  device_info_.class_code = device_class;
  device_info_.subclass_code = device_subclass;
  device_info_.protocol_code = device_protocol;
  device_info_.device_version_major = device_version >> 8;
  device_info_.device_version_minor = device_version >> 4 & 0xf;
  device_info_.device_version_subminor = device_version & 0xf;
  device_info_.bus_number = bus_number;
  device_info_.port_number = port_number;
  device_info_.vendor_id = vendor_id;
  device_info_.product_id = product_id;
  device_info_.manufacturer_name = base::UTF8ToUTF16(manufacturer_string);
  device_info_.product_name = base::UTF8ToUTF16(product_string);
  device_info_.serial_number = base::UTF8ToUTF16(serial_number);
  AddConfig(
      CreateConfiguration(device_class, device_subclass, device_protocol));
}

FakeUsbDeviceInfo::FakeUsbDeviceInfo(uint16_t vendor_id,
                                     uint16_t product_id,
                                     const std::string& manufacturer_string,
                                     const std::string& product_string,
                                     const std::string& serial_number)
    : FakeUsbDeviceInfo(/*usb_version=*/0x0200,
                        /*device_class=*/0xff,
                        /*device_subclass=*/0xff,
                        /*device_protocol=*/0xff,
                        /*device_version=*/0x0100,
                        vendor_id,
                        product_id,
                        /*bus_number=*/0,
                        /*port_number=*/0,
                        manufacturer_string,
                        product_string,
                        serial_number) {}

FakeUsbDeviceInfo::FakeUsbDeviceInfo(
    uint16_t vendor_id,
    uint16_t product_id,
    const std::string& manufacturer_string,
    const std::string& product_string,
    const std::string& serial_number,
    std::vector<mojom::UsbConfigurationInfoPtr> configurations)
    : FakeUsbDeviceInfo(vendor_id,
                        product_id,
                        manufacturer_string,
                        product_string,
                        serial_number) {
  device_info_.configurations = std::move(configurations);
}

FakeUsbDeviceInfo::FakeUsbDeviceInfo(uint16_t vendor_id,
                                     uint16_t product_id,
                                     const std::string& manufacturer_string,
                                     const std::string& product_string,
                                     const std::string& serial_number,
                                     const GURL& webusb_landing_page)
    : FakeUsbDeviceInfo(vendor_id,
                        product_id,
                        manufacturer_string,
                        product_string,
                        serial_number) {
  device_info_.webusb_landing_page = webusb_landing_page;
}

FakeUsbDeviceInfo::FakeUsbDeviceInfo(uint16_t vendor_id, uint16_t product_id)
    : FakeUsbDeviceInfo(vendor_id, product_id, "", "", "") {}

FakeUsbDeviceInfo::FakeUsbDeviceInfo(
    uint16_t vendor_id,
    uint16_t product_id,
    uint8_t device_class,
    std::vector<mojom::UsbConfigurationInfoPtr> configurations)
    : FakeUsbDeviceInfo(vendor_id, product_id) {
  device_info_.class_code = device_class;
  device_info_.configurations = std::move(configurations);
}

FakeUsbDeviceInfo::~FakeUsbDeviceInfo() = default;

bool FakeUsbDeviceInfo::SetActiveConfig(uint8_t value) {
  for (auto& config : device_info_.configurations) {
    if (config->configuration_value == value) {
      device_info_.active_configuration = value;
      return true;
    }
  }
  return false;
}

void FakeUsbDeviceInfo::AddConfig(mojom::UsbConfigurationInfoPtr config) {
  device_info_.configurations.push_back(std::move(config));
}

void FakeUsbDeviceInfo::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeUsbDeviceInfo::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeUsbDeviceInfo::NotifyDeviceRemoved() {
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(this);
}

mojom::UsbConfigurationInfoPtr FakeUsbDeviceInfo::CreateConfiguration(
    uint8_t device_class,
    uint8_t subclass_code,
    uint8_t protocol_code,
    uint8_t configuration_value) {
  auto alternate = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate->alternate_setting = 0;
  alternate->class_code = device_class;
  alternate->subclass_code = subclass_code;
  alternate->protocol_code = protocol_code;

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = configuration_value;
  config->interfaces.push_back(std::move(interface));
  return config;
}

}  // namespace device
