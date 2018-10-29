// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/public/cpp/usb_utils.h"

#include <utility>

#include "device/usb/usb_device.h"

namespace device {

bool UsbDeviceFilterMatches(const mojom::UsbDeviceFilter& filter,
                            const UsbDevice& device) {
  if (filter.has_vendor_id) {
    if (device.vendor_id() != filter.vendor_id)
      return false;

    if (filter.has_product_id && device.product_id() != filter.product_id)
      return false;
  }

  if (filter.serial_number && device.serial_number() != *filter.serial_number)
    return false;

  if (filter.has_class_code) {
    for (const UsbConfigDescriptor& config : device.configurations()) {
      for (const UsbInterfaceDescriptor& iface : config.interfaces) {
        if (iface.interface_class == filter.class_code &&
            (!filter.has_subclass_code ||
             (iface.interface_subclass == filter.subclass_code &&
              (!filter.has_protocol_code ||
               iface.interface_protocol == filter.protocol_code)))) {
          return true;
        }
      }
    }

    return false;
  }

  return true;
}

bool UsbDeviceFilterMatches(const mojom::UsbDeviceFilter& filter,
                            const mojom::UsbDeviceInfo& device_info) {
  if (filter.has_vendor_id) {
    if (device_info.vendor_id != filter.vendor_id)
      return false;

    if (filter.has_product_id && device_info.product_id != filter.product_id)
      return false;
  }

  if (filter.serial_number &&
      device_info.serial_number != *filter.serial_number) {
    return false;
  }

  if (filter.has_class_code) {
    for (auto& config : device_info.configurations) {
      for (auto& iface : config->interfaces) {
        for (auto& alternate_info : iface->alternates) {
          if (alternate_info->class_code == filter.class_code &&
              (!filter.has_subclass_code ||
               (alternate_info->subclass_code == filter.subclass_code &&
                (!filter.has_protocol_code ||
                 alternate_info->protocol_code == filter.protocol_code)))) {
            return true;
          }
        }
      }
    }

    return false;
  }

  return true;
}

bool UsbDeviceFilterMatchesAny(
    const std::vector<mojom::UsbDeviceFilterPtr>& filters,
    const UsbDevice& device) {
  if (filters.empty())
    return true;

  for (const auto& filter : filters) {
    if (UsbDeviceFilterMatches(*filter, device))
      return true;
  }
  return false;
}

bool UsbDeviceFilterMatchesAny(
    const std::vector<mojom::UsbDeviceFilterPtr>& filters,
    const mojom::UsbDeviceInfo& device_info) {
  if (filters.empty())
    return true;

  for (const auto& filter : filters) {
    if (UsbDeviceFilterMatches(*filter, device_info))
      return true;
  }
  return false;
}

std::vector<mojom::UsbIsochronousPacketPtr> BuildIsochronousPacketArray(
    const std::vector<uint32_t>& packet_lengths,
    mojom::UsbTransferStatus status) {
  std::vector<mojom::UsbIsochronousPacketPtr> packets;
  packets.reserve(packet_lengths.size());
  for (uint32_t packet_length : packet_lengths) {
    auto packet = mojom::UsbIsochronousPacket::New();
    packet->length = packet_length;
    packet->status = status;
    packets.push_back(std::move(packet));
  }
  return packets;
}

}  // namespace device
