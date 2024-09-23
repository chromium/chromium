// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/usb/usb_descriptors.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_device_handle.h"

namespace device {

using mojom::UsbAlternateInterfaceInfoPtr;
using mojom::UsbConfigurationInfoPtr;
using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbDeviceInfoPtr;
using mojom::UsbEndpointInfoPtr;
using mojom::UsbInterfaceInfoPtr;
using mojom::UsbSynchronizationType;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;
using mojom::UsbTransferType;
using mojom::UsbUsageType;

namespace {

using IndexMap = std::map<uint8_t, std::u16string>;
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
  if (desc->num_configurations == desc->device_info->configurations.size()) {
    std::move(callback).Run(std::move(desc));
  } else {
    LOG(ERROR) << "Failed to read all configuration descriptors. Expected "
               << static_cast<int>(desc->num_configurations) << ", got "
               << desc->device_info->configurations.size() << ".";
    std::move(callback).Run(nullptr);
  }
}

void OnReadConfigDescriptor(UsbDeviceDescriptor* desc,
                            base::OnceClosure closure,
                            UsbTransferStatus status,
                            scoped_refptr<base::RefCountedBytes> buffer,
                            size_t length) {
  if (status == UsbTransferStatus::COMPLETED) {
    if (!desc->Parse(base::span(*buffer).first(length))) {
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
                                  base::OnceClosure closure,
                                  UsbTransferStatus status,
                                  scoped_refptr<base::RefCountedBytes> header,
                                  size_t length) {
  if (status == UsbTransferStatus::COMPLETED &&
      length == kConfigurationDescriptorLength) {
    auto data = base::span<const uint8_t>(*header);
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
  if (!desc->Parse(base::span(*buffer).first(length))) {
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
  base::RepeatingClosure closure = base::BarrierClosure(
      num_configurations,
      base::BindOnce(OnDoneReadingConfigDescriptors, device_handle,
                     std::move(desc), std::move(callback)));
  for (uint8_t i = 0; i < num_configurations; ++i) {
    auto header = base::MakeRefCounted<base::RefCountedBytes>(
        kConfigurationDescriptorLength);
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
                           base::OnceClosure callback,
                           const std::u16string& string) {
  it->second = string;
  std::move(callback).Run();
}

void OnReadStringDescriptor(
    base::OnceCallback<void(const std::u16string&)> callback,
    UsbTransferStatus status,
    scoped_refptr<base::RefCountedBytes> buffer,
    size_t length) {
  if (status == UsbTransferStatus::COMPLETED) {
    std::u16string string;
    if (ParseUsbStringDescriptor(base::span(*buffer).first(length), &string)) {
      std::move(callback).Run(std::move(string));
      return;
    }
  }
  std::move(callback).Run(std::u16string());
}

void ReadStringDescriptor(
    scoped_refptr<UsbDeviceHandle> device_handle,
    uint8_t index,
    uint16_t language_id,
    base::OnceCallback<void(const std::u16string&)> callback) {
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
                       const std::u16string& languages) {
  // Default to English unless the device provides a language and then just pick
  // the first one.
  uint16_t language_id = languages.empty() ? 0x0409 : languages[0];

  std::map<uint8_t, IndexMap::iterator> iterator_map;
  for (auto it = index_map->begin(); it != index_map->end(); ++it)
    iterator_map[it->first] = it;

  base::RepeatingClosure barrier = base::BarrierClosure(
      static_cast<int>(iterator_map.size()),
      base::BindOnce(std::move(callback), std::move(index_map)));
  for (const auto& map_entry : iterator_map) {
    ReadStringDescriptor(
        device_handle, map_entry.first, language_id,
        base::BindOnce(&StoreStringDescriptor, map_entry.second, barrier));
  }
}

}  // namespace

CombinedInterfaceInfo::CombinedInterfaceInfo(
    const mojom::UsbInterfaceInfo* interface,
    const mojom::UsbAlternateInterfaceInfo* alternate)
    : interface(interface), alternate(alternate) {}

bool CombinedInterfaceInfo::IsValid() const {
  return interface && alternate;
}

UsbDeviceDescriptor::UsbDeviceDescriptor()
    : device_info(mojom::UsbDeviceInfo::New()) {}

UsbDeviceDescriptor::~UsbDeviceDescriptor() = default;

bool UsbDeviceDescriptor::Parse(base::span<const uint8_t> buffer) {
  mojom::UsbConfigurationInfo* last_config = nullptr;
  mojom::UsbInterfaceInfo* last_interface = nullptr;
  mojom::UsbEndpointInfo* last_endpoint = nullptr;

  for (auto it = buffer.begin(); it != buffer.end();
       /* incremented internally */) {
    const uint8_t* data = &it[0];
    uint8_t length = data[0];
    if (length < 2 || length > std::distance(it, buffer.end()))
      return false;
    it += length;

    switch (data[1] /* bDescriptorType */) {
      case kDeviceDescriptorType:
        if (device_info->configurations.size() > 0 ||
            length < kDeviceDescriptorLength) {
          return false;
        }
        device_info->usb_version_minor = data[2] >> 4 & 0xf;
        device_info->usb_version_subminor = data[2] & 0xf;
        device_info->usb_version_major = data[3];
        device_info->class_code = data[4];
        device_info->subclass_code = data[5];
        device_info->protocol_code = data[6];
        device_info->vendor_id = data[8] | data[9] << 8;
        device_info->product_id = data[10] | data[11] << 8;
        device_info->device_version_minor = data[12] >> 4 & 0xf;
        device_info->device_version_subminor = data[12] & 0xf;
        device_info->device_version_major = data[13];
        i_manufacturer = data[14];
        i_product = data[15];
        i_serial_number = data[16];
        num_configurations = data[17];
        break;
      case kConfigurationDescriptorType:
        if (length < kConfigurationDescriptorLength)
          return false;
        if (last_config) {
          AssignFirstInterfaceNumbers(last_config);
          AggregateInterfacesForConfig(last_config);
        }
        device_info->configurations.push_back(
            BuildUsbConfigurationInfoPtr(data));
        last_config = device_info->configurations.back().get();
        last_interface = nullptr;
        last_endpoint = nullptr;
        break;
      case kInterfaceDescriptorType:
        if (!last_config || length < kInterfaceDescriptorLength)
          return false;
        last_config->interfaces.push_back(BuildUsbInterfaceInfoPtr(data));
        last_interface = last_config->interfaces.back().get();
        last_endpoint = nullptr;
        break;
      case kEndpointDescriptorType:
        if (!last_interface || length < kEndpointDescriptorLength)
          return false;
        last_interface->alternates[0]->endpoints.push_back(
            BuildUsbEndpointInfoPtr(data));
        last_endpoint = last_interface->alternates[0]->endpoints.back().get();
        break;
      default:
        // Append unknown descriptor types to the |extra_data| field of the last
        // descriptor.
        if (last_endpoint) {
          last_endpoint->extra_data.insert(last_endpoint->extra_data.end(),
                                           data, data + length);
        } else if (last_interface) {
          DCHECK_EQ(1u, last_interface->alternates.size());
          last_interface->alternates[0]->extra_data.insert(
              last_interface->alternates[0]->extra_data.end(), data,
              data + length);
        } else if (last_config) {
          last_config->extra_data.insert(last_config->extra_data.end(), data,
                                         data + length);
        }
    }
  }

  if (last_config) {
    AssignFirstInterfaceNumbers(last_config);
    AggregateInterfacesForConfig(last_config);
  }

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

bool ParseUsbStringDescriptor(base::span<const uint8_t> descriptor,
                              std::u16string* output) {
  if (descriptor.size() < 2u) {
    return false;
  }

  auto [header, body] = descriptor.split_at(2u);
  if (header[1u] != kStringDescriptorType) {
    return false;
  }

  // Let the device return a buffer larger than the actual string but prefer the
  // length reported inside the descriptor.
  const size_t length_bytes =
      std::min<size_t>(header[0u], header.size() + body.size());
  // The header's size is included in the length. If not, it's malformed.
  if (length_bytes < header.size()) {
    return false;
  }
  const size_t body_bytes = length_bytes - header.size();
  const size_t body_chars = body_bytes / sizeof(char16_t);

  // The string is returned by the device in UTF-16LE.
  output->resize(body_chars);
  base::as_writable_byte_span(*output).copy_from(
      body.first(body_chars * sizeof(char16_t)));
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

UsbEndpointInfoPtr BuildUsbEndpointInfoPtr(const uint8_t* data) {
  DCHECK_GE(data[0], kEndpointDescriptorLength);
  DCHECK_EQ(data[1], kEndpointDescriptorType);

  return BuildUsbEndpointInfoPtr(
      data[2] /* bEndpointAddress */, data[3] /* bmAttributes */,
      data[4] + (data[5] << 8) /* wMaxPacketSize */, data[6] /* bInterval */);
}

UsbEndpointInfoPtr BuildUsbEndpointInfoPtr(uint8_t address,
                                           uint8_t attributes,
                                           uint16_t maximum_packet_size,
                                           uint8_t polling_interval) {
  UsbEndpointInfoPtr endpoint = mojom::UsbEndpointInfo::New();
  endpoint->endpoint_number = ConvertEndpointAddressToNumber(address);

  // These fields are defined in Table 9-24 of the USB 3.1 Specification.
  switch (address & 0x80) {
    case 0x00:
      endpoint->direction = UsbTransferDirection::OUTBOUND;
      break;
    case 0x80:
      endpoint->direction = UsbTransferDirection::INBOUND;
      break;
  }

  switch (attributes & 0x03) {
    case 0x00:
      endpoint->type = UsbTransferType::CONTROL;
      break;
    case 0x01:
      endpoint->type = UsbTransferType::ISOCHRONOUS;
      break;
    case 0x02:
      endpoint->type = UsbTransferType::BULK;
      break;
    case 0x03:
      endpoint->type = UsbTransferType::INTERRUPT;
      break;
  }

  switch (attributes & 0x0F) {
    // Isochronous endpoints only.
    case 0x05:
      endpoint->synchronization_type = UsbSynchronizationType::ASYNCHRONOUS;
      break;
    case 0x09:
      endpoint->synchronization_type = UsbSynchronizationType::ADAPTIVE;
      break;
    case 0x0D:
      endpoint->synchronization_type = UsbSynchronizationType::SYNCHRONOUS;
      break;
    default:
      endpoint->synchronization_type = UsbSynchronizationType::NONE;
  }

  switch (attributes & 0x33) {
    // Isochronous endpoint usages.
    case 0x01:
      endpoint->usage_type = UsbUsageType::DATA;
      break;
    case 0x11:
      endpoint->usage_type = UsbUsageType::FEEDBACK;
      break;
    case 0x21:
      endpoint->usage_type = UsbUsageType::EXPLICIT_FEEDBACK;
      break;
    // Interrupt endpoint usages.
    case 0x03:
      endpoint->usage_type = UsbUsageType::PERIODIC;
      break;
    case 0x13:
      endpoint->usage_type = UsbUsageType::NOTIFICATION;
      break;
    default:
      endpoint->usage_type = UsbUsageType::RESERVED;
  }

  endpoint->packet_size = static_cast<uint32_t>(maximum_packet_size);
  endpoint->polling_interval = polling_interval;

  return endpoint;
}

UsbInterfaceInfoPtr BuildUsbInterfaceInfoPtr(const uint8_t* data) {
  DCHECK_GE(data[0], kInterfaceDescriptorLength);
  DCHECK_EQ(data[1], kInterfaceDescriptorType);
  return BuildUsbInterfaceInfoPtr(
      data[2] /* bInterfaceNumber */, data[3] /* bAlternateSetting */,
      data[5] /* bInterfaceClass */, data[6] /* bInterfaceSubClass */,
      data[7] /* bInterfaceProtocol */);
}

UsbInterfaceInfoPtr BuildUsbInterfaceInfoPtr(uint8_t interface_number,
                                             uint8_t alternate_setting,
                                             uint8_t interface_class,
                                             uint8_t interface_subclass,
                                             uint8_t interface_protocol) {
  UsbInterfaceInfoPtr interface_info = mojom::UsbInterfaceInfo::New();
  interface_info->interface_number = interface_number;
  interface_info->first_interface = interface_number;

  UsbAlternateInterfaceInfoPtr alternate =
      mojom::UsbAlternateInterfaceInfo::New();
  alternate->alternate_setting = alternate_setting;
  alternate->class_code = interface_class;
  alternate->subclass_code = interface_subclass;
  alternate->protocol_code = interface_protocol;

  interface_info->alternates.push_back(std::move(alternate));

  return interface_info;
}

// Aggregate each alternate setting into an InterfaceInfo corresponding to its
// interface number.
void AggregateInterfacesForConfig(mojom::UsbConfigurationInfo* config) {
  std::map<uint8_t, device::mojom::UsbInterfaceInfo*> interface_map;
  std::vector<device::mojom::UsbInterfaceInfoPtr> interfaces =
      std::move(config->interfaces);
  config->interfaces.clear();

  for (size_t i = 0; i < interfaces.size(); ++i) {
    // As interfaces should appear in order, this map could be unnecessary,
    // but this errs on the side of caution.
    auto iter = interface_map.find(interfaces[i]->interface_number);
    if (iter == interface_map.end()) {
      // This is the first time we're seeing an alternate with this interface
      // number, so add a new InterfaceInfo to the array and map the number.
      config->interfaces.push_back(std::move(interfaces[i]));
      interface_map.insert(
          std::make_pair(config->interfaces.back()->interface_number,
                         config->interfaces.back().get()));
    } else {
      DCHECK_EQ(1u, interfaces[i]->alternates.size());
      iter->second->alternates.push_back(
          std::move(interfaces[i]->alternates[0]));
    }
  }
}

CombinedInterfaceInfo FindInterfaceInfoFromConfig(
    const mojom::UsbConfigurationInfo* config,
    uint8_t interface_number,
    uint8_t alternate_setting) {
  CombinedInterfaceInfo interface_info;
  if (!config) {
    return interface_info;
  }

  for (const auto& iface : config->interfaces) {
    if (iface->interface_number == interface_number) {
      interface_info.interface = iface.get();

      for (const auto& alternate : iface->alternates) {
        if (alternate->alternate_setting == alternate_setting) {
          interface_info.alternate = alternate.get();
          break;
        }
      }
    }
  }
  return interface_info;
}

UsbConfigurationInfoPtr BuildUsbConfigurationInfoPtr(const uint8_t* data) {
  DCHECK_GE(data[0], kConfigurationDescriptorLength);
  DCHECK_EQ(data[1], kConfigurationDescriptorType);
  return BuildUsbConfigurationInfoPtr(data[5] /* bConfigurationValue */,
                                      (data[7] & 0x02) != 0 /* bmAttributes */,
                                      (data[7] & 0x04) != 0 /* bmAttributes */,
                                      data[8] /* bMaxPower */);
}

UsbConfigurationInfoPtr BuildUsbConfigurationInfoPtr(
    uint8_t configuration_value,
    bool self_powered,
    bool remote_wakeup,
    uint8_t maximum_power) {
  UsbConfigurationInfoPtr config = mojom::UsbConfigurationInfo::New();
  config->configuration_value = configuration_value;
  config->self_powered = self_powered;
  config->remote_wakeup = remote_wakeup;
  config->maximum_power = maximum_power;
  return config;
}

void AssignFirstInterfaceNumbers(mojom::UsbConfigurationInfo* config) {
  std::vector<UsbInterfaceAssociationDescriptor> functions;
  ParseInterfaceAssociationDescriptors(config->extra_data, &functions);
  for (const auto& interface : config->interfaces) {
    DCHECK_EQ(1u, interface->alternates.size());
    ParseInterfaceAssociationDescriptors(interface->alternates[0]->extra_data,
                                         &functions);
    for (auto& endpoint : interface->alternates[0]->endpoints)
      ParseInterfaceAssociationDescriptors(endpoint->extra_data, &functions);
  }

  // libusb has collected interface association descriptors in the |extra_data|
  // fields of other descriptor types. This may have disturbed their order
  // but sorting by the bFirstInterface should fix it.
  std::sort(functions.begin(), functions.end());

  uint8_t remaining_interfaces = 0;
  auto function_it = functions.cbegin();
  auto interface_it = config->interfaces.begin();
  while (interface_it != config->interfaces.end()) {
    if (remaining_interfaces > 0) {
      // Continuation of a previous function. Tag all alternate interfaces
      // (which are guaranteed to be contiguous).
      for (uint8_t interface_number = (*interface_it)->interface_number;
           interface_it != config->interfaces.end() &&
           (*interface_it)->interface_number == interface_number;
           ++interface_it) {
        (*interface_it)->first_interface = function_it->first_interface;
      }
      if (--remaining_interfaces == 0)
        ++function_it;
    } else if (function_it != functions.end() &&
               (*interface_it)->interface_number ==
                   function_it->first_interface) {
      // Start of a new function.
      (*interface_it)->first_interface = function_it->first_interface;
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

}  // namespace device
