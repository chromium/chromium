// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_device_enumerator_linux.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"

namespace device {

namespace {

const char kSerialSubsystem[] = "tty";

const char kHostPathKey[] = "DEVNAME";
const char kHostBusKey[] = "ID_BUS";
const char kVendorIDKey[] = "ID_VENDOR_ID";
const char kProductIDKey[] = "ID_MODEL_ID";
const char kProductNameKey[] = "ID_MODEL";

}  // namespace

// static
std::unique_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create() {
  return std::unique_ptr<SerialDeviceEnumerator>(
      new SerialDeviceEnumeratorLinux());
}

SerialDeviceEnumeratorLinux::SerialDeviceEnumeratorLinux() {
  udev_.reset(udev_new());
}

SerialDeviceEnumeratorLinux::~SerialDeviceEnumeratorLinux() = default;

std::vector<mojom::SerialDeviceInfoPtr>
SerialDeviceEnumeratorLinux::GetDevices() {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  std::vector<mojom::SerialDeviceInfoPtr> devices;
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_.get()));
  if (!enumerate) {
    LOG(ERROR) << "Serial device enumeration failed.";
    return devices;
  }
  if (udev_enumerate_add_match_subsystem(enumerate.get(), kSerialSubsystem)) {
    LOG(ERROR) << "Serial device enumeration failed.";
    return devices;
  }
  if (udev_enumerate_scan_devices(enumerate.get())) {
    LOG(ERROR) << "Serial device enumeration failed.";
    return devices;
  }

  udev_list_entry* entry = udev_enumerate_get_list_entry(enumerate.get());
  for (; entry != NULL; entry = udev_list_entry_get_next(entry)) {
    ScopedUdevDevicePtr device(udev_device_new_from_syspath(
        udev_.get(), udev_list_entry_get_name(entry)));
    // TODO(rockot): There may be a better way to filter serial devices here,
    // but it's not clear what that would be. Udev will list lots of virtual
    // devices with no real endpoint to back them anywhere. The presence of
    // a bus identifier (e.g., "pci" or "usb") seems to be a good heuristic
    // for detecting actual devices.
    const char* path =
        udev_device_get_property_value(device.get(), kHostPathKey);
    const char* bus = udev_device_get_property_value(device.get(), kHostBusKey);
    if (path != NULL && bus != NULL) {
      auto info = mojom::SerialDeviceInfo::New();
      info->path = path;

      const char* vendor_id =
          udev_device_get_property_value(device.get(), kVendorIDKey);
      const char* product_id =
          udev_device_get_property_value(device.get(), kProductIDKey);
      const char* product_name =
          udev_device_get_property_value(device.get(), kProductNameKey);

      uint32_t int_value;
      if (vendor_id && base::HexStringToUInt(vendor_id, &int_value)) {
        info->vendor_id = int_value;
        info->has_vendor_id = true;
      }
      if (product_id && base::HexStringToUInt(product_id, &int_value)) {
        info->product_id = int_value;
        info->has_product_id = true;
      }
      if (product_name)
        info->display_name.emplace(product_name);
      devices.push_back(std::move(info));
    }
  }
  return devices;
}

}  // namespace device
