// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_writer_mac.h"

#include <CoreFoundation/CoreFoundation.h>

namespace device {

HidWriterMac::HidWriterMac(IOHIDDeviceRef device_ref)
    : device_ref_(device_ref) {}

HidWriterMac::~HidWriterMac() = default;

size_t HidWriterMac::WriteOutputReport(base::span<const uint8_t> report) {
  IOReturn success =
      IOHIDDeviceSetReport(device_ref_, kIOHIDReportTypeOutput, report[0],
                           report.data(), report.size_bytes());
  return (success == kIOReturnSuccess) ? report.size_bytes() : 0;
}

}  // namespace device
