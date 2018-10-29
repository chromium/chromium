// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_PUBLIC_CPP_FAKE_USB_DEVICE_INFO_H_
#define DEVICE_USB_PUBLIC_CPP_FAKE_USB_DEVICE_INFO_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

// This class acts like device::UsbDevice and provides mojom::UsbDeviceInfo.
// It should be used together with FakeUsbDeviceManager just for testing.
class FakeUsbDeviceInfo : public base::RefCounted<FakeUsbDeviceInfo> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // This method is called when the FakeUsbDeviceManager that created this
    // object detects that the device has been disconnected from the host.
    virtual void OnDeviceRemoved(scoped_refptr<FakeUsbDeviceInfo> device);
  };

  FakeUsbDeviceInfo(uint16_t vendor_id, uint16_t product_id);
  FakeUsbDeviceInfo(uint16_t vendor_id,
                    uint16_t product_id,
                    const std::string& manufacturer_string,
                    const std::string& product_string,
                    const std::string& serial_number);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void NotifyDeviceRemoved();

  std::string guid() { return device_info_.guid; }
  mojom::UsbDeviceInfoPtr GetDeviceInfo() { return device_info_.Clone(); }

 private:
  friend class RefCounted<FakeUsbDeviceInfo>;
  ~FakeUsbDeviceInfo();
  void SetDefault();

  mojom::UsbDeviceInfo device_info_;
  base::ObserverList<Observer> observer_list_;
  DISALLOW_COPY_AND_ASSIGN(FakeUsbDeviceInfo);
};

}  // namespace device

#endif  // DEVICE_USB_PUBLIC_CPP_FAKE_USB_DEVICE_INFO_H_
