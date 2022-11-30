// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_MOCK_USB_DEVICE_H_
#define SERVICES_DEVICE_USB_MOCK_USB_DEVICE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_device_handle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice(uint16_t vendor_id, uint16_t product_id);
  MockUsbDevice(uint16_t vendor_id,
                uint16_t product_id,
                const std::string& manufacturer_string,
                const std::string& product_string,
                const std::string& serial_number);

  void Open(OpenCallback callback) override { OpenInternal(callback); }
  MOCK_METHOD1(OpenInternal, void(OpenCallback&));

  void AddMockConfig(mojom::UsbConfigurationInfoPtr config);

  // Public wrappers around protected functions.
  void ActiveConfigurationChanged(int configuration_value);
  void NotifyDeviceRemoved();

 private:
  ~MockUsbDevice() override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_MOCK_USB_DEVICE_H_
