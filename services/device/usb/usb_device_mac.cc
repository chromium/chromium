// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_mac.h"

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/mac/scoped_ionotificationportref.h"
#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/usb_device_handle_mac.h"

namespace device {

UsbDeviceMac::UsbDeviceMac(uint64_t entry_id,
                           mojom::UsbDeviceInfoPtr device_info)
    : UsbDevice(std::move(device_info)), entry_id_(entry_id) {}

UsbDeviceMac::~UsbDeviceMac() = default;

void UsbDeviceMac::Open(OpenCallback callback) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> matching_dict(
      IORegistryEntryIDMatching(entry_id()));
  if (!matching_dict.get()) {
    USB_LOG(ERROR) << "Failed to create matching dictionary for ID.";
    std::move(callback).Run(nullptr);
    return;
  }
  // IOServiceGetMatchingService consumes a reference to the matching dictionary
  // passed to it.
  base::mac::ScopedIOObject<io_service_t> usb_device(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  matching_dict.release()));
  if (!usb_device.get()) {
    USB_LOG(ERROR) << "IOService not found for ID.";
    std::move(callback).Run(nullptr);
    return;
  }

  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> plugin_interface;
  int32_t score;
  IOReturn kr = IOCreatePlugInInterfaceForService(
      usb_device.get(), kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
      plugin_interface.InitializeInto(), &score);
  if ((kr != kIOReturnSuccess) || !plugin_interface) {
    USB_LOG(ERROR) << "Unable to create a plug-in: " << std::hex << kr;
    std::move(callback).Run(nullptr);
    return;
  }

  base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface187> device_interface;
  kr = (*plugin_interface.get())
           ->QueryInterface(
               plugin_interface.get(),
               CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
               reinterpret_cast<LPVOID*>(device_interface.InitializeInto()));
  if (kr != kIOReturnSuccess || !device_interface) {
    USB_LOG(ERROR) << "Couldn’t create a device interface.";
    std::move(callback).Run(nullptr);
    return;
  }

  kr = (*device_interface.get())->USBDeviceOpen(device_interface.get());
  if (kr != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Failed to open device: " << std::hex << kr;
    std::move(callback).Run(nullptr);
    return;
  }

  auto device_handle = base::MakeRefCounted<UsbDeviceHandleMac>(
      this, std::move(device_interface));

  handles().push_back(device_handle.get());
  std::move(callback).Run(device_handle);
}

}  // namespace device
