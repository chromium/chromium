// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_mac.h"

#include <IOKit/IOCFBundle.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOTypes.h>
#include <IOKit/usb/IOUSBLib.h>
#include <MacTypes.h>

#include <memory>
#include <numeric>
#include <utility>

#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_device_mac.h"

namespace device {

struct Transfer {
  UsbDeviceHandleMac::TransferCallback generic_callback;
  scoped_refptr<UsbDeviceHandleMac> handle;
  scoped_refptr<base::RefCountedBytes> buffer;
  std::vector<uint32_t> packet_lengths;
  std::vector<IOUSBIsocFrame> frame_list;
  mojom::UsbTransferType type;
  UsbDeviceHandleMac::IsochronousTransferCallback isochronous_callback;
};

namespace {

// This is the bit 7 of the request type.
enum class EndpointDirection : uint8_t { kIn = 0x80, kOut = 0x00 };

// These are bits 5 and 6 of the request type.
enum class RequestType : uint8_t {
  kStandard = 0x00,
  kClass = 0x20,
  kVendor = 0x40,
  kReserved = 0x60
};

// These are bits 0 and 1 of the request type.
enum class RequestRecipient : uint8_t {
  kDevice = 0x00,
  kInterface = 0x01,
  kEndpoint = 0x02,
  kOther = 0x03,
};

mojom::UsbTransferStatus ConvertTransferStatus(IOReturn status) {
  switch (status) {
    // kIOReturnUnderrun can be ignored because the lower-than-expected transfer
    // size is reported alongside the COMPLETED status.
    case kIOReturnUnderrun:
    case kIOReturnSuccess:
      return mojom::UsbTransferStatus::COMPLETED;
    case kIOUSBTransactionTimeout:
      return mojom::UsbTransferStatus::TIMEOUT;
    case kIOUSBPipeStalled:
      return mojom::UsbTransferStatus::STALLED;
    case kIOReturnOverrun:
      return mojom::UsbTransferStatus::BABBLE;
    case kIOReturnAborted:
      return mojom::UsbTransferStatus::CANCELLED;
    default:
      return mojom::UsbTransferStatus::TRANSFER_ERROR;
  }
}

uint8_t ConvertTransferDirection(mojom::UsbTransferDirection direction) {
  switch (direction) {
    case mojom::UsbTransferDirection::INBOUND:
      return static_cast<uint8_t>(EndpointDirection::kIn);
    case mojom::UsbTransferDirection::OUTBOUND:
      return static_cast<uint8_t>(EndpointDirection::kOut);
  }
  NOTREACHED();
  return 0;
}

uint8_t CreateRequestType(mojom::UsbTransferDirection direction,
                          mojom::UsbControlTransferType request_type,
                          mojom::UsbControlTransferRecipient recipient) {
  uint8_t result = ConvertTransferDirection(direction);

  switch (request_type) {
    case mojom::UsbControlTransferType::STANDARD:
      result |= static_cast<uint8_t>(RequestType::kStandard);
      break;
    case mojom::UsbControlTransferType::CLASS:
      result |= static_cast<uint8_t>(RequestType::kClass);
      break;
    case mojom::UsbControlTransferType::VENDOR:
      result |= static_cast<uint8_t>(RequestType::kVendor);
      break;
    case mojom::UsbControlTransferType::RESERVED:
      result |= static_cast<uint8_t>(RequestType::kReserved);
      break;
  }

  switch (recipient) {
    case mojom::UsbControlTransferRecipient::DEVICE:
      result |= static_cast<uint8_t>(RequestRecipient::kDevice);
      break;
    case mojom::UsbControlTransferRecipient::INTERFACE:
      result |= static_cast<uint8_t>(RequestRecipient::kInterface);
      break;
    case mojom::UsbControlTransferRecipient::ENDPOINT:
      result |= static_cast<uint8_t>(RequestRecipient::kEndpoint);
      break;
    case mojom::UsbControlTransferRecipient::OTHER:
      result |= static_cast<uint8_t>(RequestRecipient::kOther);
      break;
  }

  return result;
}

}  // namespace

UsbDeviceHandleMac::UsbDeviceHandleMac(
    scoped_refptr<UsbDeviceMac> device,
    ScopedIOUSBDeviceInterface device_interface)
    : device_interface_(std::move(device_interface)),
      device_(std::move(device)) {}

scoped_refptr<UsbDevice> UsbDeviceHandleMac::GetDevice() const {
  return device_;
}

void UsbDeviceHandleMac::Close() {
  if (!device_)
    return;

  if (device_source_) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), device_source_.get(),
                          kCFRunLoopDefaultMode);
  }

  for (const auto& source : sources_) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source.second.get(),
                          kCFRunLoopDefaultMode);
  }

  IOReturn kr = (*device_interface_)->USBDeviceClose(device_interface_);
  if (kr != kIOReturnSuccess) {
    USB_LOG(DEBUG) << "Failed to close device: " << std::hex << kr;
  }

  Clear();
  device_->HandleClosed(this);
  device_ = nullptr;
}

void UsbDeviceHandleMac::SetConfiguration(int configuration_value,
                                          ResultCallback callback) {
  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  if (!base::IsValueInRangeForNumericType<uint8_t>(configuration_value)) {
    std::move(callback).Run(false);
    return;
  }

  Clear();

  IOReturn kr =
      (*device_interface_)
          ->SetConfiguration(device_interface_,
                             static_cast<uint8_t>(configuration_value));
  if (kr != kIOReturnSuccess) {
    std::move(callback).Run(false);
    return;
  }

  device_->ActiveConfigurationChanged(configuration_value);

  std::move(callback).Run(true);
}

void UsbDeviceHandleMac::ClaimInterface(int interface_number,
                                        ResultCallback callback) {
  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  if (!base::IsValueInRangeForNumericType<uint8_t>(interface_number)) {
    std::move(callback).Run(false);
    return;
  }

  IOUSBFindInterfaceRequest request;
  request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
  request.bAlternateSetting = kIOUSBFindInterfaceDontCare;

  base::mac::ScopedIOObject<io_iterator_t> interface_iterator;
  IOReturn kr =
      (*device_interface_)
          ->CreateInterfaceIterator(device_interface_, &request,
                                    interface_iterator.InitializeInto());
  if (kr != kIOReturnSuccess) {
    std::move(callback).Run(false);
    return;
  }

  base::mac::ScopedIOObject<io_service_t> usb_interface;
  while (usb_interface.reset(IOIteratorNext(interface_iterator)),
         usb_interface) {
    base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> plugin_interface;
    int32_t score;
    kr = IOCreatePlugInInterfaceForService(
        usb_interface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID,
        plugin_interface.InitializeInto(), &score);

    if (kr != kIOReturnSuccess || !plugin_interface) {
      USB_LOG(ERROR) << "Unable to create a plug-in: " << std::hex << kr;
      continue;
    }

    ScopedIOUSBInterfaceInterface interface_interface;
    kr = (*plugin_interface)
             ->QueryInterface(plugin_interface.get(),
                              CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
                              reinterpret_cast<LPVOID*>(
                                  interface_interface.InitializeInto()));
    if (kr != kIOReturnSuccess || !interface_interface) {
      USB_LOG(ERROR) << "Could not create a device interface: " << std::hex
                     << kr;
      continue;
    }

    uint8_t retrieved_interface_number;
    kr = (*interface_interface)
             ->GetInterfaceNumber(interface_interface,
                                  &retrieved_interface_number);
    if (kr != kIOReturnSuccess) {
      USB_LOG(ERROR) << "Could not retrieve an interface number: " << std::hex
                     << kr;
      continue;
    }

    if (retrieved_interface_number != interface_number)
      continue;

    kr = (*interface_interface)->USBInterfaceOpen(interface_interface);
    if (kr != kIOReturnSuccess) {
      USB_LOG(ERROR) << "Could not open interface: " << std::hex << kr;
      break;
    }

    interfaces_[interface_number] = interface_interface;
    base::ScopedCFTypeRef<CFRunLoopSourceRef> run_loop_source;
    kr = (*interface_interface)
             ->CreateInterfaceAsyncEventSource(
                 interface_interface, run_loop_source.InitializeInto());
    if (kr != kIOReturnSuccess) {
      USB_LOG(ERROR) << "Could not retrieve port: " << std::hex << kr;
      (*interface_interface)->USBInterfaceClose(interface_interface);
      break;
    }
    RefreshEndpointMap();
    CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source.get(),
                       kCFRunLoopDefaultMode);
    sources_[interface_number] = run_loop_source;
    std::move(callback).Run(true);
    return;
  }
  std::move(callback).Run(false);
  USB_LOG(ERROR) << "Could not find interface matching number: "
                 << interface_number;
}

void UsbDeviceHandleMac::ReleaseInterface(int interface_number,
                                          ResultCallback callback) {
  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  auto interface_it = interfaces_.find(static_cast<uint8_t>(interface_number));
  if (interface_it == interfaces_.end()) {
    std::move(callback).Run(false);
    return;
  }

  auto released_interface = std::move(interface_it->second);
  interfaces_.erase(interface_it);

  auto source_it = sources_.find(interface_number);
  if (source_it != sources_.end()) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source_it->second.get(),
                          kCFRunLoopDefaultMode);
    sources_.erase(source_it);
  }

  IOReturn kr = (*released_interface)->USBInterfaceClose(released_interface);
  if (kr != kIOReturnSuccess) {
    std::move(callback).Run(false);
    return;
  }
  RefreshEndpointMap();
  std::move(callback).Run(true);
}

void UsbDeviceHandleMac::SetInterfaceAlternateSetting(int interface_number,
                                                      int alternate_setting,
                                                      ResultCallback callback) {
  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  auto interface_it = interfaces_.find(interface_number);
  if (interface_it == interfaces_.end()) {
    std::move(callback).Run(false);
    return;
  }
  const auto& interface_interface = interface_it->second;

  IOReturn kr =
      (*interface_interface)
          ->SetAlternateInterface(interface_interface, alternate_setting);
  if (kr != kIOReturnSuccess) {
    std::move(callback).Run(false);
    return;
  }
  RefreshEndpointMap();
  std::move(callback).Run(true);
}

void UsbDeviceHandleMac::ResetDevice(ResultCallback callback) {
  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(https://crbug.com/1096743): Figure out if open interfaces need to be
  // closed as well.
  IOReturn kr = (*device_interface_)
                    ->USBDeviceReEnumerate(device_interface_, /*options=*/0);
  if (kr != kIOReturnSuccess) {
    std::move(callback).Run(false);
    return;
  }

  Clear();
  std::move(callback).Run(true);
}

void UsbDeviceHandleMac::ClearHalt(mojom::UsbTransferDirection direction,
                                   uint8_t endpoint_number,
                                   ResultCallback callback) {
  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  uint8_t endpoint_address =
      ConvertTransferDirection(direction) | endpoint_number;
  auto* mojom_interface = FindInterfaceByEndpoint(endpoint_address);
  uint8_t interface_number = mojom_interface->interface_number;

  auto interface_it = interfaces_.find(interface_number);
  if (interface_it == interfaces_.end()) {
    std::move(callback).Run(false);
    return;
  }

  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it == endpoint_map_.end()) {
    std::move(callback).Run(false);
    return;
  }

  const auto& interface_interface = interface_it->second;
  IOReturn kr = (*interface_interface)
                    ->ClearPipeStall(interface_interface,
                                     endpoint_it->second.pipe_reference);
  if (kr != kIOReturnSuccess) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void UsbDeviceHandleMac::ControlTransfer(
    mojom::UsbTransferDirection direction,
    mojom::UsbControlTransferType request_type,
    mojom::UsbControlTransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  if (!device_) {
    std::move(callback).Run(mojom::UsbTransferStatus::DISCONNECT,
                            std::move(buffer), 0);
    return;
  }

  if (!base::IsValueInRangeForNumericType<uint16_t>(buffer->size())) {
    USB_LOG(ERROR) << "Transfer too long.";
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR,
                            std::move(buffer), 0);
    return;
  }

  if (!device_source_) {
    IOReturn kr = (*device_interface_)
                      ->CreateDeviceAsyncEventSource(
                          device_interface_, device_source_.InitializeInto());
    if (kr != kIOReturnSuccess) {
      USB_LOG(ERROR) << "Unable to create device async event source: "
                     << std::hex << kr;
      std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR,
                              std::move(buffer), 0);
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), device_source_.get(),
                       kCFRunLoopDefaultMode);
  }

  IOUSBDevRequestTO device_request;
  device_request.bRequest = request;
  device_request.wValue = value;
  device_request.wIndex = index;
  device_request.bmRequestType =
      CreateRequestType(direction, request_type, recipient);
  device_request.pData = buffer->front_as<void*>();
  device_request.wLength = static_cast<uint16_t>(buffer->size());
  device_request.completionTimeout = timeout;
  device_request.noDataTimeout = timeout;

  auto transfer = std::make_unique<Transfer>();
  transfer->generic_callback = std::move(callback);
  transfer->handle = this;
  transfer->buffer = std::move(buffer);

  Transfer* transfer_ptr = transfer.get();
  auto result = transfers_.insert(std::move(transfer));
  IOReturn kr = (*device_interface_)
                    ->DeviceRequestAsyncTO(
                        device_interface_, &device_request, &AsyncIoCallback,
                        reinterpret_cast<void*>(transfer_ptr));

  if (kr != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Failed to send control request: " << std::hex << kr;
    std::move((*result.first)->generic_callback)
        .Run(mojom::UsbTransferStatus::TRANSFER_ERROR,
             std::move((*result.first)->buffer), 0);
    transfers_.erase(result.first);
  }
}

void UsbDeviceHandleMac::IsochronousTransferIn(
    uint8_t endpoint,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  if (!device_) {
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   mojom::UsbTransferStatus::DISCONNECT);
    return;
  }

  uint8_t endpoint_address =
      ConvertTransferDirection(mojom::UsbTransferDirection::INBOUND) | endpoint;
  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it == endpoint_map_.end()) {
    USB_LOG(ERROR) << "Failed to submit transfer because endpoint "
                   << int{endpoint_address}
                   << " is not part of a claimed interface.";
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  size_t length =
      std::accumulate(packet_lengths.begin(), packet_lengths.end(), 0u);
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(length);

  auto interface_it =
      interfaces_.find(endpoint_it->second.interface->interface_number);
  if (interface_it == interfaces_.end()) {
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }
  const auto& interface_interface = interface_it->second;

  uint64_t bus_frame;
  AbsoluteTime time;
  IOReturn kr = (*interface_interface)
                    ->GetBusFrameNumber(interface_interface, &bus_frame, &time);
  if (kr != kIOReturnSuccess) {
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  auto transfer = std::make_unique<Transfer>();
  transfer->isochronous_callback = std::move(callback);
  transfer->handle = this;
  transfer->buffer = buffer;
  transfer->type = mojom::UsbTransferType::ISOCHRONOUS;

  Transfer* transfer_data = transfer.get();
  auto result = transfers_.insert(std::move(transfer));

  std::vector<IOUSBIsocFrame> frame_list;
  for (const auto& size : packet_lengths) {
    if (!base::IsValueInRangeForNumericType<uint16_t>(size)) {
      USB_LOG(ERROR) << "Transfer too long.";
      ReportIsochronousTransferError(
          std::move(transfer_data->isochronous_callback), packet_lengths,
          mojom::UsbTransferStatus::TRANSFER_ERROR);
      return;
    }
    IOUSBIsocFrame frame_entry;
    frame_entry.frReqCount = static_cast<uint16_t>(size);
    frame_list.push_back(frame_entry);
  }
  transfer_data->frame_list = frame_list;

  kr = (*interface_interface)
           ->ReadIsochPipeAsync(interface_interface,
                                endpoint_it->second.pipe_reference,
                                buffer->front_as<void*>(), bus_frame,
                                static_cast<uint32_t>(packet_lengths.size()),
                                transfer->frame_list.data(), &AsyncIoCallback,
                                reinterpret_cast<void*>(transfer_data));

  if (kr != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Isochrnous read failed.";
    ReportIsochronousTransferError(
        std::move((*result.first)->isochronous_callback), packet_lengths,
        mojom::UsbTransferStatus::TRANSFER_ERROR);
    transfers_.erase(result.first);
    return;
  }
}

void UsbDeviceHandleMac::IsochronousTransferOut(
    uint8_t endpoint,
    scoped_refptr<base::RefCountedBytes> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  if (!device_) {
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   mojom::UsbTransferStatus::DISCONNECT);
    return;
  }

  uint8_t endpoint_address =
      ConvertTransferDirection(mojom::UsbTransferDirection::INBOUND) | endpoint;
  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it == endpoint_map_.end()) {
    USB_LOG(ERROR) << "Failed to submit transfer because endpoint "
                   << int{endpoint_address}
                   << " is not part of a claimed interface.";
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  auto interface_it =
      interfaces_.find(endpoint_it->second.interface->interface_number);
  if (interface_it == interfaces_.end()) {
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }
  const auto& interface_interface = interface_it->second;

  uint64_t bus_frame;
  AbsoluteTime time;
  IOReturn kr = (*interface_interface)
                    ->GetBusFrameNumber(interface_interface, &bus_frame, &time);
  if (kr != kIOReturnSuccess) {
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   mojom::UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  auto transfer = std::make_unique<Transfer>();
  transfer->isochronous_callback = std::move(callback);
  transfer->handle = this;
  transfer->buffer = buffer;
  transfer->type = mojom::UsbTransferType::ISOCHRONOUS;

  Transfer* transfer_data = transfer.get();
  auto result = transfers_.insert(std::move(transfer));

  std::vector<IOUSBIsocFrame> frame_list;
  for (const auto& size : packet_lengths) {
    if (!base::IsValueInRangeForNumericType<uint16_t>(size)) {
      USB_LOG(ERROR) << "Transfer too long.";
      ReportIsochronousTransferError(
          std::move(transfer_data->isochronous_callback), packet_lengths,
          mojom::UsbTransferStatus::TRANSFER_ERROR);
      return;
    }
    IOUSBIsocFrame frame_entry;
    frame_entry.frReqCount = static_cast<uint16_t>(size);
    frame_list.push_back(frame_entry);
  }
  transfer_data->frame_list = frame_list;

  kr = (*interface_interface)
           ->WriteIsochPipeAsync(interface_interface,
                                 endpoint_it->second.pipe_reference,
                                 buffer->front_as<void*>(), bus_frame,
                                 static_cast<uint32_t>(packet_lengths.size()),
                                 transfer->frame_list.data(), &AsyncIoCallback,
                                 reinterpret_cast<void*>(transfer_data));

  if (kr != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Isochrnous write failed.";
    ReportIsochronousTransferError(
        std::move((*result.first)->isochronous_callback), packet_lengths,
        mojom::UsbTransferStatus::TRANSFER_ERROR);
    transfers_.erase(result.first);
  }
}

void UsbDeviceHandleMac::GenericTransfer(
    mojom::UsbTransferDirection direction,
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  if (!device_) {
    std::move(callback).Run(mojom::UsbTransferStatus::DISCONNECT, buffer, 0);
    return;
  }

  uint8_t endpoint_address =
      ConvertEndpointNumberToAddress(endpoint_number, direction);

  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it == endpoint_map_.end()) {
    USB_LOG(ERROR) << "Failed to submit transfer because endpoint "
                   << int{endpoint_address}
                   << " is not part of a claimed interface.";
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR, buffer,
                            0);
    return;
  }

  if (!base::IsValueInRangeForNumericType<uint32_t>(buffer->size())) {
    USB_LOG(ERROR) << "Transfer too long.";
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR, buffer,
                            0);
    return;
  }

  auto interface_it =
      interfaces_.find(endpoint_it->second.interface->interface_number);
  if (interface_it == interfaces_.end()) {
    std::move(callback).Run(mojom::UsbTransferStatus::TRANSFER_ERROR, buffer,
                            0);
    return;
  }
  const auto& interface_interface = interface_it->second;

  auto transfer = std::make_unique<Transfer>();
  transfer->generic_callback = std::move(callback);
  transfer->handle = this;
  transfer->buffer = buffer;

  mojom::UsbTransferType transfer_type = endpoint_it->second.endpoint->type;
  transfer->type = transfer_type;

  switch (transfer_type) {
    case mojom::UsbTransferType::BULK:
      switch (direction) {
        case mojom::UsbTransferDirection::INBOUND:
          BulkIn(std::move(interface_interface),
                 endpoint_it->second.pipe_reference, buffer,
                 static_cast<uint32_t>(timeout), std::move(transfer));
          return;
        case mojom::UsbTransferDirection::OUTBOUND:
          BulkOut(std::move(interface_interface),
                  endpoint_it->second.pipe_reference, buffer,
                  static_cast<uint32_t>(timeout), std::move(transfer));
          return;
      }
    case mojom::UsbTransferType::INTERRUPT:
      switch (direction) {
        case mojom::UsbTransferDirection::INBOUND:
          InterruptIn(interface_interface, endpoint_it->second.pipe_reference,
                      buffer, std::move(transfer));
          return;
        case mojom::UsbTransferDirection::OUTBOUND:
          InterruptOut(interface_interface, endpoint_it->second.pipe_reference,
                       buffer, std::move(transfer));
          return;
      }
    default:
      std::move(transfer->generic_callback)
          .Run(mojom::UsbTransferStatus::TRANSFER_ERROR, buffer, 0);
  }
}

const mojom::UsbInterfaceInfo* UsbDeviceHandleMac::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it != endpoint_map_.end())
    return endpoint_it->second.interface;
  return nullptr;
}

UsbDeviceHandleMac::~UsbDeviceHandleMac() {}

void UsbDeviceHandleMac::BulkIn(
    const ScopedIOUSBInterfaceInterface& interface_interface,
    uint8_t pipe_reference,
    scoped_refptr<base::RefCountedBytes> buffer,
    uint32_t timeout,
    std::unique_ptr<Transfer> transfer) {
  Transfer* transfer_data = transfer.get();
  auto result = transfers_.insert(std::move(transfer));
  IOReturn kr = (*interface_interface)
                    ->ReadPipeAsyncTO(interface_interface, pipe_reference,
                                      buffer->front_as<void*>(),
                                      static_cast<uint32_t>(buffer->size()),
                                      timeout, timeout, &AsyncIoCallback,
                                      reinterpret_cast<void*>(transfer_data));

  if (kr != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Failed to read from device: " << std::hex << kr;
    std::move((*result.first)->generic_callback)
        .Run(mojom::UsbTransferStatus::TRANSFER_ERROR, buffer, 0);
    transfers_.erase(result.first);
  }
}

void UsbDeviceHandleMac::BulkOut(
    const ScopedIOUSBInterfaceInterface& interface_interface,
    uint8_t pipe_reference,
    scoped_refptr<base::RefCountedBytes> buffer,
    uint32_t timeout,
    std::unique_ptr<Transfer> transfer) {
  Transfer* transfer_data = transfer.get();
  auto result = transfers_.insert(std::move(transfer));
  IOReturn kr = (*interface_interface)
                    ->WritePipeAsyncTO(interface_interface, pipe_reference,
                                       buffer->front_as<void*>(),
                                       static_cast<uint32_t>(buffer->size()),
                                       timeout, timeout, &AsyncIoCallback,
                                       reinterpret_cast<void*>(transfer_data));

  if (kr != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Failed to write to device: " << std::hex << kr;
    std::move((*result.first)->generic_callback)
        .Run(mojom::UsbTransferStatus::TRANSFER_ERROR, buffer, 0);
    transfers_.erase(result.first);
  }
}

void UsbDeviceHandleMac::InterruptIn(
    const ScopedIOUSBInterfaceInterface& interface_interface,
    uint8_t pipe_reference,
    scoped_refptr<base::RefCountedBytes> buffer,
    std::unique_ptr<Transfer> transfer) {
  Transfer* transfer_data = transfer.get();
  auto result = transfers_.insert(std::move(transfer));
  IOReturn kr = (*interface_interface)
                    ->ReadPipeAsync(interface_interface, pipe_reference,
                                    buffer->front_as<void*>(),
                                    static_cast<uint32_t>(buffer->size()),
                                    &AsyncIoCallback,
                                    reinterpret_cast<void*>(transfer_data));
  if (kr != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Failed to read from device: " << std::hex << kr;
    std::move(transfer_data->generic_callback)
        .Run(mojom::UsbTransferStatus::TRANSFER_ERROR, buffer, 0);
    transfers_.erase(result.first);
  }
}

void UsbDeviceHandleMac::InterruptOut(
    const ScopedIOUSBInterfaceInterface& interface_interface,
    uint8_t pipe_reference,
    scoped_refptr<base::RefCountedBytes> buffer,
    std::unique_ptr<Transfer> transfer) {
  Transfer* transfer_data = transfer.get();
  auto result = transfers_.insert(std::move(transfer));
  IOReturn kr = (*interface_interface)
                    ->WritePipeAsync(interface_interface, pipe_reference,
                                     buffer->front_as<void*>(),
                                     static_cast<uint32_t>(buffer->size()),
                                     &AsyncIoCallback,
                                     reinterpret_cast<void*>(transfer_data));
  if (kr != kIOReturnSuccess) {
    USB_LOG(ERROR) << "Failed to write to device: " << std::hex << kr;
    std::move(transfer_data->generic_callback)
        .Run(mojom::UsbTransferStatus::TRANSFER_ERROR, buffer, 0);
    transfers_.erase(result.first);
  }
}

void UsbDeviceHandleMac::RefreshEndpointMap() {
  endpoint_map_.clear();
  const mojom::UsbConfigurationInfo* config = device_->GetActiveConfiguration();
  if (!config)
    return;

  for (const auto& map_entry : interfaces_) {
    uint8_t alternate_setting;
    IOReturn kr =
        (*map_entry.second)
            ->GetAlternateSetting(map_entry.second, &alternate_setting);
    if (kr != kIOReturnSuccess)
      continue;
    CombinedInterfaceInfo interface_info =
        FindInterfaceInfoFromConfig(config, map_entry.first, alternate_setting);

    if (!interface_info.IsValid())
      continue;

    // macOS references an interface's endpoint via an index number of the
    // endpoint we want in the given interface. It is called a pipe reference.
    // The indices start at 1 for each interface.
    uint8_t pipe_reference = 1;
    for (const auto& endpoint : interface_info.alternate->endpoints) {
      endpoint_map_[ConvertEndpointNumberToAddress(*endpoint)] = {
          interface_info.interface, endpoint.get(), pipe_reference};
      pipe_reference++;
    }
  }
}

void UsbDeviceHandleMac::ReportIsochronousTransferError(
    UsbDeviceHandle::IsochronousTransferCallback callback,
    std::vector<uint32_t> packet_lengths,
    mojom::UsbTransferStatus status) {
  std::vector<mojom::UsbIsochronousPacketPtr> packets;
  packets.reserve(packet_lengths.size());
  for (const auto& packet_length : packet_lengths) {
    auto packet = mojom::UsbIsochronousPacket::New();
    packet->length = packet_length;
    packet->transferred_length = 0;
    packet->status = status;
    packets.push_back(std::move(packet));
  }
  std::move(callback).Run(nullptr, std::move(packets));
}

void UsbDeviceHandleMac::Clear() {
  base::flat_set<std::unique_ptr<Transfer>, base::UniquePtrComparator>
      transfers;
  transfers.swap(transfers_);
  for (auto& transfer : transfers) {
    DCHECK(transfer);
    if (transfer->type == mojom::UsbTransferType::ISOCHRONOUS) {
      ReportIsochronousTransferError(std::move(transfer->isochronous_callback),
                                     transfer->packet_lengths,
                                     mojom::UsbTransferStatus::TRANSFER_ERROR);
    } else {
      std::move(transfer->generic_callback)
          .Run(mojom::UsbTransferStatus::TRANSFER_ERROR,
               std::move(transfer->buffer), 0);
    }
  }
  transfers.clear();
  interfaces_.clear();
  sources_.clear();
}

void UsbDeviceHandleMac::OnAsyncGeneric(IOReturn result,
                                        size_t size,
                                        Transfer* transfer) {
  auto transfer_it = transfers_.find(transfer);
  if (transfer_it == transfers_.end())
    return;
  auto transfer_ptr = std::move(*transfer_it);

  std::move(transfer_ptr->generic_callback)
      .Run(mojom::UsbTransferStatus::COMPLETED, transfer_ptr->buffer,
           transfer_ptr->buffer->size());
  transfers_.erase(transfer_it);
}

void UsbDeviceHandleMac::OnAsyncIsochronous(IOReturn result,
                                            size_t size,
                                            Transfer* transfer) {
  auto transfer_it = transfers_.find(transfer);
  if (transfer_it == transfers_.end())
    return;
  auto transfer_ptr = std::move(*transfer_it);

  std::vector<mojom::UsbIsochronousPacketPtr> packets;
  packets.reserve(transfer_ptr->frame_list.size());
  for (const auto& frame : transfer_ptr->frame_list) {
    auto packet = mojom::UsbIsochronousPacket::New();
    packet->length = frame.frReqCount;
    packet->transferred_length = frame.frActCount;
    packet->status = ConvertTransferStatus(frame.frStatus);
    packets.push_back(std::move(packet));
  }

  std::move(transfer_ptr->isochronous_callback)
      .Run(transfer_ptr->buffer, std::move(packets));
  transfers_.erase(transfer_it);
}

// static
void UsbDeviceHandleMac::AsyncIoCallback(void* refcon,
                                         IOReturn result,
                                         void* arg0) {
  auto* transfer = reinterpret_cast<Transfer*>(refcon);
  DCHECK(transfer);
  DCHECK(transfer->handle);
  if (transfer->type == mojom::UsbTransferType::ISOCHRONOUS) {
    transfer->handle->OnAsyncIsochronous(result, reinterpret_cast<size_t>(arg0),
                                         transfer);
    return;
  }
  transfer->handle->OnAsyncGeneric(result, reinterpret_cast<size_t>(arg0),
                                   transfer);
}

}  // namespace device
