// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_MAC_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_MAC_H_

#include "services/device/usb/usb_device.h"

namespace device {

class UsbDeviceMac : public UsbDevice {
 public:
  UsbDeviceMac(uint64_t entry_id, mojom::UsbDeviceInfoPtr device_info);
  UsbDeviceMac(const UsbDeviceMac&) = delete;
  UsbDeviceMac& operator=(const UsbDeviceMac&) = delete;

  // UsbDevice implementation:
  void Open(OpenCallback callback) override;

  uint64_t entry_id() const { return entry_id_; }

 protected:
  ~UsbDeviceMac() override;

 private:
  const uint64_t entry_id_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_MAC_H_
