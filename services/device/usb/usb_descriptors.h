// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DESCRIPTORS_H_
#define SERVICES_DEVICE_USB_USB_DESCRIPTORS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace device {

class UsbDeviceHandle;

struct CombinedInterfaceInfo {
  CombinedInterfaceInfo() = default;
  CombinedInterfaceInfo(const mojom::UsbInterfaceInfo* interface,
                        const mojom::UsbAlternateInterfaceInfo* alternate);

  bool IsValid() const;

  const mojom::UsbInterfaceInfo* interface = nullptr;
  const mojom::UsbAlternateInterfaceInfo* alternate = nullptr;
};

struct UsbDeviceDescriptor {
  UsbDeviceDescriptor();
  UsbDeviceDescriptor(const UsbDeviceDescriptor& other) = delete;
  ~UsbDeviceDescriptor();

  // Parses |buffer| for USB descriptors. Any configuration descriptors found
  // will be added to |configurations|. If a device descriptor is found it will
  // be used to populate this struct's fields. This function may be called more
  // than once (i.e. for multiple buffers containing a configuration descriptor
  // each).
  bool Parse(const std::vector<uint8_t>& buffer);

  uint8_t i_manufacturer = 0;
  uint8_t i_product = 0;
  uint8_t i_serial_number = 0;
  uint8_t num_configurations = 0;
  mojom::UsbDeviceInfoPtr device_info;
};

void ReadUsbDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    base::OnceCallback<void(std::unique_ptr<UsbDeviceDescriptor>)> callback);

bool ParseUsbStringDescriptor(const std::vector<uint8_t>& descriptor,
                              base::string16* output);

void ReadUsbStringDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    std::unique_ptr<std::map<uint8_t, base::string16>> index_map,
    base::OnceCallback<void(std::unique_ptr<std::map<uint8_t, base::string16>>)>
        callback);

mojom::UsbEndpointInfoPtr BuildUsbEndpointInfoPtr(const uint8_t* data);

mojom::UsbEndpointInfoPtr BuildUsbEndpointInfoPtr(uint8_t address,
                                                  uint8_t attributes,
                                                  uint16_t maximum_packet_size,
                                                  uint8_t polling_interval);

mojom::UsbInterfaceInfoPtr BuildUsbInterfaceInfoPtr(const uint8_t* data);

mojom::UsbInterfaceInfoPtr BuildUsbInterfaceInfoPtr(uint8_t interface_number,
                                                    uint8_t alternate_setting,
                                                    uint8_t interface_class,
                                                    uint8_t interface_subclass,
                                                    uint8_t interface_protocol);

void AggregateInterfacesForConfig(mojom::UsbConfigurationInfo* config);

CombinedInterfaceInfo FindInterfaceInfoFromConfig(
    const mojom::UsbConfigurationInfo* config,
    uint8_t interface_number,
    uint8_t alternate_setting);

mojom::UsbConfigurationInfoPtr BuildUsbConfigurationInfoPtr(
    const uint8_t* data);

mojom::UsbConfigurationInfoPtr BuildUsbConfigurationInfoPtr(
    uint8_t configuration_value,
    bool self_powered,
    bool remote_wakeup,
    uint8_t maximum_power);

void AssignFirstInterfaceNumbers(mojom::UsbConfigurationInfo* config);

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DESCRIPTORS_H_
