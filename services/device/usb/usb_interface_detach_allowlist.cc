// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_interface_detach_allowlist.h"

#include <algorithm>

namespace device {
namespace {

using ::device::mojom::kUsbCommClass;

constexpr uint8_t kUsbCdcSubClassAcm = 2;
constexpr uint8_t kUsbCdcProtoNone = 0;
constexpr uint8_t kUsbCdcProtoAtV25ter = 1;
constexpr uint8_t kUsbCdcProtoAtPcca101 = 2;
constexpr uint8_t kUsbCdcProtoAtPcca101Wake = 3;
constexpr uint8_t kUsbCdcProtoAtGsm = 4;
constexpr uint8_t kUsbCdcProtoAt3G = 5;
constexpr uint8_t kUsbCdcProtoAtCdma = 6;

}  // namespace

UsbInterfaceDetachAllowlist::UsbInterfaceDetachAllowlist()
    : UsbInterfaceDetachAllowlist({
          // CDC/ACM (communication) devices
          {/*driver_name=*/"cdc_acm",
           /*protocols=without any protocol set or with AT-command sets*/
           {
               {kUsbCommClass, kUsbCdcSubClassAcm, kUsbCdcProtoNone},
               {kUsbCommClass, kUsbCdcSubClassAcm, kUsbCdcProtoAtV25ter},
               {kUsbCommClass, kUsbCdcSubClassAcm, kUsbCdcProtoAtPcca101},
               {kUsbCommClass, kUsbCdcSubClassAcm, kUsbCdcProtoAtPcca101Wake},
               {kUsbCommClass, kUsbCdcSubClassAcm, kUsbCdcProtoAtGsm},
               {kUsbCommClass, kUsbCdcSubClassAcm, kUsbCdcProtoAt3G},
               {kUsbCommClass, kUsbCdcSubClassAcm, kUsbCdcProtoAtCdma},
           }},

          // Printers
          {/*driver_name=*/"usblp", /*protocols=ALL*/ {}},

          // FTDI serial devices
          {/*driver_name=*/"ftdi_sio", /*protocols=ALL*/ {}},
      }) {}

UsbInterfaceDetachAllowlist::UsbInterfaceDetachAllowlist(
    std::vector<Entry> entries)
    : entries_(std::move(entries)) {}

UsbInterfaceDetachAllowlist::~UsbInterfaceDetachAllowlist() = default;

// static
const UsbInterfaceDetachAllowlist& UsbInterfaceDetachAllowlist::Get() {
  static base::NoDestructor<UsbInterfaceDetachAllowlist> instance;
  return *instance;
}

bool UsbInterfaceDetachAllowlist::CanDetach(
    std::string_view driver_name,
    const mojom::UsbAlternateInterfaceInfo& interface_info) const {
  return std::ranges::any_of(entries_, [&](const Entry& rule) {
    if (rule.driver_name != driver_name) {
      return false;
    }
    if (rule.protocols.empty()) {
      return true;
    }
    return std::ranges::any_of(rule.protocols, [&](const Protocol& protocol) {
      return protocol.class_code == interface_info.class_code &&
             protocol.subclass_code == interface_info.subclass_code &&
             protocol.protocol_code == interface_info.protocol_code;
    });
  });
}

UsbInterfaceDetachAllowlist::Entry::Entry(
    const std::string& driver_name,
    const std::vector<Protocol>& protocols)
    : driver_name(driver_name), protocols(protocols) {}

UsbInterfaceDetachAllowlist::Entry::Entry(
    const UsbInterfaceDetachAllowlist::Entry& other) = default;

UsbInterfaceDetachAllowlist::Entry::~Entry() = default;

}  // namespace device
