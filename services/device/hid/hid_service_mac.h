// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_MAC_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDDevice.h>

#include <string>

#include "base/apple/foundation_util.h"
#include "base/mac/scoped_ionotificationportref.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/device/hid/hid_service.h"

namespace device {

class HidServiceMac : public HidService {
 public:
  HidServiceMac();
  HidServiceMac(const HidServiceMac&) = delete;
  HidServiceMac& operator=(const HidServiceMac&) = delete;
  ~HidServiceMac() override;

  void Connect(const std::string& device_id,
               bool allow_protected_reports,
               bool allow_fido_reports,
               ConnectCallback connect) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

 private:
  static base::apple::ScopedCFTypeRef<IOHIDDeviceRef> OpenOnBlockingThread(
      scoped_refptr<HidDeviceInfo> device_info);
  void DeviceOpened(scoped_refptr<HidDeviceInfo> device_info,
                    bool allow_protected_reports,
                    bool allow_fido_reports,
                    ConnectCallback callback,
                    base::apple::ScopedCFTypeRef<IOHIDDeviceRef> hid_device);

  // IOService matching callbacks.
  static void FirstMatchCallback(void* context, io_iterator_t iterator);
  static void TerminatedCallback(void* context, io_iterator_t iterator);

  void AddDevices();
  void RemoveDevices();

  // Platform notification port.
  base::mac::ScopedIONotificationPortRef notify_port_;
  base::mac::ScopedIOObject<io_iterator_t> devices_added_iterator_;
  base::mac::ScopedIOObject<io_iterator_t> devices_removed_iterator_;

  base::WeakPtrFactory<HidServiceMac> weak_factory_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_SERVICE_MAC_H_
