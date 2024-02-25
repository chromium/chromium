// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_linux.h"

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/device_event_log/device_event_log.h"
#include "device/udev_linux/udev.h"

namespace device {

namespace {

// Holds information about a TTY driver for serial devices. Each driver creates
// device nodes with a given major number and in a range of minor numbers.
struct SerialDriverInfo {
  int major;
  int minor_start;
  int minor_end;  // Inclusive.
};

std::vector<SerialDriverInfo> ReadSerialDriverInfo(const base::FilePath& path) {
  std::string tty_drivers;
  if (!base::ReadFileToString(path, &tty_drivers)) {
    return {};
  }

  // Each line has information on a single TTY driver.
  std::vector<SerialDriverInfo> serial_drivers;
  for (const auto& line :
       base::SplitStringPiece(tty_drivers, "\n", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string_view> fields = base::SplitStringPiece(
        line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    // The format of each line is:
    //
    //   driver name<SPACE>name<SPACE>major<SPACE>minor range<SPACE>type
    //
    // We only care about drivers that provide the "serial" type. The rest are
    // things like pseudoterminals.
    if (fields.size() < 5 || fields[4] != "serial")
      continue;

    SerialDriverInfo info;
    if (!base::StringToInt(fields[2], &info.major))
      continue;

    std::vector<std::string_view> minor_range = base::SplitStringPiece(
        fields[3], "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (minor_range.size() == 1) {
      if (!base::StringToInt(minor_range[0], &info.minor_start))
        continue;
      info.minor_end = info.minor_start;
    } else if (minor_range.size() == 2) {
      if (!base::StringToInt(minor_range[0], &info.minor_start) ||
          !base::StringToInt(minor_range[1], &info.minor_end)) {
        continue;
      }
    } else {
      continue;
    }

    serial_drivers.push_back(info);
  }

  return serial_drivers;
}

}  // namespace

// static
std::unique_ptr<SerialDeviceEnumeratorLinux>
SerialDeviceEnumeratorLinux::Create() {
  return std::make_unique<SerialDeviceEnumeratorLinux>(
      base::FilePath("/proc/tty/drivers"));
}

SerialDeviceEnumeratorLinux::SerialDeviceEnumeratorLinux(
    const base::FilePath& tty_driver_info_path)
    : tty_driver_info_path_(tty_driver_info_path) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  watcher_ = UdevWatcher::StartWatching(this);
  if (watcher_)
    watcher_->EnumerateExistingDevices();
}

SerialDeviceEnumeratorLinux::~SerialDeviceEnumeratorLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SerialDeviceEnumeratorLinux::OnDeviceAdded(ScopedUdevDevicePtr device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const char* subsystem = udev_device_get_subsystem(device.get());
  if (!subsystem || strcmp(subsystem, "tty") != 0)
    return;

  const char* syspath_str = udev_device_get_syspath(device.get());
  if (!syspath_str)
    return;
  std::string syspath(syspath_str);

  const char* major_str = udev_device_get_property_value(device.get(), "MAJOR");
  const char* minor_str = udev_device_get_property_value(device.get(), "MINOR");

  int major, minor;
  if (!major_str || !minor_str || !base::StringToInt(major_str, &major) ||
      !base::StringToInt(minor_str, &minor)) {
    return;
  }

  for (const auto& driver : ReadSerialDriverInfo(tty_driver_info_path_)) {
    if (major == driver.major && minor >= driver.minor_start &&
        minor <= driver.minor_end) {
      CreatePort(std::move(device), syspath);
      return;
    }
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
  base::UnguessableToken token = it->second;

  paths_.erase(it);
  RemovePort(token);
}

void SerialDeviceEnumeratorLinux::CreatePort(ScopedUdevDevicePtr device,
                                             const std::string& syspath) {
  const char* path = udev_device_get_property_value(device.get(), "DEVNAME");
  if (!path)
    return;

  auto token = base::UnguessableToken::Create();
  auto info = mojom::SerialPortInfo::New();
  info->path = base::FilePath(path);
  info->token = token;

  uint32_t int_value;
  const char* vendor_id =
      udev_device_get_property_value(device.get(), "ID_VENDOR_ID");
  if (vendor_id && base::HexStringToUInt(vendor_id, &int_value)) {
    info->vendor_id = int_value;
    info->has_vendor_id = true;
  }

  const char* product_id =
      udev_device_get_property_value(device.get(), "ID_MODEL_ID");
  if (product_id && base::HexStringToUInt(product_id, &int_value)) {
    info->product_id = int_value;
    info->has_product_id = true;
  }

  const char* product_name_enc =
      udev_device_get_property_value(device.get(), "ID_MODEL_ENC");
  if (product_name_enc)
    info->display_name = device::UdevDecodeString(product_name_enc);

  const char* serial_number =
      udev_device_get_property_value(device.get(), "ID_SERIAL_SHORT");
  if (serial_number)
    info->serial_number = serial_number;

  SERIAL_LOG(EVENT) << "Serial device added: path=" << info->path
                    << " vid=" << (vendor_id ? vendor_id : "(none)")
                    << " pid=" << (product_id ? product_id : "(none)")
                    << " serial=" << info->serial_number.value_or("(none)");

  paths_.insert(std::make_pair(syspath, token));
  AddPort(std::move(info));
}

}  // namespace device
