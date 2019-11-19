// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_win.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <usbioctl.h>
#include <usbspec.h>
#include <winioctl.h>
#include <winusb.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/object_watcher.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_context.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_win.h"
#include "services/device/usb/usb_service.h"

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;

namespace {

uint8_t BuildRequestFlags(UsbTransferDirection direction,
                          UsbControlTransferType request_type,
                          UsbControlTransferRecipient recipient) {
  uint8_t flags = 0;

  switch (direction) {
    case UsbTransferDirection::OUTBOUND:
      flags |= BMREQUEST_HOST_TO_DEVICE << 7;
      break;
    case UsbTransferDirection::INBOUND:
      flags |= BMREQUEST_DEVICE_TO_HOST << 7;
      break;
  }

  switch (request_type) {
    case UsbControlTransferType::STANDARD:
      flags |= BMREQUEST_STANDARD << 5;
      break;
    case UsbControlTransferType::CLASS:
      flags |= BMREQUEST_CLASS << 5;
      break;
    case UsbControlTransferType::VENDOR:
      flags |= BMREQUEST_VENDOR << 5;
      break;
    case UsbControlTransferType::RESERVED:
      flags |= 4 << 5;  // Not defined by usbspec.h.
      break;
  }

  switch (recipient) {
    case UsbControlTransferRecipient::DEVICE:
      flags |= BMREQUEST_TO_DEVICE;
      break;
    case UsbControlTransferRecipient::INTERFACE:
      flags |= BMREQUEST_TO_INTERFACE;
      break;
    case UsbControlTransferRecipient::ENDPOINT:
      flags |= BMREQUEST_TO_ENDPOINT;
      break;
    case UsbControlTransferRecipient::OTHER:
      flags |= BMREQUEST_TO_OTHER;
      break;
  }

  return flags;
}

}  // namespace

// Encapsulates waiting for the completion of an overlapped event.
class UsbDeviceHandleWin::Request : public base::win::ObjectWatcher::Delegate {
 public:
  Request(HANDLE handle, bool winusb_handle)
      : handle_(handle),
        winusb_handle_(winusb_handle),
        event_(CreateEvent(nullptr, false, false, nullptr)) {
    memset(&overlapped_, 0, sizeof(overlapped_));
    overlapped_.hEvent = event_.Get();
  }

  ~Request() override = default;

  // Starts watching for completion of the overlapped event.
  void MaybeStartWatching(
      BOOL success,
      DWORD last_error,
      base::OnceCallback<void(Request*, DWORD, size_t)> callback) {
    callback_ = std::move(callback);
    if (success) {
      OnObjectSignaled(event_.Get());
    } else {
      if (last_error == ERROR_IO_PENDING)
        watcher_.StartWatchingOnce(event_.Get(), this);
      else
        std::move(callback_).Run(this, last_error, 0);
    }
  }

  void Abort() {
    watcher_.StopWatching();
    std::move(callback_).Run(this, ERROR_REQUEST_ABORTED, 0);
  }

  OVERLAPPED* overlapped() { return &overlapped_; }

  // base::win::ObjectWatcher::Delegate
  void OnObjectSignaled(HANDLE object) override {
    DCHECK_EQ(object, event_.Get());
    DWORD size;
    BOOL result;
    if (winusb_handle_)
      result = WinUsb_GetOverlappedResult(handle_, &overlapped_, &size, true);
    else
      result = GetOverlappedResult(handle_, &overlapped_, &size, true);
    DWORD last_error = GetLastError();

    if (result)
      std::move(callback_).Run(this, ERROR_SUCCESS, size);
    else
      std::move(callback_).Run(this, last_error, 0);
  }

 private:
  HANDLE handle_;
  // Records whether |handle_| above is a true HANDLE or a
  // WINUSB_INTERFACE_HANDLE.
  bool winusb_handle_;
  OVERLAPPED overlapped_;
  base::win::ScopedHandle event_;
  base::win::ObjectWatcher watcher_;
  base::OnceCallback<void(Request*, DWORD, size_t)> callback_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

UsbDeviceHandleWin::Interface::Interface() = default;

UsbDeviceHandleWin::Interface::~Interface() = default;

scoped_refptr<UsbDevice> UsbDeviceHandleWin::GetDevice() const {
  return device_;
}

void UsbDeviceHandleWin::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_)
    return;

  device_->HandleClosed(this);
  device_ = nullptr;

  if (hub_handle_.IsValid()) {
    CancelIo(hub_handle_.Get());
    hub_handle_.Close();
  }

  if (function_handle_.IsValid()) {
    CancelIo(function_handle_.Get());
    function_handle_.Close();
    first_interface_handle_ = INVALID_HANDLE_VALUE;
  }

  // Aborting requests may run or destroy callbacks holding the last reference
  // to this object so hold a reference for the rest of this method.
  scoped_refptr<UsbDeviceHandleWin> self(this);
  while (!requests_.empty())
    requests_.begin()->second->Abort();
}

void UsbDeviceHandleWin::SetConfiguration(int configuration_value,
                                          ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Setting device configuration is not supported on Windows.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), false));
}

void UsbDeviceHandleWin::ClaimInterface(int interface_number,
                                        ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  auto interface_it = interfaces_.find(interface_number);
  if (interface_it == interfaces_.end()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }
  Interface* interface = &interface_it->second;

  if (!OpenInterfaceHandle(interface)) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  interface->claimed = true;
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), true));
}

void UsbDeviceHandleWin::ReleaseInterface(int interface_number,
                                          ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  auto interface_it = interfaces_.find(interface_number);
  if (interface_it == interfaces_.end()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }
  Interface* interface = &interface_it->second;

  if (interface->handle.IsValid()) {
    interface->handle.Close();
    interface->alternate_setting = 0;
  }

  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), true));
}

void UsbDeviceHandleWin::SetInterfaceAlternateSetting(int interface_number,
                                                      int alternate_setting,
                                                      ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // TODO: Unimplemented.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), false));
}

void UsbDeviceHandleWin::ResetDevice(ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Resetting the device is not supported on Windows.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), false));
}

void UsbDeviceHandleWin::ClearHalt(uint8_t endpoint, ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // TODO: Unimplemented.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), false));
}

void UsbDeviceHandleWin::ControlTransfer(
    UsbTransferDirection direction,
    UsbControlTransferType request_type,
    UsbControlTransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  UsbTransferStatus::DISCONNECT, nullptr, 0));
    return;
  }

  if (hub_handle_.IsValid()) {
    if (direction == UsbTransferDirection::INBOUND &&
        request_type == UsbControlTransferType::STANDARD &&
        recipient == UsbControlTransferRecipient::DEVICE &&
        request == USB_REQUEST_GET_DESCRIPTOR) {
      if ((value >> 8) == USB_DEVICE_DESCRIPTOR_TYPE) {
        auto* node_connection_info = new USB_NODE_CONNECTION_INFORMATION_EX;
        node_connection_info->ConnectionIndex = device_->port_number();

        Request* request = MakeRequest(false /* winusb_handle */);
        BOOL result = DeviceIoControl(
            hub_handle_.Get(), IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
            node_connection_info, sizeof(*node_connection_info),
            node_connection_info, sizeof(*node_connection_info), nullptr,
            request->overlapped());
        DWORD last_error = GetLastError();
        request->MaybeStartWatching(
            result, last_error,
            base::BindOnce(&UsbDeviceHandleWin::GotNodeConnectionInformation,
                           weak_factory_.GetWeakPtr(), std::move(callback),
                           base::Owned(node_connection_info), buffer));
        return;
      } else if (((value >> 8) == USB_CONFIGURATION_DESCRIPTOR_TYPE) ||
                 ((value >> 8) == USB_STRING_DESCRIPTOR_TYPE)) {
        size_t size = sizeof(USB_DESCRIPTOR_REQUEST) + buffer->size();
        auto request_buffer = base::MakeRefCounted<base::RefCountedBytes>(size);
        USB_DESCRIPTOR_REQUEST* descriptor_request =
            request_buffer->front_as<USB_DESCRIPTOR_REQUEST>();
        descriptor_request->ConnectionIndex = device_->port_number();
        descriptor_request->SetupPacket.bmRequest = BMREQUEST_DEVICE_TO_HOST;
        descriptor_request->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
        descriptor_request->SetupPacket.wValue = value;
        descriptor_request->SetupPacket.wIndex = index;
        descriptor_request->SetupPacket.wLength = buffer->size();

        Request* request = MakeRequest(false /* winusb_handle */);
        BOOL result = DeviceIoControl(
            hub_handle_.Get(), IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
            request_buffer->front(), size, request_buffer->front(), size,
            nullptr, request->overlapped());
        DWORD last_error = GetLastError();
        request->MaybeStartWatching(
            result, last_error,
            base::BindOnce(&UsbDeviceHandleWin::GotDescriptorFromNodeConnection,
                           weak_factory_.GetWeakPtr(), std::move(callback),
                           request_buffer, buffer));
        return;
      }
    }

    // Unsupported transfer for hub.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       nullptr, 0));
    return;
  }

  // Submit a normal control transfer.
  WINUSB_INTERFACE_HANDLE handle =
      GetInterfaceForControlTransfer(recipient, index);
  if (handle == INVALID_HANDLE_VALUE) {
    USB_LOG(ERROR) << "Interface handle not available for control transfer.";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       nullptr, 0));
    return;
  }

  WINUSB_SETUP_PACKET setup = {0};
  setup.RequestType = BuildRequestFlags(direction, request_type, recipient);
  setup.Request = request;
  setup.Value = value;
  setup.Index = index;
  setup.Length = buffer->size();

  Request* control_request = MakeRequest(true /* winusb_handle */);
  BOOL result =
      WinUsb_ControlTransfer(handle, setup, buffer->front(), buffer->size(),
                             nullptr, control_request->overlapped());
  DWORD last_error = GetLastError();
  control_request->MaybeStartWatching(
      result, last_error,
      base::BindOnce(&UsbDeviceHandleWin::TransferComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), buffer));
}

void UsbDeviceHandleWin::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Isochronous is not yet supported on Windows.
  ReportIsochronousError(packet_lengths, std::move(callback),
                         UsbTransferStatus::TRANSFER_ERROR);
}

void UsbDeviceHandleWin::IsochronousTransferOut(
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Isochronous is not yet supported on Windows.
  ReportIsochronousError(packet_lengths, std::move(callback),
                         UsbTransferStatus::TRANSFER_ERROR);
}

void UsbDeviceHandleWin::GenericTransfer(
    UsbTransferDirection direction,
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint8_t endpoint_address =
      ConvertEndpointNumberToAddress(endpoint_number, direction);

  auto endpoint_it = endpoints_.find(endpoint_address);
  if (endpoint_it == endpoints_.end()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       nullptr, 0));
    return;
  }

  auto interface_it =
      interfaces_.find(endpoint_it->second.interface->interface_number);
  DCHECK(interface_it != interfaces_.end());
  Interface* interface = &interface_it->second;
  if (!interface->claimed) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       nullptr, 0));
    return;
  }

  DCHECK(interface->handle.IsValid());
  Request* request = MakeRequest(true /* winusb_handle */);
  BOOL result;
  if (direction == UsbTransferDirection::INBOUND) {
    result = WinUsb_ReadPipe(interface->handle.Get(), endpoint_address,
                             buffer->front(), buffer->size(), nullptr,
                             request->overlapped());
  } else {
    result = WinUsb_WritePipe(interface->handle.Get(), endpoint_address,
                              buffer->front(), buffer->size(), nullptr,
                              request->overlapped());
  }
  DWORD last_error = GetLastError();
  request->MaybeStartWatching(
      result, last_error,
      base::BindOnce(&UsbDeviceHandleWin::TransferComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(buffer)));
}

const mojom::UsbInterfaceInfo* UsbDeviceHandleWin::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = endpoints_.find(endpoint_address);
  if (it != endpoints_.end())
    return it->second.interface;
  return nullptr;
}

UsbDeviceHandleWin::UsbDeviceHandleWin(scoped_refptr<UsbDeviceWin> device,
                                       bool composite)
    : device_(std::move(device)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      blocking_task_runner_(UsbService::CreateBlockingTaskRunner()) {
  DCHECK(!composite);
  // Windows only supports configuration 1, which therefore must be active.
  DCHECK(device_->GetActiveConfiguration());

  for (const auto& interface : device_->GetActiveConfiguration()->interfaces) {
    for (const auto& alternate : interface->alternates) {
      if (alternate->alternate_setting != 0)
        continue;

      Interface& interface_info = interfaces_[interface->interface_number];
      interface_info.interface_number = interface->interface_number;
      interface_info.first_interface = interface->first_interface;
      RegisterEndpoints(
          CombinedInterfaceInfo(interface.get(), alternate.get()));
    }
  }
}

UsbDeviceHandleWin::UsbDeviceHandleWin(scoped_refptr<UsbDeviceWin> device,
                                       base::win::ScopedHandle handle)
    : device_(std::move(device)),
      hub_handle_(std::move(handle)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      blocking_task_runner_(UsbService::CreateBlockingTaskRunner()) {}

UsbDeviceHandleWin::~UsbDeviceHandleWin() {}

bool UsbDeviceHandleWin::OpenInterfaceHandle(Interface* interface) {
  if (interface->handle.IsValid())
    return true;

  WINUSB_INTERFACE_HANDLE handle;
  if (interface->first_interface == interface->interface_number) {
    if (!function_handle_.IsValid()) {
      function_handle_.Set(CreateFileA(
          device_->device_path().c_str(), GENERIC_READ | GENERIC_WRITE,
          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
          FILE_FLAG_OVERLAPPED, nullptr));
      if (!function_handle_.IsValid()) {
        USB_PLOG(ERROR) << "Failed to open " << device_->device_path();
        return false;
      }
    }

    if (!WinUsb_Initialize(function_handle_.Get(), &handle)) {
      USB_PLOG(ERROR) << "Failed to initialize WinUSB handle";
      return false;
    }

    first_interface_handle_ = handle;
  } else {
    auto first_interface_it = interfaces_.find(interface->first_interface);
    DCHECK(first_interface_it != interfaces_.end());
    Interface* first_interface = &first_interface_it->second;

    if (!OpenInterfaceHandle(first_interface))
      return false;

    int index = interface->interface_number - interface->first_interface - 1;
    if (!WinUsb_GetAssociatedInterface(first_interface->handle.Get(), index,
                                       &handle)) {
      USB_PLOG(ERROR) << "Failed to get associated interface " << index
                      << " from interface "
                      << static_cast<int>(interface->first_interface);
      return false;
    }
  }

  interface->handle.Set(handle);
  return interface->handle.IsValid();
}

void UsbDeviceHandleWin::RegisterEndpoints(
    const CombinedInterfaceInfo& interface) {
  DCHECK(interface.IsValid());
  for (const auto& endpoint : interface.alternate->endpoints) {
    Endpoint& endpoint_info =
        endpoints_[ConvertEndpointNumberToAddress(*endpoint)];
    endpoint_info.interface = interface.interface;
    endpoint_info.type = endpoint->type;
  }
}

void UsbDeviceHandleWin::UnregisterEndpoints(
    const CombinedInterfaceInfo& interface) {
  for (const auto& endpoint : interface.alternate->endpoints)
    endpoints_.erase(ConvertEndpointNumberToAddress(*endpoint));
}

WINUSB_INTERFACE_HANDLE UsbDeviceHandleWin::GetInterfaceForControlTransfer(
    UsbControlTransferRecipient recipient,
    uint16_t index) {
  if (recipient == UsbControlTransferRecipient::ENDPOINT) {
    auto endpoint_it = endpoints_.find(index & 0xff);
    if (endpoint_it == endpoints_.end())
      return INVALID_HANDLE_VALUE;

    // "Fall through" to the interface case.
    recipient = UsbControlTransferRecipient::INTERFACE;
    index = endpoint_it->second.interface->interface_number;
  }

  Interface* interface;
  if (recipient == UsbControlTransferRecipient::INTERFACE) {
    auto interface_it = interfaces_.find(index & 0xff);
    if (interface_it == interfaces_.end())
      return INVALID_HANDLE_VALUE;

    interface = &interface_it->second;
  } else {
    // TODO: To support composite devices a particular function handle must be
    // chosen, probably arbitrarily.
    interface = &interfaces_[0];
  }

  OpenInterfaceHandle(interface);
  return interface->handle.Get();
}

UsbDeviceHandleWin::Request* UsbDeviceHandleWin::MakeRequest(
    bool winusb_handle) {
  auto request = std::make_unique<Request>(
      winusb_handle ? first_interface_handle_ : hub_handle_.Get(),
      winusb_handle);
  Request* request_ptr = request.get();
  requests_[request_ptr] = std::move(request);
  return request_ptr;
}

std::unique_ptr<UsbDeviceHandleWin::Request> UsbDeviceHandleWin::UnlinkRequest(
    UsbDeviceHandleWin::Request* request_ptr) {
  auto it = requests_.find(request_ptr);
  DCHECK(it != requests_.end());
  std::unique_ptr<Request> request = std::move(it->second);
  requests_.erase(it);
  return request;
}

void UsbDeviceHandleWin::GotNodeConnectionInformation(
    TransferCallback callback,
    void* node_connection_info_ptr,
    scoped_refptr<base::RefCountedBytes> buffer,
    Request* request_ptr,
    DWORD win32_result,
    size_t bytes_transferred) {
  USB_NODE_CONNECTION_INFORMATION_EX* node_connection_info =
      static_cast<USB_NODE_CONNECTION_INFORMATION_EX*>(
          node_connection_info_ptr);
  std::unique_ptr<Request> request = UnlinkRequest(request_ptr);

  if (win32_result != ERROR_SUCCESS) {
    SetLastError(win32_result);
    USB_PLOG(ERROR) << "Failed to get node connection information";
    std::move(callback).Run(UsbTransferStatus::TRANSFER_ERROR, nullptr, 0);
    return;
  }

  DCHECK_EQ(bytes_transferred, sizeof(USB_NODE_CONNECTION_INFORMATION_EX));
  bytes_transferred = std::min(sizeof(USB_DEVICE_DESCRIPTOR), buffer->size());
  memcpy(buffer->front(), &node_connection_info->DeviceDescriptor,
         bytes_transferred);
  std::move(callback).Run(UsbTransferStatus::COMPLETED, buffer,
                          bytes_transferred);
}

void UsbDeviceHandleWin::GotDescriptorFromNodeConnection(
    TransferCallback callback,
    scoped_refptr<base::RefCountedBytes> request_buffer,
    scoped_refptr<base::RefCountedBytes> original_buffer,
    Request* request_ptr,
    DWORD win32_result,
    size_t bytes_transferred) {
  std::unique_ptr<Request> request = UnlinkRequest(request_ptr);

  if (win32_result != ERROR_SUCCESS) {
    SetLastError(win32_result);
    USB_PLOG(ERROR) << "Failed to read descriptor from node connection";
    std::move(callback).Run(UsbTransferStatus::TRANSFER_ERROR, nullptr, 0);
    return;
  }

  DCHECK_GE(bytes_transferred, sizeof(USB_DESCRIPTOR_REQUEST));
  bytes_transferred -= sizeof(USB_DESCRIPTOR_REQUEST);
  DCHECK_LE(bytes_transferred, original_buffer->size());
  memcpy(original_buffer->front(),
         request_buffer->front() + sizeof(USB_DESCRIPTOR_REQUEST),
         bytes_transferred);
  std::move(callback).Run(UsbTransferStatus::COMPLETED, original_buffer,
                          bytes_transferred);
}

void UsbDeviceHandleWin::TransferComplete(
    TransferCallback callback,
    scoped_refptr<base::RefCountedBytes> buffer,
    Request* request_ptr,
    DWORD win32_result,
    size_t bytes_transferred) {
  std::unique_ptr<Request> request = UnlinkRequest(request_ptr);
  UsbTransferStatus status = UsbTransferStatus::COMPLETED;

  if (win32_result != ERROR_SUCCESS) {
    SetLastError(win32_result);
    USB_PLOG(ERROR) << "Transfer failed";

    buffer = nullptr;
    bytes_transferred = 0;
    status = UsbTransferStatus::TRANSFER_ERROR;
  }

  std::move(callback).Run(status, std::move(buffer), bytes_transferred);
}

void UsbDeviceHandleWin::ReportIsochronousError(
    const std::vector<uint32_t>& packet_lengths,
    IsochronousTransferCallback callback,
    UsbTransferStatus status) {
  std::vector<mojom::UsbIsochronousPacketPtr> packets(packet_lengths.size());
  for (size_t i = 0; i < packet_lengths.size(); ++i) {
    packets[i] = mojom::UsbIsochronousPacket::New();
    packets[i]->length = packet_lengths[i];
    packets[i]->transferred_length = 0;
    packets[i]->status = status;
  }
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), nullptr,
                                                   std::move(packets)));
}

}  // namespace device
