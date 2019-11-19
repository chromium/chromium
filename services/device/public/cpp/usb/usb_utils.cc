// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/usb/usb_utils.h"

#include <utility>

#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

namespace device {

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

uint8_t ConvertEndpointAddressToNumber(uint8_t address) {
  return address & 0x0F;
}

uint8_t ConvertEndpointNumberToAddress(uint8_t endpoint_number,
                                       mojom::UsbTransferDirection direction) {
  return endpoint_number |
         (direction == mojom::UsbTransferDirection::INBOUND ? 0x80 : 0x00);
}

uint8_t ConvertEndpointNumberToAddress(
    const mojom::UsbEndpointInfo& mojo_endpoint) {
  return ConvertEndpointNumberToAddress(mojo_endpoint.endpoint_number,
                                        mojo_endpoint.direction);
}

uint16_t GetUsbVersion(const mojom::UsbDeviceInfo& device_info) {
  return device_info.usb_version_major << 8 |
         device_info.usb_version_minor << 4 | device_info.usb_version_subminor;
}

uint16_t GetDeviceVersion(const mojom::UsbDeviceInfo& device_info) {
  return device_info.device_version_major << 8 |
         device_info.device_version_minor << 4 |
         device_info.device_version_subminor;
}

}  // namespace device
