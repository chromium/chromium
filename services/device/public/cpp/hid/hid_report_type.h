// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_TYPE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_TYPE_H_

namespace device {

// A HID report is a packet of data meaningful to the device. The report type
// indicates the direction and initiator of the report.
enum class HidReportType {
  // Input reports are sent from device to host, initiated by the device.
  kInput,
  // Output reports are sent from host to device, initiated by the host.
  kOutput,
  // Feature reports are sent in either direction, initiated by the host.
  kFeature,
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_TYPE_H_
