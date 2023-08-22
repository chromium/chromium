// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_CONNECTION_MAC_H_
#define SERVICES_DEVICE_HID_HID_CONNECTION_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <stddef.h>
#include <stdint.h>

#include "base/apple/foundation_util.h"
#include "base/containers/queue.h"
#include "services/device/hid/hid_connection.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class HidConnectionMac : public HidConnection {
 public:
  HidConnectionMac(base::apple::ScopedCFTypeRef<IOHIDDeviceRef> device,
                   scoped_refptr<HidDeviceInfo> device_info,
                   bool allow_protected_reports,
                   bool allow_fido_reports);
  HidConnectionMac(HidConnectionMac&) = delete;
  HidConnectionMac& operator=(HidConnectionMac&) = delete;

 private:
  ~HidConnectionMac() override;

  // HidConnection implementation.
  void PlatformClose() override;
  void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                     WriteCallback callback) override;
  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override;
  void PlatformSendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                                 WriteCallback callback) override;

  static void InputReportCallback(void* context,
                                  IOReturn result,
                                  void* sender,
                                  IOHIDReportType type,
                                  uint32_t report_id,
                                  uint8_t* report_bytes,
                                  CFIndex report_length);
  void GetFeatureReportAsync(uint8_t report_id, ReadCallback callback);
  void SetReportAsync(IOHIDReportType report_type,
                      scoped_refptr<base::RefCountedBytes> buffer,
                      WriteCallback callback);
  void ReturnAsyncResult(base::OnceClosure callback);

  base::apple::ScopedCFTypeRef<IOHIDDeviceRef> device_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::vector<uint8_t> inbound_buffer_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_CONNECTION_MAC_H_
