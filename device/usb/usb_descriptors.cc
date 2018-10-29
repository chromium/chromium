// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_descriptors.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "device/usb/usb_device_handle.h"

namespace device {

namespace {

using IndexMap = std::map<uint8_t, base::string16>;
using IndexMapPtr = std::unique_ptr<IndexMap>;

// Standard USB requests and descriptor types:
const uint8_t kGetDescriptorRequest = 0x06;

const uint8_t kDeviceDescriptorType = 0x01;
const uint8_t kConfigurationDescriptorType = 0x02;
const uint8_t kStringDescriptorType = 0x03;
const uint8_t kInterfaceDescriptorType = 0x04;
const uint8_t kEndpointDescriptorType = 0x05;
const uint8_t kInterfaceAssociationDescriptorType = 11;

const uint8_t kDeviceDescriptorLength = 18;
const uint8_t kConfigurationDescriptorLength = 9;
const uint8_t kInterfaceDescriptorLength = 9;
const uint8_t kEndpointDescriptorLength = 7;
const uint8_t kInterfaceAssociationDescriptorLength = 8;

const int kControlTransferTimeoutMs = 2000;  // 2 seconds

struct UsbInterfaceAssociationDescriptor {
  UsbInterfaceAssociationDescriptor(uint8_t first_interface,
                                    uint8_t interface_count)
      : first_interface(first_interface), interface_count(interface_count) {}

  bool operator<(const UsbInterfaceAssociationDescriptor& other) const {
    return first_interface < other.first_interface;
  }

  uint8_t first_interface;
  uint8_t interface_count;
};

void ParseInterfaceAssociationDescriptors(
    const std::vector<uint8_t>& buffer,
    std::vector<UsbInterfaceAssociationDescriptor>* functions) {
  auto it = buffer.begin();

  while (it != buffer.end()) {
    // All descriptors must be at least 2 byte which means the length and type
    // are safe to read.
    if (std::distance(it, buffer.end()) < 2)
      return;
    uint8_t length = it[0];
    if (length > std::distance(it, buffer.end()))
      return;
    if (it[1] == kInterfaceAssociationDescriptorType &&
        length == kInterfaceAssociationDescriptorLength) {
      functions->push_back(UsbInterfaceAssociationDescriptor(it[2], it[3]));
    }
    std::advance(it, length);
  }
}

void OnDoneReadingConfigDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    std::unique_ptr<UsbDeviceDescriptor> desc,
    base::OnceCallback<void(std::unique_ptr<UsbDeviceDescriptor>)> callback) {
  if (desc->num_configurations == desc->configurations.size()) {
    std::move(callback).Run(std::move(desc));
  } else {
    LOG(ERROR) << "Failed to read all configuration descriptors. Expected "
               << static_cast<int>(desc->num_configurations) << ", got "
               << desc->configurations.size() << ".";
    std::move(callback).Run(nullptr);
  }
}

void OnReadConfigDescriptor(UsbDeviceDescriptor* desc,
                            base::Closure closure,
                            UsbTransferStatus status,
                            scoped_refptr<base::RefCountedBytes> buffer,
                            size_t length) {
  if (status == UsbTransferStatus::COMPLETED) {
    if (!desc->Parse(
            std::vector<uint8_t>(buffer->front(), buffer->front() + length))) {
      LOG(ERROR) << "Failed to parse configuration descriptor.";
    }
  } else {
    LOG(ERROR) << "Failed to read configuration descriptor.";
  }
  std::move(closure).Run();
}

void OnReadConfigDescriptorHeader(scoped_refptr<UsbDeviceHandle> device_handle,
                                  UsbDeviceDescriptor* desc,
                                  uint8_t index,
                                  base::Closure closure,
                                  UsbTransferStatus status,
                                  scoped_refptr<base::RefCountedBytes> header,
                                  size_t length) {
  if (status == UsbTransferStatus::COMPLETED && length == 4) {
    const uint8_t* data = header->front();
    uint16_t total_length = data[2] | data[3] << 8;
    auto buffer = base::MakeRefCounted<base::RefCountedBytes>(total_length);
    device_handle->ControlTransfer(
        UsbTransferDirection::INBOUND, UsbControlTransferType::STANDARD,
        UsbControlTransferRecipient::DEVICE, kGetDescriptorRequest,
        kConfigurationDescriptorType << 8 | index, 0, buffer,
        kControlTransferTimeoutMs,
        base::BindOnce(&OnReadConfigDescriptor, desc, std::move(closure)));
  } else {
    LOG(ERROR) << "Failed to read length for configuration "
               << static_cast<int>(index) << ".";
    std::move(closure).Run();
  }
}

void OnReadDeviceDescriptor(
    scoped_refptr<UsbDeviceHandle> device_handle,
    base::OnceCallback<void(std::unique_ptr<UsbDeviceDescriptor>)> callback,
    UsbTransferStatus status,
    scoped_refptr<base::RefCountedBytes> buffer,
    size_t length) {
  if (status != UsbTransferStatus::COMPLETED) {
    LOG(ERROR) << "Failed to read device descriptor.";
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<UsbDeviceDescriptor> desc(new UsbDeviceDescriptor());
  if (!desc->Parse(
          std::vector<uint8_t>(buffer->front(), buffer->front() + length))) {
    LOG(ERROR) << "Device descriptor parsing error.";
    std::move(callback).Run(nullptr);
    return;
  }

  if (desc->num_configurations == 0) {
    std::move(callback).Run(std::move(desc));
    return;
  }

  uint8_t num_configurations = desc->num_configurations;
  UsbDeviceDescriptor* desc_ptr = desc.get();
  base::Closure closure = base::BarrierClosure(
      num_configurations,
      base::BindOnce(OnDoneReadingConfigDescriptors, device_handle,
                     std::move(desc), std::move(callback)));
  for (uint8_t i = 0; i < num_configurations; ++i) {
    auto header = base::MakeRefCounted<base::RefCountedBytes>(4);
    device_handle->ControlTransfer(
        UsbTransferDirection::INBOUND, UsbControlTransferType::STANDARD,
        UsbControlTransferRecipient::DEVICE, kGetDescriptorRequest,
        kConfigurationDescriptorType << 8 | i, 0, header,
        kControlTransferTimeoutMs,
        base::BindOnce(&OnReadConfigDescriptorHeader, device_handle, desc_ptr,
                       i, closure));
  }
}

void StoreStringDescriptor(IndexMap::iterator it,
                           base::Closure callback,
                           const base::string16& string) {
  it->second = string;
  std::move(callback).Run();
}

void OnReadStringDescriptor(
    base::OnceCallback<void(const base::string16&)> callback,
    UsbTransferStatus status,
    scoped_refptr<base::RefCountedBytes> buffer,
    size_t length) {
  base::string16 string;
  if (status == UsbTransferStatus::COMPLETED &&
      ParseUsbStringDescriptor(
          std::vector<uint8_t>(buffer->front(), buffer->front() + length),
          &string)) {
    std::move(callback).Run(string);
  } else {
    std::move(callback).Run(base::string16());
  }
}

void ReadStringDescriptor(
    scoped_refptr<UsbDeviceHandle> device_handle,
    uint8_t index,
    uint16_t language_id,
    base::OnceCallback<void(const base::string16&)> callback) {
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(255);
  device_handle->ControlTransfer(
      UsbTransferDirection::INBOUND, UsbControlTransferType::STANDARD,
      UsbControlTransferRecipient::DEVICE, kGetDescriptorRequest,
      kStringDescriptorType << 8 | index, language_id, buffer,
      kControlTransferTimeoutMs,
      base::BindOnce(&OnReadStringDescriptor, std::move(callback)));
}

void OnReadLanguageIds(scoped_refptr<UsbDeviceHandle> device_handle,
                       IndexMapPtr index_map,
                       base::OnceCallback<void(IndexMapPtr)> callback,
                       const base::string16& languages) {
  // Default to English unless the device provides a language and then just pick
  // the first one.
  uint16_t language_id = languages.empty() ? 0x0409 : languages[0];

  std::map<uint8_t, IndexMap::iterator> iterator_map;
  for (auto it = index_map->begin(); it != index_map->end(); ++it)
    iterator_map[it->first] = it;

  base::Closure barrier = base::BarrierClosure(
      static_cast<int>(iterator_map.size()),
      base::BindOnce(std::move(callback), std::move(index_map)));
  for (const auto& map_entry : iterator_map) {
    ReadStringDescriptor(
        device_handle, map_entry.first, language_id,
        base::BindOnce(&StoreStringDescriptor, map_entry.second, barrier));
  }
}

}  // namespace

UsbEndpointDescriptor::UsbEndpointDescriptor(const uint8_t* data)
    : UsbEndpointDescriptor(data[2] /* bEndpointAddress */,
                            data[3] /* bmAttributes */,
                            data[4] + (data[5] << 8) /* wMaxPacketSize */,
                            data[6] /* bInterval */) {
  DCHECK_GE(data[0], kEndpointDescriptorLength);
  DCHECK_EQ(data[1], kEndpointDescriptorType);
}

UsbEndpointDescriptor::UsbEndpointDescriptor(uint8_t address,
                                             uint8_t attributes,
                                             uint16_t maximum_packet_size,
                                             uint8_t polling_interval)
    : address(address),
      maximum_packet_size(maximum_packet_size),
      polling_interval(polling_interval) {
  // These fields are defined in Table 9-24 of the USB 3.1 Specification.
  switch (address & 0x80) {
    case 0x00:
      direction = UsbTransferDirection::OUTBOUND;
      break;
    case 0x80:
      direction = UsbTransferDirection::INBOUND;
      break;
  }
  switch (attributes & 0x03) {
    case 0x00:
      transfer_type = UsbTransferType::CONTROL;
      break;
    case 0x01:
      transfer_type = UsbTransferType::ISOCHRONOUS;
      break;
    case 0x02:
      transfer_type = UsbTransferType::BULK;
      break;
    case 0x03:
      transfer_type = UsbTransferType::INTERRUPT;
      break;
  }
  switch (attributes & 0x0F) {
    // Isochronous endpoints only.
    case 0x05:
      synchronization_type = USB_SYNCHRONIZATION_ASYNCHRONOUS;
      break;
    case 0x09:
      synchronization_type = USB_SYNCHRONIZATION_ADAPTIVE;
      break;
    case 0x0D:
      synchronization_type = USB_SYNCHRONIZATION_SYNCHRONOUS;
      break;
    default:
      synchronization_type = USB_SYNCHRONIZATION_NONE;
  }
  switch (attributes & 0x33) {
    // Isochronous endpoint usages.
    case 0x01:
      usage_type = USB_USAGE_DATA;
      break;
    case 0x11:
      usage_type = USB_USAGE_FEEDBACK;
      break;
    case 0x21:
      usage_type = USB_USAGE_EXPLICIT_FEEDBACK;
      break;
    // Interrupt endpoint usages.
    case 0x03:
      usage_type = USB_USAGE_PERIODIC;
      break;
    case 0x13:
      usage_type = USB_USAGE_NOTIFICATION;
      break;
    default:
      usage_type = USB_USAGE_RESERVED;
  }
}

UsbEndpointDescriptor::UsbEndpointDescriptor(
    const UsbEndpointDescriptor& other) = default;

UsbEndpointDescriptor::~UsbEndpointDescriptor() = default;

UsbInterfaceDescriptor::UsbInterfaceDescriptor(const uint8_t* data)
    : UsbInterfaceDescriptor(data[2] /* bInterfaceNumber */,
                             data[3] /* bAlternateSetting */,
                             data[5] /* bInterfaceClass */,
                             data[6] /* bInterfaceSubClass */,
                             data[7] /* bInterfaceProtocol */) {
  DCHECK_GE(data[0], kInterfaceDescriptorLength);
  DCHECK_EQ(data[1], kInterfaceDescriptorType);
}

UsbInterfaceDescriptor::UsbInterfaceDescriptor(uint8_t interface_number,
                                               uint8_t alternate_setting,
                                               uint8_t interface_class,
                                               uint8_t interface_subclass,
                                               uint8_t interface_protocol)
    : interface_number(interface_number),
      alternate_setting(alternate_setting),
      interface_class(interface_class),
      interface_subclass(interface_subclass),
      interface_protocol(interface_protocol),
      first_interface(interface_number) {}

UsbInterfaceDescriptor::UsbInterfaceDescriptor(
    const UsbInterfaceDescriptor& other) = default;

UsbInterfaceDescriptor::~UsbInterfaceDescriptor() = default;

UsbConfigDescriptor::UsbConfigDescriptor(const uint8_t* data)
    : UsbConfigDescriptor(data[5] /* bConfigurationValue */,
                          (data[7] & 0x02) != 0 /* bmAttributes */,
                          (data[7] & 0x04) != 0 /* bmAttributes */,
                          data[8] /* bMaxPower */) {
  DCHECK_GE(data[0], kConfigurationDescriptorLength);
  DCHECK_EQ(data[1], kConfigurationDescriptorType);
}

UsbConfigDescriptor::UsbConfigDescriptor(uint8_t configuration_value,
                                         bool self_powered,
                                         bool remote_wakeup,
                                         uint8_t maximum_power)
    : configuration_value(configuration_value),
      self_powered(self_powered),
      remote_wakeup(remote_wakeup),
      maximum_power(maximum_power) {}

UsbConfigDescriptor::UsbConfigDescriptor(const UsbConfigDescriptor& other) =
    default;

UsbConfigDescriptor::~UsbConfigDescriptor() = default;

void UsbConfigDescriptor::AssignFirstInterfaceNumbers() {
  std::vector<UsbInterfaceAssociationDescriptor> functions;
  ParseInterfaceAssociationDescriptors(extra_data, &functions);
  for (const auto& interface : interfaces) {
    ParseInterfaceAssociationDescriptors(interface.extra_data, &functions);
    for (const auto& endpoint : interface.endpoints)
      ParseInterfaceAssociationDescriptors(endpoint.extra_data, &functions);
  }

  // libusb has collected interface association descriptors in the |extra_data|
  // fields of other descriptor types. This may have disturbed their order
  // but sorting by the bFirstInterface should fix it.
  std::sort(functions.begin(), functions.end());

  uint8_t remaining_interfaces = 0;
  auto function_it = functions.cbegin();
  auto interface_it = interfaces.begin();
  while (interface_it != interfaces.end()) {
    if (remaining_interfaces > 0) {
      // Continuation of a previous function. Tag all alternate interfaces
      // (which are guaranteed to be contiguous).
      for (uint8_t interface_number = interface_it->interface_number;
           interface_it != interfaces.end() &&
           interface_it->interface_number == interface_number;
           ++interface_it) {
        interface_it->first_interface = function_it->first_interface;
      }
      if (--remaining_interfaces == 0)
        ++function_it;
    } else if (function_it != functions.end() &&
               interface_it->interface_number == function_it->first_interface) {
      // Start of a new function.
      interface_it->first_interface = function_it->first_interface;
      if (function_it->interface_count > 1)
        remaining_interfaces = function_it->interface_count - 1;
      else
        ++function_it;
      ++interface_it;
    } else {
      // Unassociated interfaces already have |first_interface| set to
      // |interface_number|.
      ++interface_it;
    }
  }
}

UsbDeviceDescriptor::UsbDeviceDescriptor() = default;

UsbDeviceDescriptor::UsbDeviceDescriptor(const UsbDeviceDescriptor& other) =
    default;

UsbDeviceDescriptor::~UsbDeviceDescriptor() = default;

bool UsbDeviceDescriptor::Parse(const std::vector<uint8_t>& buffer) {
  UsbConfigDescriptor* last_config = nullptr;
  UsbInterfaceDescriptor* last_interface = nullptr;
  UsbEndpointDescriptor* last_endpoint = nullptr;

  for (auto it = buffer.begin(); it != buffer.end();
       /* incremented internally */) {
    const uint8_t* data = &it[0];
    uint8_t length = data[0];
    if (length < 2 || length > std::distance(it, buffer.end()))
      return false;
    it += length;

    switch (data[1] /* bDescriptorType */) {
      case kDeviceDescriptorType:
        if (configurations.size() > 0 || length < kDeviceDescriptorLength)
          return false;
        usb_version = data[2] | data[3] << 8;
        device_class = data[4];
        device_subclass = data[5];
        device_protocol = data[6];
        vendor_id = data[8] | data[9] << 8;
        product_id = data[10] | data[11] << 8;
        device_version = data[12] | data[13] << 8;
        i_manufacturer = data[14];
        i_product = data[15];
        i_serial_number = data[16];
        num_configurations = data[17];
        break;
      case kConfigurationDescriptorType:
        if (length < kConfigurationDescriptorLength)
          return false;
        if (last_config)
          last_config->AssignFirstInterfaceNumbers();
        configurations.emplace_back(data);
        last_config = &configurations.back();
        last_interface = nullptr;
        last_endpoint = nullptr;
        break;
      case kInterfaceDescriptorType:
        if (!last_config || length < kInterfaceDescriptorLength)
          return false;
        last_config->interfaces.emplace_back(data);
        last_interface = &last_config->interfaces.back();
        last_endpoint = nullptr;
        break;
      case kEndpointDescriptorType:
        if (!last_interface || length < kEndpointDescriptorLength)
          return false;
        last_interface->endpoints.emplace_back(data);
        last_endpoint = &last_interface->endpoints.back();
        break;
      default:
        // Append unknown descriptor types to the |extra_data| field of the last
        // descriptor.
        if (last_endpoint) {
          last_endpoint->extra_data.insert(last_endpoint->extra_data.end(),
                                           data, data + length);
        } else if (last_interface) {
          last_interface->extra_data.insert(last_interface->extra_data.end(),
                                            data, data + length);
        } else if (last_config) {
          last_config->extra_data.insert(last_config->extra_data.end(), data,
                                         data + length);
        }
    }
  }

  if (last_config)
    last_config->AssignFirstInterfaceNumbers();

  return true;
}

void ReadUsbDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    base::OnceCallback<void(std::unique_ptr<UsbDeviceDescriptor>)> callback) {
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(kDeviceDescriptorLength);
  device_handle->ControlTransfer(
      UsbTransferDirection::INBOUND, UsbControlTransferType::STANDARD,
      UsbControlTransferRecipient::DEVICE, kGetDescriptorRequest,
      kDeviceDescriptorType << 8, 0, buffer, kControlTransferTimeoutMs,
      base::BindOnce(&OnReadDeviceDescriptor, device_handle,
                     std::move(callback)));
}

bool ParseUsbStringDescriptor(const std::vector<uint8_t>& descriptor,
                              base::string16* output) {
  if (descriptor.size() < 2 || descriptor[1] != kStringDescriptorType)
    return false;

  // Let the device return a buffer larger than the actual string but prefer the
  // length reported inside the descriptor.
  size_t length = descriptor[0];
  length = std::min(length, descriptor.size());
  if (length < 2)
    return false;

  // The string is returned by the device in UTF-16LE.
  *output = base::string16(
      reinterpret_cast<const base::char16*>(descriptor.data() + 2),
      length / 2 - 1);
  return true;
}

// For each key in |index_map| this function reads that string descriptor from
// |device_handle| and updates the value in in |index_map|.
void ReadUsbStringDescriptors(scoped_refptr<UsbDeviceHandle> device_handle,
                              IndexMapPtr index_map,
                              base::OnceCallback<void(IndexMapPtr)> callback) {
  if (index_map->empty()) {
    std::move(callback).Run(std::move(index_map));
    return;
  }

  ReadStringDescriptor(
      device_handle, 0, 0,
      base::BindOnce(&OnReadLanguageIds, device_handle, std::move(index_map),
                     std::move(callback)));
}

}  // namespace device
