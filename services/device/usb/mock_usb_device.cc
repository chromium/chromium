// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mock_usb_device.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"

namespace device {

MockUsbDevice::MockUsbDevice(uint16_t vendor_id, uint16_t product_id)
    : MockUsbDevice(vendor_id, product_id, "", "", "") {}

MockUsbDevice::MockUsbDevice(uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& manufacturer_string,
                             const std::string& product_string,
                             const std::string& serial_number)
    : UsbDevice(0x0200,  // usb_version
                0xff,    // device_class
                0xff,    // device_subclass
                0xff,    // device_protocol
                vendor_id,
                product_id,
                0x0100,  // device_version
                base::UTF8ToUTF16(manufacturer_string),
                base::UTF8ToUTF16(product_string),
                base::UTF8ToUTF16(serial_number),
                /*bus_number=*/0,
                /*port_number=*/0) {}

MockUsbDevice::~MockUsbDevice() = default;

void MockUsbDevice::AddMockConfig(mojom::UsbConfigurationInfoPtr config) {
  device_info_->configurations.push_back(std::move(config));
}

void MockUsbDevice::ActiveConfigurationChanged(int configuration_value) {
  UsbDevice::ActiveConfigurationChanged(configuration_value);
}

void MockUsbDevice::NotifyDeviceRemoved() {
  UsbDevice::NotifyDeviceRemoved();
}

}  // namespace device
