// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_linux.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"

namespace device {

namespace {

const char kSerialSubsystem[] = "tty";

const char kHostPathKey[] = "DEVNAME";
const char kHostBusKey[] = "ID_BUS";
const char kMajorKey[] = "MAJOR";
const char kVendorIDKey[] = "ID_VENDOR_ID";
const char kProductIDKey[] = "ID_MODEL_ID";
const char kProductNameKey[] = "ID_MODEL";

// The major number for an RFCOMM TTY device.
const char kRfcommMajor[] = "216";

}  // namespace

// static
std::unique_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create() {
  return std::make_unique<SerialDeviceEnumeratorLinux>();
}

SerialDeviceEnumeratorLinux::SerialDeviceEnumeratorLinux() {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  watcher_ = UdevWatcher::StartWatching(this);
  watcher_->EnumerateExistingDevices();
}

SerialDeviceEnumeratorLinux::~SerialDeviceEnumeratorLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::vector<mojom::SerialPortInfoPtr>
SerialDeviceEnumeratorLinux::GetDevices() {
  std::vector<mojom::SerialPortInfoPtr> ports;
  ports.reserve(ports_.size());
  for (const auto& map_entry : ports_)
    ports.push_back(map_entry.second->Clone());
  return ports;
}

base::Optional<base::FilePath> SerialDeviceEnumeratorLinux::GetPathFromToken(
    const base::UnguessableToken& token) {
  auto it = ports_.find(token);
  if (it == ports_.end())
    return base::nullopt;
  return it->second->path;
}

void SerialDeviceEnumeratorLinux::OnDeviceAdded(ScopedUdevDevicePtr device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const char* subsystem = udev_device_get_subsystem(device.get());
  if (!subsystem || strcmp(subsystem, kSerialSubsystem) != 0)
    return;

  const char* syspath_str = udev_device_get_syspath(device.get());
  if (!syspath_str)
    return;
  std::string syspath(syspath_str);

  // Platform serial ports.
  if (base::StartsWith(syspath, "/sys/devices/platform/",
                       base::CompareCase::SENSITIVE)) {
    CreatePort(std::move(device), syspath);
    return;
  }

  // USB serial ports and others that have a proper bus identifier.
  const char* bus = udev_device_get_property_value(device.get(), kHostBusKey);
  if (bus) {
    CreatePort(std::move(device), syspath);
    return;
  }

  // Bluetooth ports are virtual TTYs but have an identifiable major number.
  const char* major = udev_device_get_property_value(device.get(), kMajorKey);
  if (major && base::StringPiece(major) == kRfcommMajor) {
    CreatePort(std::move(device), syspath);
    return;
  }
}

void SerialDeviceEnumeratorLinux::OnDeviceChanged(ScopedUdevDevicePtr device) {}

void SerialDeviceEnumeratorLinux::OnDeviceRemoved(ScopedUdevDevicePtr device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const char* syspath = udev_device_get_syspath(device.get());
  if (!syspath)
    return;

  auto it = paths_.find(syspath);
  if (it == paths_.end())
    return;

  ports_.erase(it->second);
  paths_.erase(it);
}

void SerialDeviceEnumeratorLinux::CreatePort(ScopedUdevDevicePtr device,
                                             const std::string& syspath) {
  const char* path = udev_device_get_property_value(device.get(), kHostPathKey);
  if (!path)
    return;

  auto token = base::UnguessableToken::Create();
  auto info = mojom::SerialPortInfo::New();
  info->path = base::FilePath(path);
  info->token = token;

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

  ports_.insert(std::make_pair(token, std::move(info)));
  paths_.insert(std::make_pair(syspath, token));
}

}  // namespace device
