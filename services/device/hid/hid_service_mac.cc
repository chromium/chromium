// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/hid/hid_connection_mac.h"

namespace device {

namespace {

std::string HexErrorCode(IOReturn error_code) {
  return base::StringPrintf("0x%04x", error_code);
}

int32_t GetIntProperty(io_service_t service, CFStringRef key) {
  base::ScopedCFTypeRef<CFNumberRef> ref(base::mac::CFCast<CFNumberRef>(
      IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0)));
  int32_t result;
  if (ref && CFNumberGetValue(ref, kCFNumberSInt32Type, &result))
    return result;
  return 0;
}

std::string GetStringProperty(io_service_t service, CFStringRef key) {
  base::ScopedCFTypeRef<CFStringRef> ref(base::mac::CFCast<CFStringRef>(
      IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0)));
  if (ref)
    return base::SysCFStringRefToUTF8(ref);
  return std::string();
}

bool TryGetHidDataProperty(io_service_t service,
                           CFStringRef key,
                           std::vector<uint8_t>* result) {
  base::ScopedCFTypeRef<CFDataRef> ref(base::mac::CFCast<CFDataRef>(
      IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0)));
  if (!ref)
    return false;

  base::STLClearObject(result);
  const uint8_t* bytes = CFDataGetBytePtr(ref);
  result->insert(result->begin(), bytes, bytes + CFDataGetLength(ref));
  return true;
}

scoped_refptr<HidDeviceInfo> CreateDeviceInfo(
    base::mac::ScopedIOObject<io_service_t> service) {
  uint64_t entry_id;
  IOReturn result = IORegistryEntryGetRegistryEntryID(service, &entry_id);
  if (result != kIOReturnSuccess) {
    HID_LOG(EVENT) << "Failed to get IORegistryEntry ID: "
                   << HexErrorCode(result);
    return nullptr;
  }

  std::vector<uint8_t> report_descriptor;
  if (!TryGetHidDataProperty(service, CFSTR(kIOHIDReportDescriptorKey),
                             &report_descriptor)) {
    HID_LOG(DEBUG) << "Device report descriptor not available.";
  }

  return new HidDeviceInfo(
      entry_id, GetIntProperty(service, CFSTR(kIOHIDVendorIDKey)),
      GetIntProperty(service, CFSTR(kIOHIDProductIDKey)),
      GetStringProperty(service, CFSTR(kIOHIDProductKey)),
      GetStringProperty(service, CFSTR(kIOHIDSerialNumberKey)),
      // TODO(reillyg): Detect Bluetooth. crbug.com/443335
      mojom::HidBusType::kHIDBusTypeUSB, report_descriptor);
}

}  // namespace

HidServiceMac::HidServiceMac() : weak_factory_(this) {
  notify_port_.reset(IONotificationPortCreate(kIOMasterPortDefault));
  CFRunLoopAddSource(CFRunLoopGetMain(),
                     IONotificationPortGetRunLoopSource(notify_port_.get()),
                     kCFRunLoopDefaultMode);

  IOReturn result = IOServiceAddMatchingNotification(
      notify_port_.get(), kIOFirstMatchNotification,
      IOServiceMatching(kIOHIDDeviceKey), FirstMatchCallback, this,
      devices_added_iterator_.InitializeInto());
  if (result != kIOReturnSuccess) {
    HID_LOG(ERROR) << "Failed to listen for device arrival: "
                   << HexErrorCode(result);
    return;
  }

  // Drain the iterator to arm the notification.
  AddDevices();

  result = IOServiceAddMatchingNotification(
      notify_port_.get(), kIOTerminatedNotification,
      IOServiceMatching(kIOHIDDeviceKey), TerminatedCallback, this,
      devices_removed_iterator_.InitializeInto());
  if (result != kIOReturnSuccess) {
    HID_LOG(ERROR) << "Failed to listen for device removal: "
                   << HexErrorCode(result);
    return;
  }

  // Drain devices_added_iterator_ to arm the notification.
  RemoveDevices();
  FirstEnumerationComplete();
}

HidServiceMac::~HidServiceMac() {}

void HidServiceMac::Connect(const std::string& device_guid,
                            const ConnectCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& map_entry = devices().find(device_guid);
  if (map_entry == devices().end()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, nullptr));
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kBlockingTaskTraits,
      base::BindOnce(&HidServiceMac::OpenOnBlockingThread, map_entry->second),
      base::BindOnce(&HidServiceMac::DeviceOpened, weak_factory_.GetWeakPtr(),
                     map_entry->second, callback));
}

base::WeakPtr<HidService> HidServiceMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
base::ScopedCFTypeRef<IOHIDDeviceRef> HidServiceMac::OpenOnBlockingThread(
    scoped_refptr<HidDeviceInfo> device_info) {
  base::ScopedCFTypeRef<CFDictionaryRef> matching_dict(
      IORegistryEntryIDMatching(device_info->platform_device_id()));
  if (!matching_dict.get()) {
    HID_LOG(EVENT) << "Failed to create matching dictionary for ID: "
                   << device_info->platform_device_id();
    return base::ScopedCFTypeRef<IOHIDDeviceRef>();
  }

  // IOServiceGetMatchingService consumes a reference to the matching dictionary
  // passed to it.
  base::mac::ScopedIOObject<io_service_t> service(IOServiceGetMatchingService(
      kIOMasterPortDefault, matching_dict.release()));
  if (!service.get()) {
    HID_LOG(EVENT) << "IOService not found for ID: "
                   << device_info->platform_device_id();
    return base::ScopedCFTypeRef<IOHIDDeviceRef>();
  }

  base::ScopedCFTypeRef<IOHIDDeviceRef> hid_device(
      IOHIDDeviceCreate(kCFAllocatorDefault, service));
  if (!hid_device) {
    HID_LOG(EVENT) << "Unable to create IOHIDDevice object.";
    return base::ScopedCFTypeRef<IOHIDDeviceRef>();
  }

  IOReturn result = IOHIDDeviceOpen(hid_device, kIOHIDOptionsTypeNone);
  if (result != kIOReturnSuccess) {
    HID_LOG(EVENT) << "Failed to open device: " << HexErrorCode(result);
    return base::ScopedCFTypeRef<IOHIDDeviceRef>();
  }

  return hid_device;
}

void HidServiceMac::DeviceOpened(
    scoped_refptr<HidDeviceInfo> device_info,
    const ConnectCallback& callback,
    base::ScopedCFTypeRef<IOHIDDeviceRef> hid_device) {
  if (hid_device) {
    callback.Run(base::MakeRefCounted<HidConnectionMac>(
        std::move(hid_device), std::move(device_info)));
  } else {
    callback.Run(nullptr);
  }
}

// static
void HidServiceMac::FirstMatchCallback(void* context, io_iterator_t iterator) {
  DCHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());
  HidServiceMac* service = static_cast<HidServiceMac*>(context);
  DCHECK_EQ(service->devices_added_iterator_, iterator);
  service->AddDevices();
}

// static
void HidServiceMac::TerminatedCallback(void* context, io_iterator_t iterator) {
  DCHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());
  HidServiceMac* service = static_cast<HidServiceMac*>(context);
  DCHECK_EQ(service->devices_removed_iterator_, iterator);
  service->RemoveDevices();
}

void HidServiceMac::AddDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_added_iterator_)), device) {
    scoped_refptr<HidDeviceInfo> device_info =
        CreateDeviceInfo(std::move(device));
    if (device_info)
      AddDevice(device_info);
  }
}

void HidServiceMac::RemoveDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_removed_iterator_)), device) {
    uint64_t entry_id;
    IOReturn result = IORegistryEntryGetRegistryEntryID(device, &entry_id);
    if (result == kIOReturnSuccess)
      RemoveDevice(entry_id);
  }
}

}  // namespace device
