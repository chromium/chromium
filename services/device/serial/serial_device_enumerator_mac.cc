// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_mac.h"

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"

namespace device {

namespace {

std::string HexErrorCode(IOReturn error_code) {
  return base::StringPrintf("0x%08x", error_code);
}

// Searches a service and all ancestor services for a property with the
// specified key, returning NULL if no such key was found.
CFTypeRef GetCFProperty(io_service_t service, const CFStringRef key) {
  // We search for the specified property not only on the specified service, but
  // all ancestors of that service. This is important because if a device is
  // both serial and USB, in the registry tree it appears as a serial service
  // with a USB service as its ancestor. Without searching ancestors services
  // for the specified property, we'd miss all USB properties.
  return IORegistryEntrySearchCFProperty(
      service, kIOServicePlane, key, NULL,
      kIORegistryIterateRecursively | kIORegistryIterateParents);
}

// Searches a service and all ancestor services for a string property with the
// specified key, returning NULL if no such key was found.
CFStringRef GetCFStringProperty(io_service_t service, const CFStringRef key) {
  CFTypeRef value = GetCFProperty(service, key);
  if (value && (CFGetTypeID(value) == CFStringGetTypeID()))
    return static_cast<CFStringRef>(value);

  return NULL;
}

// Searches a service and all ancestor services for a number property with the
// specified key, returning NULL if no such key was found.
CFNumberRef GetCFNumberProperty(io_service_t service, const CFStringRef key) {
  CFTypeRef value = GetCFProperty(service, key);
  if (value && (CFGetTypeID(value) == CFNumberGetTypeID()))
    return static_cast<CFNumberRef>(value);

  return NULL;
}

// Searches the specified service for a string property with the specified key,
// sets value to that property's value, and returns whether the operation was
// successful.
bool GetStringProperty(io_service_t service,
                       const CFStringRef key,
                       std::string* value) {
  CFStringRef propValue = GetCFStringProperty(service, key);
  if (propValue) {
    *value = base::SysCFStringRefToUTF8(propValue);
    return true;
  }

  return false;
}

// Searches the specified service for a uint16_t property with the specified
// key, sets value to that property's value, and returns whether the operation
// was successful.
bool GetUInt16Property(io_service_t service,
                       const CFStringRef key,
                       uint16_t* value) {
  CFNumberRef propValue = GetCFNumberProperty(service, key);
  if (propValue) {
    int intValue;
    if (CFNumberGetValue(propValue, kCFNumberIntType, &intValue)) {
      *value = static_cast<uint16_t>(intValue);
      return true;
    }
  }

  return false;
}

}  // namespace

// static
std::unique_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create() {
  return std::make_unique<SerialDeviceEnumeratorMac>();
}

SerialDeviceEnumeratorMac::SerialDeviceEnumeratorMac() {
  notify_port_.reset(IONotificationPortCreate(kIOMasterPortDefault));
  CFRunLoopAddSource(CFRunLoopGetMain(),
                     IONotificationPortGetRunLoopSource(notify_port_.get()),
                     kCFRunLoopDefaultMode);

  IOReturn result = IOServiceAddMatchingNotification(
      notify_port_.get(), kIOFirstMatchNotification,
      IOServiceMatching(kIOSerialBSDServiceValue), FirstMatchCallback, this,
      devices_added_iterator_.InitializeInto());
  if (result != kIOReturnSuccess) {
    DLOG(ERROR) << "Failed to listen for device arrival: "
                << HexErrorCode(result);
    return;
  }

  // Drain |devices_added_iterator_| to arm the notification.
  AddDevices();

  result = IOServiceAddMatchingNotification(
      notify_port_.get(), kIOTerminatedNotification,
      IOServiceMatching(kIOSerialBSDServiceValue), TerminatedCallback, this,
      devices_removed_iterator_.InitializeInto());
  if (result != kIOReturnSuccess) {
    DLOG(ERROR) << "Failed to listen for device removal: "
                << HexErrorCode(result);
    return;
  }

  // Drain |devices_removed_iterator_| to arm the notification.
  RemoveDevices();
}

SerialDeviceEnumeratorMac::~SerialDeviceEnumeratorMac() {}

std::vector<mojom::SerialPortInfoPtr> SerialDeviceEnumeratorMac::GetDevices() {
  std::vector<mojom::SerialPortInfoPtr> ports;
  ports.reserve(ports_.size());
  for (const auto& map_entry : ports_)
    ports.push_back(map_entry.second->Clone());
  return ports;
}

base::Optional<base::FilePath> SerialDeviceEnumeratorMac::GetPathFromToken(
    const base::UnguessableToken& token) {
  auto it = ports_.find(token);
  if (it == ports_.end())
    return base::nullopt;
  return it->second->path;
}

// static
void SerialDeviceEnumeratorMac::FirstMatchCallback(void* context,
                                                   io_iterator_t iterator) {
  auto* enumerator = static_cast<SerialDeviceEnumeratorMac*>(context);
  DCHECK_EQ(enumerator->devices_added_iterator_, iterator);
  enumerator->AddDevices();
}

// static
void SerialDeviceEnumeratorMac::TerminatedCallback(void* context,
                                                   io_iterator_t iterator) {
  auto* enumerator = static_cast<SerialDeviceEnumeratorMac*>(context);
  DCHECK_EQ(enumerator->devices_removed_iterator_, iterator);
  enumerator->RemoveDevices();
}

void SerialDeviceEnumeratorMac::AddDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_added_iterator_)), device) {
    uint64_t entry_id;
    IOReturn result = IORegistryEntryGetRegistryEntryID(device, &entry_id);
    if (result != kIOReturnSuccess)
      continue;

    auto callout_info = mojom::SerialPortInfo::New();
    uint16_t vendorId;
    if (GetUInt16Property(device.get(), CFSTR(kUSBVendorID), &vendorId)) {
      callout_info->has_vendor_id = true;
      callout_info->vendor_id = vendorId;
    }

    uint16_t productId;
    if (GetUInt16Property(device.get(), CFSTR(kUSBProductID), &productId)) {
      callout_info->has_product_id = true;
      callout_info->product_id = productId;
    }

    std::string display_name;
    if (GetStringProperty(device.get(), CFSTR(kUSBProductString),
                          &display_name)) {
      callout_info->display_name = std::move(display_name);
    }

    // Each serial device has two "paths" in /dev/ associated with it: a
    // "dialin" path starting with "tty" and a "callout" path starting with
    // "cu". Each of these is considered a different device from Chrome's
    // standpoint, but both should share the device's USB properties.
    std::string dialin_device;
    if (GetStringProperty(device.get(), CFSTR(kIODialinDeviceKey),
                          &dialin_device)) {
      auto token = base::UnguessableToken::Create();
      mojom::SerialPortInfoPtr dialin_info = callout_info.Clone();
      dialin_info->path = base::FilePath(dialin_device);
      dialin_info->token = token;
      ports_.insert(std::make_pair(token, std::move(dialin_info)));
      entries_[entry_id].first = token;
    }

    std::string callout_device;
    if (GetStringProperty(device.get(), CFSTR(kIOCalloutDeviceKey),
                          &callout_device)) {
      auto token = base::UnguessableToken::Create();
      callout_info->path = base::FilePath(callout_device);
      callout_info->token = token;
      ports_.insert(std::make_pair(token, std::move(callout_info)));
      entries_[entry_id].second = token;
    }
  }
}

void SerialDeviceEnumeratorMac::RemoveDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_removed_iterator_)), device) {
    uint64_t entry_id;
    IOReturn result = IORegistryEntryGetRegistryEntryID(device, &entry_id);
    if (result != kIOReturnSuccess)
      continue;

    auto it = entries_.find(entry_id);
    if (it == entries_.end())
      continue;

    if (it->second.first)
      ports_.erase(it->second.first);
    if (it->second.second)
      ports_.erase(it->second.second);
    entries_.erase(it);
  }
}

}  // namespace device
