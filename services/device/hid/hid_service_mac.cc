// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/hid/hid_service_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/hid/hid_connection_mac.h"
#include "services/device/utils/mac_utils.h"

namespace device {

namespace {

bool TryGetHidDataProperty(io_service_t service,
                           CFStringRef key,
                           std::vector<uint8_t>* result) {
  base::apple::ScopedCFTypeRef<CFDataRef> ref(base::apple::CFCast<CFDataRef>(
      IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0)));
  if (!ref) {
    return false;
  }

  auto data_span = base::apple::CFDataToSpan(ref.get());
  result->assign(data_span.begin(), data_span.end());
  return true;
}

scoped_refptr<HidDeviceInfo> CreateDeviceInfo(
    base::mac::ScopedIOObject<io_service_t> service) {
  uint64_t entry_id;
  IOReturn result = IORegistryEntryGetRegistryEntryID(service.get(), &entry_id);
  if (result != kIOReturnSuccess) {
    HID_LOG(EVENT) << "Failed to get IORegistryEntry ID: "
                   << HexErrorCode(result);
    return nullptr;
  }

  std::vector<uint8_t> report_descriptor;
  if (!TryGetHidDataProperty(service.get(), CFSTR(kIOHIDReportDescriptorKey),
                             &report_descriptor)) {
    HID_LOG(DEBUG) << "Device report descriptor not available.";
  }

  int32_t location_id =
      GetIntegerProperty<int32_t>(service.get(), CFSTR(kIOHIDLocationIDKey))
          .value_or(0);
  std::string physical_device_id =
      location_id == 0 ? "" : base::NumberToString(location_id);

  return new HidDeviceInfo(
      entry_id, physical_device_id,
      GetIntegerProperty<int32_t>(service.get(), CFSTR(kIOHIDVendorIDKey))
          .value_or(0),
      GetIntegerProperty<int32_t>(service.get(), CFSTR(kIOHIDProductIDKey))
          .value_or(0),
      GetStringProperty<std::string>(service.get(), CFSTR(kIOHIDProductKey))
          .value_or(""),
      GetStringProperty<std::string>(service.get(),
                                     CFSTR(kIOHIDSerialNumberKey))
          .value_or(""),
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
    HID_LOG(DEBUG) << "Failed to listen for device arrival: "
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
    HID_LOG(DEBUG) << "Failed to listen for device removal: "
                   << HexErrorCode(result);
    return;
  }

  // Drain devices_added_iterator_ to arm the notification.
  RemoveDevices();
  FirstEnumerationComplete();
}

HidServiceMac::~HidServiceMac() {}

void HidServiceMac::Connect(const std::string& device_guid,
                            bool allow_protected_reports,
                            bool allow_fido_reports,
                            ConnectCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& map_entry = devices().find(device_guid);
  if (map_entry == devices().end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kBlockingTaskTraits,
      base::BindOnce(&HidServiceMac::OpenOnBlockingThread, map_entry->second),
      base::BindOnce(&HidServiceMac::DeviceOpened, weak_factory_.GetWeakPtr(),
                     map_entry->second, allow_protected_reports,
                     allow_fido_reports, std::move(callback)));
}

base::WeakPtr<HidService> HidServiceMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
base::apple::ScopedCFTypeRef<IOHIDDeviceRef>
HidServiceMac::OpenOnBlockingThread(scoped_refptr<HidDeviceInfo> device_info) {
  DCHECK_EQ(device_info->platform_device_id_map().size(), 1u);
  const auto& platform_device_id =
      device_info->platform_device_id_map().front().platform_device_id;
  base::apple::ScopedCFTypeRef<CFDictionaryRef> matching_dict(
      IORegistryEntryIDMatching(platform_device_id));
  if (!matching_dict.get()) {
    HID_LOG(DEBUG) << "Failed to create matching dictionary for ID: "
                   << platform_device_id;
    return base::apple::ScopedCFTypeRef<IOHIDDeviceRef>();
  }

  // IOServiceGetMatchingService consumes a reference to the matching dictionary
  // passed to it.
  base::mac::ScopedIOObject<io_service_t> service(IOServiceGetMatchingService(
      kIOMasterPortDefault, matching_dict.release()));
  if (!service.get()) {
    HID_LOG(DEBUG) << "IOService not found for ID: " << platform_device_id;
    return base::apple::ScopedCFTypeRef<IOHIDDeviceRef>();
  }

  base::apple::ScopedCFTypeRef<IOHIDDeviceRef> hid_device(
      IOHIDDeviceCreate(kCFAllocatorDefault, service.get()));
  if (!hid_device) {
    HID_LOG(DEBUG) << "Unable to create IOHIDDevice object.";
    return base::apple::ScopedCFTypeRef<IOHIDDeviceRef>();
  }

  IOReturn result = IOHIDDeviceOpen(hid_device.get(), kIOHIDOptionsTypeNone);
  if (result != kIOReturnSuccess) {
    HID_LOG(DEBUG) << "Failed to open device: " << HexErrorCode(result);
    return base::apple::ScopedCFTypeRef<IOHIDDeviceRef>();
  }

  return hid_device;
}

void HidServiceMac::DeviceOpened(
    scoped_refptr<HidDeviceInfo> device_info,
    bool allow_protected_reports,
    bool allow_fido_reports,
    ConnectCallback callback,
    base::apple::ScopedCFTypeRef<IOHIDDeviceRef> hid_device) {
  if (hid_device) {
    std::move(callback).Run(base::MakeRefCounted<HidConnectionMac>(
        std::move(hid_device), std::move(device_info), allow_protected_reports,
        allow_fido_reports));
  } else {
    std::move(callback).Run(nullptr);
  }
}

// static
void HidServiceMac::FirstMatchCallback(void* context, io_iterator_t iterator) {
  DCHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());
  HidServiceMac* service = static_cast<HidServiceMac*>(context);
  DCHECK_EQ(service->devices_added_iterator_.get(), iterator);
  service->AddDevices();
}

// static
void HidServiceMac::TerminatedCallback(void* context, io_iterator_t iterator) {
  DCHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());
  HidServiceMac* service = static_cast<HidServiceMac*>(context);
  DCHECK_EQ(service->devices_removed_iterator_.get(), iterator);
  service->RemoveDevices();
}

void HidServiceMac::AddDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_added_iterator_.get())), device) {
    scoped_refptr<HidDeviceInfo> device_info =
        CreateDeviceInfo(std::move(device));
    if (device_info)
      AddDevice(device_info);
  }
}

void HidServiceMac::RemoveDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_removed_iterator_.get())),
         device.get()) {
    uint64_t entry_id;
    IOReturn result =
        IORegistryEntryGetRegistryEntryID(device.get(), &entry_id);
    if (result == kIOReturnSuccess)
      RemoveDevice(entry_id);
  }
}

}  // namespace device
