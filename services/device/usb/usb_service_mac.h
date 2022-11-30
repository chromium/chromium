// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_SERVICE_MAC_H_
#define SERVICES_DEVICE_USB_USB_SERVICE_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>

#include <unordered_map>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_ionotificationportref.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/weak_ptr.h"
#include "services/device/usb/usb_service.h"

namespace device {

class UsbDeviceMac;

// The USB service is responsible for device discovery on the system, which
// allows it to re-use device handles to prevent competition for the same USB
// device.
class UsbServiceMac final : public UsbService {
 public:
  UsbServiceMac();
  UsbServiceMac(const UsbServiceMac&) = delete;
  UsbServiceMac& operator=(const UsbServiceMac&) = delete;
  ~UsbServiceMac() override;

 private:
  // IOService matching callbacks.
  static void FirstMatchCallback(void* context, io_iterator_t iterator);
  static void TerminatedCallback(void* context, io_iterator_t iterator);

  void AddDevices();
  void AddDevice(io_service_t device);
  void RemoveDevices();

  std::unordered_map<uint64_t, scoped_refptr<UsbDeviceMac>> device_map_;

  base::mac::ScopedIONotificationPortRef notify_port_;
  base::mac::ScopedIOObject<io_iterator_t> devices_added_iterator_;
  base::mac::ScopedIOObject<io_iterator_t> devices_removed_iterator_;

  base::WeakPtrFactory<UsbServiceMac> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_SERVICE_MAC_H_
