// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_mac.h"

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBHostFamilyDefinitions.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USBSpec.h>
#include <stdint.h>

#include <optional>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/scoped_policy.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/device_event_log/device_event_log.h"

namespace device {

namespace {

std::string HexErrorCode(IOReturn error_code) {
  return base::StringPrintf("0x%08x", error_code);
}

// Searches a service and all ancestor services for a property with the
// specified key, returning null if no such key was found.
base::apple::ScopedCFTypeRef<CFTypeRef> GetCFProperty(io_service_t service,
                                                      const CFStringRef key) {
  // We search for the specified property not only on the specified service, but
  // all ancestors of that service. This is important because if a device is
  // both serial and USB, in the registry tree it appears as a serial service
  // with a USB service as its ancestor. Without searching ancestors services
  // for the specified property, we'd miss all USB properties.
  return base::apple::ScopedCFTypeRef<CFTypeRef>(
      IORegistryEntrySearchCFProperty(
          service, kIOServicePlane, key, nullptr,
          kIORegistryIterateRecursively | kIORegistryIterateParents));
}

// Searches a service and all ancestor services for a string property with the
// specified key, returning null if no such key was found.
base::apple::ScopedCFTypeRef<CFStringRef> GetCFStringProperty(
    io_service_t service,
    const CFStringRef key) {
  base::apple::ScopedCFTypeRef<CFTypeRef> value = GetCFProperty(service, key);

  return base::apple::ScopedCFTypeRef<CFStringRef>(
      base::apple::CFCast<CFStringRef>(value.get()),
      base::scoped_policy::RETAIN);
}

// Searches a service and all ancestor services for a number property with the
// specified key, returning null if no such key was found.
base::apple::ScopedCFTypeRef<CFNumberRef> GetCFNumberProperty(
    io_service_t service,
    const CFStringRef key) {
  base::apple::ScopedCFTypeRef<CFTypeRef> value = GetCFProperty(service, key);

  return base::apple::ScopedCFTypeRef<CFNumberRef>(
      base::apple::CFCast<CFNumberRef>(value.get()),
      base::scoped_policy::RETAIN);
}

// Searches the specified service for a string property with the specified key.
std::optional<std::string> GetStringProperty(io_service_t service,
                                             const CFStringRef key) {
  base::apple::ScopedCFTypeRef<CFStringRef> property =
      GetCFStringProperty(service, key);
  if (property) {
    return base::SysCFStringRefToUTF8(property.get());
  }

  return std::nullopt;
}

// Searches the specified service for a uint16_t property with the specified
// key.
std::optional<uint16_t> GetUInt16Property(io_service_t service,
                                          const CFStringRef key) {
  base::apple::ScopedCFTypeRef<CFNumberRef> property =
      GetCFNumberProperty(service, key);
  if (property) {
    int value;
    if (CFNumberGetValue(property.get(), kCFNumberIntType, &value)) {
      return static_cast<uint16_t>(value);
    }
  }

  return std::nullopt;
}

// Finds the name of the USB driver for |device| by walking up the
// IORegistry tree to find the first entry provided by the IOUSBHostInterface
// class.
std::optional<std::string> GetUsbDriverName(
    base::mac::ScopedIOObject<io_object_t> device) {
  base::mac::ScopedIOObject<io_iterator_t> iterator;
  kern_return_t kr = IORegistryEntryCreateIterator(
      device.get(), kIOServicePlane,
      kIORegistryIterateRecursively | kIORegistryIterateParents,
      iterator.InitializeInto());
  if (kr != KERN_SUCCESS) {
    return std::nullopt;
  }

  base::mac::ScopedIOObject<io_service_t> ancestor;
  while (ancestor.reset(IOIteratorNext(iterator.get())), ancestor) {
    std::optional<std::string> provider_class =
        GetStringProperty(ancestor.get(), CFSTR(kIOProviderClassKey));
    if (provider_class && *provider_class == kIOUSBHostInterfaceClassName) {
      return GetStringProperty(ancestor.get(), kCFBundleIdentifierKey);
    }
  }

  return std::nullopt;
}

}  // namespace

SerialDeviceEnumeratorMac::SerialDeviceEnumeratorMac() {
  notify_port_.reset(IONotificationPortCreate(kIOMainPortDefault));
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

SerialDeviceEnumeratorMac::~SerialDeviceEnumeratorMac() = default;

// static
void SerialDeviceEnumeratorMac::FirstMatchCallback(void* context,
                                                   io_iterator_t iterator) {
  auto* enumerator = static_cast<SerialDeviceEnumeratorMac*>(context);
  DCHECK_EQ(enumerator->devices_added_iterator_.get(), iterator);
  enumerator->AddDevices();
}

// static
void SerialDeviceEnumeratorMac::TerminatedCallback(void* context,
                                                   io_iterator_t iterator) {
  auto* enumerator = static_cast<SerialDeviceEnumeratorMac*>(context);
  DCHECK_EQ(enumerator->devices_removed_iterator_.get(), iterator);
  enumerator->RemoveDevices();
}

void SerialDeviceEnumeratorMac::AddDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_added_iterator_.get())), device) {
    uint64_t entry_id;
    IOReturn result =
        IORegistryEntryGetRegistryEntryID(device.get(), &entry_id);
    if (result != kIOReturnSuccess) {
      continue;
    }

    auto info = mojom::SerialPortInfo::New();
    std::optional<uint16_t> vendor_id =
        GetUInt16Property(device.get(), CFSTR(kUSBVendorID));
    std::optional<std::string> vendor_id_str;
    if (vendor_id) {
      info->has_vendor_id = true;
      info->vendor_id = *vendor_id;
      vendor_id_str = base::StringPrintf("%04X", *vendor_id);
    }

    std::optional<uint16_t> product_id =
        GetUInt16Property(device.get(), CFSTR(kUSBProductID));
    std::optional<std::string> product_id_str;
    if (product_id) {
      info->has_product_id = true;
      info->product_id = *product_id;
      product_id_str = base::StringPrintf("%04X", *product_id);
    }

    info->display_name =
        GetStringProperty(device.get(), CFSTR(kUSBProductString));
    info->serial_number =
        GetStringProperty(device.get(), CFSTR(kUSBSerialNumberString));
    info->usb_driver_name = GetUsbDriverName(device);

    // Each serial device has two paths associated with it: a "dialin" path
    // starting with "tty" and a "callout" path starting with "cu". The
    // call-out device is typically preferred but requesting the dial-in device
    // is supported for the legacy Chrome Apps API.
    std::optional<std::string> dialin_device =
        GetStringProperty(device.get(), CFSTR(kIODialinDeviceKey));
    std::optional<std::string> callout_device =
        GetStringProperty(device.get(), CFSTR(kIOCalloutDeviceKey));

    if (callout_device) {
      info->path = base::FilePath(*callout_device);
      if (dialin_device) {
        info->alternate_path = base::FilePath(*dialin_device);
      }
    } else if (dialin_device) {
      info->path = base::FilePath(*dialin_device);
    } else {
      continue;
    }

    auto token = base::UnguessableToken::Create();
    info->token = token;

    SERIAL_LOG(EVENT) << "Serial device added: dialin="
                      << dialin_device.value_or("(none)")
                      << " callout=" << callout_device.value_or("(none)")
                      << " vid=" << vendor_id_str.value_or("(none)")
                      << " pid=" << product_id_str.value_or("(none)")
                      << " usb_serial="
                      << info->serial_number.value_or("(none)")
                      << " usb_driver="
                      << info->usb_driver_name.value_or("(none)");

    entries_.insert({entry_id, token});
    AddPort(std::move(info));
  }
}

void SerialDeviceEnumeratorMac::RemoveDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_removed_iterator_.get())),
         device) {
    uint64_t entry_id;
    IOReturn result =
        IORegistryEntryGetRegistryEntryID(device.get(), &entry_id);
    if (result != kIOReturnSuccess) {
      continue;
    }

    auto it = entries_.find(entry_id);
    if (it == entries_.end()) {
      continue;
    }

    base::UnguessableToken token = it->second;
    entries_.erase(it);

    RemovePort(token);
  }
}

}  // namespace device
