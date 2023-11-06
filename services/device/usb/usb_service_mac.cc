// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_service_mac.h"

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOReturn.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_mac.h"
#include "services/device/utils/mac_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

namespace {

// USB class codes are defined by the USB specification.
// https://www.usb.org/defined-class-codes
constexpr uint8_t kDeviceClassHub = 0x09;

}  // namespace

UsbServiceMac::UsbServiceMac() {
  notify_port_.reset(IONotificationPortCreate(kIOMasterPortDefault));
  CFRunLoopAddSource(CFRunLoopGetMain(),
                     IONotificationPortGetRunLoopSource(notify_port_.get()),
                     kCFRunLoopDefaultMode);

  IOReturn result = IOServiceAddMatchingNotification(
      notify_port_.get(), kIOFirstMatchNotification,
      IOServiceMatching(kIOUSBDeviceClassName), FirstMatchCallback, this,
      devices_added_iterator_.InitializeInto());
  if (result != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Failed to listen for device arrival: " << std::hex
                   << result << ".";
    return;
  }
  // Drain |devices_added_iterator_| to arm the notification.
  AddDevices();

  result = IOServiceAddMatchingNotification(
      notify_port_.get(), kIOTerminatedNotification,
      IOServiceMatching(kIOUSBDeviceClassName), TerminatedCallback, this,
      devices_removed_iterator_.InitializeInto());
  if (result != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Failed to listen for device removal: " << std::hex
                   << result << ".";
    return;
  }

  // Drain |devices_removed_iterator_| to arm the notification.
  RemoveDevices();
}

UsbServiceMac::~UsbServiceMac() = default;

// static
void UsbServiceMac::FirstMatchCallback(void* context, io_iterator_t iterator) {
  DCHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());
  UsbServiceMac* service = reinterpret_cast<UsbServiceMac*>(context);
  DCHECK_EQ(service->devices_added_iterator_.get(), iterator);
  service->AddDevices();
}

// static
void UsbServiceMac::TerminatedCallback(void* context, io_iterator_t iterator) {
  DCHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());
  UsbServiceMac* service = reinterpret_cast<UsbServiceMac*>(context);
  DCHECK_EQ(service->devices_removed_iterator_.get(), iterator);
  service->RemoveDevices();
}

void UsbServiceMac::AddDevices() {
  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_added_iterator_.get())), device) {
    AddDevice(device.get());
  }
}

void UsbServiceMac::AddDevice(io_service_t device) {
  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> plugin_interface;
  int32_t score;

  // This call fails sometimes due to a resource shortage.
  // TODO(richardmachado): Figure out what is causing this failure.
  IOReturn kr = IOCreatePlugInInterfaceForService(
      device, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
      plugin_interface.InitializeInto(), &score);

  if ((kr != kIOReturnSuccess) || !plugin_interface.get()) {
    USB_LOG(ERROR) << "Unable to create a plug-in: " << std::hex << kr << ".";
    return;
  }

  base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface182> device_interface;
  kr = (*plugin_interface.get())
           ->QueryInterface(
               plugin_interface.get(),
               CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
               reinterpret_cast<LPVOID*>(device_interface.InitializeInto()));

  if (kr != kIOReturnSuccess || !device_interface) {
    USB_LOG(ERROR) << "Couldn’t create a device interface.";
    return;
  }

  uint8_t device_class;
  if ((*device_interface.get())
          ->GetDeviceClass(device_interface.get(), &device_class) !=
      kIOReturnSuccess) {
    return;
  }

  // We don't want to enumerate hubs.
  if (device_class == kDeviceClassHub)
    return;

  uint16_t vendor_id;
  if ((*device_interface.get())
          ->GetDeviceVendor(device_interface.get(), &vendor_id) !=
      kIOReturnSuccess) {
    return;
  }

  uint16_t product_id;
  if ((*device_interface.get())
          ->GetDeviceProduct(device_interface.get(), &product_id) !=
      kIOReturnSuccess) {
    return;
  }

  uint8_t device_protocol;
  if ((*device_interface.get())
          ->GetDeviceProtocol(device_interface.get(), &device_protocol) !=
      kIOReturnSuccess) {
    return;
  }

  uint8_t device_subclass;
  if ((*device_interface.get())
          ->GetDeviceSubClass(device_interface.get(), &device_subclass) !=
      kIOReturnSuccess) {
    return;
  }

  uint16_t device_version;
  if ((*device_interface.get())
          ->GetDeviceReleaseNumber(device_interface.get(), &device_version) !=
      kIOReturnSuccess) {
    return;
  }

  uint32_t location_id;
  if ((*device_interface.get())
          ->GetLocationID(device_interface.get(), &location_id) !=
      kIOReturnSuccess) {
    return;
  }

  uint64_t entry_id;
  if (IORegistryEntryGetRegistryEntryID(device, &entry_id) != kIOReturnSuccess)
    return;

  absl::optional<uint8_t> property_uint8 =
      GetIntegerProperty<uint8_t>(device, CFSTR("PortNum"));
  if (!property_uint8.has_value())
    return;
  uint8_t port_number = property_uint8.value();

  absl::optional<uint16_t> property_uint16 =
      GetIntegerProperty<uint16_t>(device, CFSTR("bcdUSB"));
  uint16_t usb_version;
  if (!property_uint16.has_value())
    return;
  usb_version = property_uint16.value();

  absl::optional<std::u16string> property_string16 =
      GetStringProperty<std::u16string>(device, CFSTR(kUSBVendorString));
  std::u16string manufacturer_string;
  if (property_string16.has_value())
    manufacturer_string = property_string16.value();

  property_string16 =
      GetStringProperty<std::u16string>(device, CFSTR(kUSBSerialNumberString));
  std::u16string serial_number_string;
  if (property_string16.has_value())
    serial_number_string = property_string16.value();

  property_string16 =
      GetStringProperty<std::u16string>(device, CFSTR(kUSBProductString));
  std::u16string product_string;
  if (property_string16.has_value())
    product_string = property_string16.value();

  uint8_t num_config;
  if ((*device_interface.get())
          ->GetNumberOfConfigurations(device_interface.get(), &num_config) !=
      kIOReturnSuccess) {
    return;
  }

  // Populate device descriptor with all necessary configuration info.
  auto descriptor = std::make_unique<UsbDeviceDescriptor>();
  IOUSBConfigurationDescriptorPtr desc;
  for (uint8_t i = 0; i < num_config; i++) {
    if ((*device_interface.get())
            ->GetConfigurationDescriptorPtr(device_interface.get(), i, &desc) !=
        kIOReturnSuccess) {
      return;
    }
    if (!descriptor->Parse(base::make_span(reinterpret_cast<uint8_t*>(desc),
                                           desc->wTotalLength))) {
      return;
    }
  }

  descriptor->device_info->usb_version_major = usb_version >> 8;
  descriptor->device_info->usb_version_minor = usb_version >> 4 & 0xf;
  descriptor->device_info->usb_version_subminor = usb_version & 0xf;
  descriptor->device_info->class_code = device_class;
  descriptor->device_info->subclass_code = device_subclass;
  descriptor->device_info->protocol_code = device_protocol;
  descriptor->device_info->vendor_id = vendor_id;
  descriptor->device_info->product_id = product_id;
  descriptor->device_info->device_version_major = device_version >> 8;
  descriptor->device_info->device_version_minor = device_version >> 4 & 0xf;
  descriptor->device_info->device_version_subminor = device_version & 0xf;
  descriptor->device_info->manufacturer_name = manufacturer_string;
  descriptor->device_info->product_name = product_string;
  descriptor->device_info->serial_number = serial_number_string;
  descriptor->device_info->bus_number = location_id >> 24;
  descriptor->device_info->port_number = port_number;

  scoped_refptr<UsbDeviceMac> mac_device =
      new UsbDeviceMac(entry_id, std::move(descriptor->device_info));

  device_map_[entry_id] = mac_device;
  devices()[mac_device->guid()] = mac_device;

  NotifyDeviceAdded(mac_device);
}

void UsbServiceMac::RemoveDevices() {
  base::mac::ScopedIOObject<io_service_t> device;
  while (device.reset(IOIteratorNext(devices_removed_iterator_.get())),
         device) {
    uint64_t entry_id;

    if (kIOReturnSuccess !=
        IORegistryEntryGetRegistryEntryID(device.get(), &entry_id)) {
      continue;
    }

    auto it = device_map_.find(entry_id);
    if (it == device_map_.end())
      continue;

    auto mac_device = it->second;
    device_map_.erase(it);

    auto by_guid_it = devices().find(mac_device->guid());
    devices().erase(by_guid_it);
    NotifyDeviceRemoved(mac_device);
    mac_device->OnDisconnect();
  }
}

}  // namespace device
