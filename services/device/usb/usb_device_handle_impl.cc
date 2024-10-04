// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/usb/usb_device_handle_impl.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_context.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_impl.h"
#include "services/device/usb/usb_error.h"
#include "services/device/usb/usb_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbIsochronousPacketPtr;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;
using mojom::UsbTransferType;

void HandleTransferCompletion(PlatformUsbTransferHandle transfer);

namespace {

uint8_t ConvertTransferDirection(UsbTransferDirection direction) {
  switch (direction) {
    case UsbTransferDirection::INBOUND:
      return LIBUSB_ENDPOINT_IN;
    case UsbTransferDirection::OUTBOUND:
      return LIBUSB_ENDPOINT_OUT;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

uint8_t CreateRequestType(UsbTransferDirection direction,
                          UsbControlTransferType request_type,
                          UsbControlTransferRecipient recipient) {
  uint8_t result = ConvertTransferDirection(direction);

  switch (request_type) {
    case UsbControlTransferType::STANDARD:
      result |= LIBUSB_REQUEST_TYPE_STANDARD;
      break;
    case UsbControlTransferType::CLASS:
      result |= LIBUSB_REQUEST_TYPE_CLASS;
      break;
    case UsbControlTransferType::VENDOR:
      result |= LIBUSB_REQUEST_TYPE_VENDOR;
      break;
    case UsbControlTransferType::RESERVED:
      result |= LIBUSB_REQUEST_TYPE_RESERVED;
      break;
  }

  switch (recipient) {
    case UsbControlTransferRecipient::DEVICE:
      result |= LIBUSB_RECIPIENT_DEVICE;
      break;
    case UsbControlTransferRecipient::INTERFACE:
      result |= LIBUSB_RECIPIENT_INTERFACE;
      break;
    case UsbControlTransferRecipient::ENDPOINT:
      result |= LIBUSB_RECIPIENT_ENDPOINT;
      break;
    case UsbControlTransferRecipient::OTHER:
      result |= LIBUSB_RECIPIENT_OTHER;
      break;
  }

  return result;
}

static UsbTransferStatus ConvertTransferStatus(
    const libusb_transfer_status status) {
  switch (status) {
    case LIBUSB_TRANSFER_COMPLETED:
      return UsbTransferStatus::COMPLETED;
    case LIBUSB_TRANSFER_ERROR:
      return UsbTransferStatus::TRANSFER_ERROR;
    case LIBUSB_TRANSFER_TIMED_OUT:
      return UsbTransferStatus::TIMEOUT;
    case LIBUSB_TRANSFER_STALL:
      return UsbTransferStatus::STALLED;
    case LIBUSB_TRANSFER_NO_DEVICE:
      return UsbTransferStatus::DISCONNECT;
    case LIBUSB_TRANSFER_OVERFLOW:
      return UsbTransferStatus::BABBLE;
    case LIBUSB_TRANSFER_CANCELLED:
      return UsbTransferStatus::CANCELLED;
  }
  NOTREACHED_IN_MIGRATION();
  return UsbTransferStatus::TRANSFER_ERROR;
}

}  // namespace

class UsbDeviceHandleImpl::InterfaceClaimer
    : public base::RefCountedThreadSafe<UsbDeviceHandleImpl::InterfaceClaimer> {
 public:
  InterfaceClaimer(scoped_refptr<UsbDeviceHandleImpl> handle,
                   int interface_number,
                   scoped_refptr<base::SequencedTaskRunner> task_runner);

  InterfaceClaimer(const InterfaceClaimer&) = delete;
  InterfaceClaimer& operator=(const InterfaceClaimer&) = delete;

  int interface_number() const { return interface_number_; }
  int alternate_setting() const { return alternate_setting_; }
  void set_alternate_setting(const int alternate_setting) {
    alternate_setting_ = alternate_setting;
  }

  void set_release_callback(ResultCallback callback) {
    release_callback_ = std::move(callback);
  }

 private:
  friend class base::RefCountedThreadSafe<InterfaceClaimer>;
  ~InterfaceClaimer();

  const scoped_refptr<UsbDeviceHandleImpl> handle_;
  const int interface_number_;
  int alternate_setting_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ResultCallback release_callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

UsbDeviceHandleImpl::InterfaceClaimer::InterfaceClaimer(
    scoped_refptr<UsbDeviceHandleImpl> handle,
    int interface_number,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : handle_(handle),
      interface_number_(interface_number),
      alternate_setting_(0),
      task_runner_(task_runner) {}

UsbDeviceHandleImpl::InterfaceClaimer::~InterfaceClaimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rc = libusb_release_interface(handle_->handle(), interface_number_);
  if (rc != LIBUSB_SUCCESS) {
    USB_LOG(DEBUG) << "Failed to release interface: "
                   << ConvertPlatformUsbErrorToString(rc);
  }
  if (!release_callback_.is_null()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(release_callback_), rc == LIBUSB_SUCCESS));
  }
}

// This inner class owns the underlying libusb_transfer and may outlast
// the UsbDeviceHandle that created it.
class UsbDeviceHandleImpl::Transfer {
 public:
  // These functions takes |*callback| if they successfully create Transfer
  // instance, otherwise |*callback| left unchanged.
  static std::unique_ptr<Transfer> CreateControlTransfer(
      scoped_refptr<UsbDeviceHandleImpl> device_handle,
      uint8_t type,
      uint8_t request,
      uint16_t value,
      uint16_t index,
      uint16_t length,
      scoped_refptr<base::RefCountedBytes> buffer,
      unsigned int timeout,
      TransferCallback* callback);
  static std::unique_ptr<Transfer> CreateBulkTransfer(
      scoped_refptr<UsbDeviceHandleImpl> device_handle,
      uint8_t endpoint,
      scoped_refptr<base::RefCountedBytes> buffer,
      int length,
      unsigned int timeout,
      TransferCallback* callback);
  static std::unique_ptr<Transfer> CreateInterruptTransfer(
      scoped_refptr<UsbDeviceHandleImpl> device_handle,
      uint8_t endpoint,
      scoped_refptr<base::RefCountedBytes> buffer,
      int length,
      unsigned int timeout,
      TransferCallback* callback);
  static std::unique_ptr<Transfer> CreateIsochronousTransfer(
      scoped_refptr<UsbDeviceHandleImpl> device_handle,
      uint8_t endpoint,
      scoped_refptr<base::RefCountedBytes> buffer,
      size_t length,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      IsochronousTransferCallback* callback);

  ~Transfer();

  void Submit();
  void Cancel();
  void ProcessCompletion();
  void TransferComplete(UsbTransferStatus status, size_t bytes_transferred);

  const UsbDeviceHandleImpl::InterfaceClaimer* claimed_interface() const {
    return claimed_interface_.get();
  }

 private:
  Transfer(scoped_refptr<UsbDeviceHandleImpl> device_handle,
           scoped_refptr<InterfaceClaimer> claimed_interface,
           UsbTransferType transfer_type,
           scoped_refptr<base::RefCountedBytes> buffer,
           size_t length,
           TransferCallback callback);
  Transfer(scoped_refptr<UsbDeviceHandleImpl> device_handle,
           scoped_refptr<InterfaceClaimer> claimed_interface,
           scoped_refptr<base::RefCountedBytes> buffer,
           IsochronousTransferCallback callback);

  static void LIBUSB_CALL PlatformCallback(PlatformUsbTransferHandle handle);

  void IsochronousTransferComplete();

  UsbTransferType transfer_type_;
  scoped_refptr<UsbDeviceHandleImpl> device_handle_;
  PlatformUsbTransferHandle platform_transfer_ = nullptr;
  scoped_refptr<base::RefCountedBytes> buffer_;
  scoped_refptr<UsbDeviceHandleImpl::InterfaceClaimer> claimed_interface_;
  size_t length_;
  bool cancelled_ = false;
  TransferCallback callback_;
  IsochronousTransferCallback iso_callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

// static
std::unique_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateControlTransfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    uint8_t type,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    uint16_t length,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback* callback) {
  std::unique_ptr<Transfer> transfer(
      new Transfer(device_handle, nullptr, UsbTransferType::CONTROL, buffer,
                   length + LIBUSB_CONTROL_SETUP_SIZE, std::move(*callback)));

  transfer->platform_transfer_ = libusb_alloc_transfer(0);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate control transfer.";
    *callback = std::move(transfer->callback_);
    return nullptr;
  }

  libusb_fill_control_setup(buffer->as_vector().data(), type, request, value,
                            index, length);
  libusb_fill_control_transfer(transfer->platform_transfer_,
                               device_handle->handle(),
                               buffer->as_vector().data(),
                               &UsbDeviceHandleImpl::Transfer::PlatformCallback,
                               transfer.get(), timeout);

  return transfer;
}

// static
std::unique_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateBulkTransfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    uint8_t endpoint,
    scoped_refptr<base::RefCountedBytes> buffer,
    int length,
    unsigned int timeout,
    TransferCallback* callback) {
  std::unique_ptr<Transfer> transfer(new Transfer(
      device_handle, device_handle->GetClaimedInterfaceForEndpoint(endpoint),
      UsbTransferType::BULK, buffer, length, std::move(*callback)));

  transfer->platform_transfer_ = libusb_alloc_transfer(0);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate bulk transfer.";
    *callback = std::move(transfer->callback_);
    return nullptr;
  }

  libusb_fill_bulk_transfer(transfer->platform_transfer_,
                            device_handle->handle(), endpoint,
                            buffer->as_vector().data(), length,
                            &UsbDeviceHandleImpl::Transfer::PlatformCallback,
                            transfer.get(), timeout);

  return transfer;
}

// static
std::unique_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateInterruptTransfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    uint8_t endpoint,
    scoped_refptr<base::RefCountedBytes> buffer,
    int length,
    unsigned int timeout,
    TransferCallback* callback) {
  std::unique_ptr<Transfer> transfer(new Transfer(
      device_handle, device_handle->GetClaimedInterfaceForEndpoint(endpoint),
      UsbTransferType::INTERRUPT, buffer, length, std::move(*callback)));

  transfer->platform_transfer_ = libusb_alloc_transfer(0);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate interrupt transfer.";
    *callback = std::move(transfer->callback_);
    return nullptr;
  }

  libusb_fill_interrupt_transfer(
      transfer->platform_transfer_, device_handle->handle(), endpoint,
      buffer->as_vector().data(), length,
      &UsbDeviceHandleImpl::Transfer::PlatformCallback, transfer.get(),
      timeout);

  return transfer;
}

// static
std::unique_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateIsochronousTransfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    uint8_t endpoint,
    scoped_refptr<base::RefCountedBytes> buffer,
    size_t length,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback* callback) {
  std::unique_ptr<Transfer> transfer(new Transfer(
      device_handle, device_handle->GetClaimedInterfaceForEndpoint(endpoint),
      buffer, std::move(*callback)));

  int num_packets = static_cast<int>(packet_lengths.size());
  transfer->platform_transfer_ = libusb_alloc_transfer(num_packets);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate isochronous transfer.";
    *callback = std::move(transfer->iso_callback_);
    return nullptr;
  }

  libusb_fill_iso_transfer(
      transfer->platform_transfer_, device_handle->handle(), endpoint,
      buffer->as_vector().data(), static_cast<int>(length), num_packets,
      &Transfer::PlatformCallback, transfer.get(), timeout);

  for (size_t i = 0; i < packet_lengths.size(); ++i)
    transfer->platform_transfer_->iso_packet_desc[i].length = packet_lengths[i];

  return transfer;
}

UsbDeviceHandleImpl::Transfer::Transfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    scoped_refptr<InterfaceClaimer> claimed_interface,
    UsbTransferType transfer_type,
    scoped_refptr<base::RefCountedBytes> buffer,
    size_t length,
    TransferCallback callback)
    : transfer_type_(transfer_type),
      device_handle_(device_handle),
      buffer_(buffer),
      claimed_interface_(claimed_interface),
      length_(length),
      callback_(std::move(callback)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

UsbDeviceHandleImpl::Transfer::Transfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    scoped_refptr<InterfaceClaimer> claimed_interface,
    scoped_refptr<base::RefCountedBytes> buffer,
    IsochronousTransferCallback callback)
    : transfer_type_(UsbTransferType::ISOCHRONOUS),
      device_handle_(device_handle),
      buffer_(buffer),
      claimed_interface_(claimed_interface),
      iso_callback_(std::move(callback)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

UsbDeviceHandleImpl::Transfer::~Transfer() {
  if (platform_transfer_) {
    libusb_free_transfer(platform_transfer_);
  }
}

void UsbDeviceHandleImpl::Transfer::Submit() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const int rv = libusb_submit_transfer(platform_transfer_);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to submit transfer: "
                   << ConvertPlatformUsbErrorToString(rv);
    TransferComplete(UsbTransferStatus::TRANSFER_ERROR, 0);
  }
}

void UsbDeviceHandleImpl::Transfer::Cancel() {
  if (!cancelled_) {
    libusb_cancel_transfer(platform_transfer_);
    claimed_interface_ = nullptr;
  }
  cancelled_ = true;
}

void UsbDeviceHandleImpl::Transfer::ProcessCompletion() {
  DCHECK_GE(platform_transfer_->actual_length, 0)
      << "Negative actual length received";
  size_t actual_length =
      static_cast<size_t>(std::max(platform_transfer_->actual_length, 0));

  DCHECK(length_ >= actual_length)
      << "data too big for our buffer (libusb failure?)";

  switch (transfer_type_) {
    case UsbTransferType::CONTROL:
      // If the transfer is a control transfer we do not expose the control
      // setup header to the caller. This logic strips off the header if
      // present before invoking the callback provided with the transfer.
      if (actual_length > 0) {
        CHECK(length_ >= LIBUSB_CONTROL_SETUP_SIZE)
            << "buffer was not correctly set: too small for the control header";

        if (length_ >= (LIBUSB_CONTROL_SETUP_SIZE + actual_length)) {
          auto resized_buffer =
              base::MakeRefCounted<base::RefCountedBytes>(actual_length);
          base::span(resized_buffer->as_vector())
              .copy_from(base::span(*buffer_).subspan(LIBUSB_CONTROL_SETUP_SIZE,
                                                      actual_length));
          buffer_ = resized_buffer;
        }
      }
      [[fallthrough]];

    case UsbTransferType::BULK:
    case UsbTransferType::INTERRUPT:
      TransferComplete(ConvertTransferStatus(platform_transfer_->status),
                       actual_length);
      break;

    case UsbTransferType::ISOCHRONOUS:
      IsochronousTransferComplete();
      break;

    default:
      NOTREACHED_IN_MIGRATION() << "Invalid usb transfer type";
      break;
  }
}

/* static */
void LIBUSB_CALL UsbDeviceHandleImpl::Transfer::PlatformCallback(
    PlatformUsbTransferHandle platform_transfer) {
  Transfer* transfer =
      reinterpret_cast<Transfer*>(platform_transfer->user_data);
  DCHECK(transfer->platform_transfer_ == platform_transfer);
  transfer->ProcessCompletion();
}

void UsbDeviceHandleImpl::Transfer::TransferComplete(UsbTransferStatus status,
                                                     size_t bytes_transferred) {
  base::OnceClosure closure;
  if (transfer_type_ == UsbTransferType::ISOCHRONOUS) {
    DCHECK_NE(LIBUSB_TRANSFER_COMPLETED, platform_transfer_->status);
    std::vector<UsbIsochronousPacketPtr> packets(
        platform_transfer_->num_iso_packets);
    for (size_t i = 0; i < packets.size(); ++i) {
      packets[i] = mojom::UsbIsochronousPacket::New();
      packets[i]->length = platform_transfer_->iso_packet_desc[i].length;
      packets[i]->transferred_length = 0;
      packets[i]->status = status;
    }
    closure =
        base::BindOnce(std::move(iso_callback_), buffer_, std::move(packets));
  } else {
    closure = base::BindOnce(std::move(callback_), status, buffer_,
                             bytes_transferred);
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceHandleImpl::TransferComplete, device_handle_,
                     base::Unretained(this), std::move(closure)));
}

void UsbDeviceHandleImpl::Transfer::IsochronousTransferComplete() {
  std::vector<UsbIsochronousPacketPtr> packets(
      platform_transfer_->num_iso_packets);
  for (size_t i = 0; i < packets.size(); ++i) {
    packets[i] = mojom::UsbIsochronousPacket::New();
    packets[i]->length = platform_transfer_->iso_packet_desc[i].length;
    packets[i]->transferred_length =
        platform_transfer_->iso_packet_desc[i].actual_length;
    packets[i]->status =
        ConvertTransferStatus(platform_transfer_->iso_packet_desc[i].status);
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbDeviceHandleImpl::TransferComplete,
                                device_handle_, base::Unretained(this),
                                base::BindOnce(std::move(iso_callback_),
                                               buffer_, std::move(packets))));
}

scoped_refptr<UsbDevice> UsbDeviceHandleImpl::GetDevice() const {
  return device_;
}

void UsbDeviceHandleImpl::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_)
    return;

  // Cancel all the transfers, their callbacks will be called some time later.
  for (Transfer* transfer : transfers_)
    transfer->Cancel();

  // Release all remaining interfaces once their transfers have completed.
  // This loop must ensure that what may be the final reference is released on
  // the right thread.
  for (auto& map_entry : claimed_interfaces_) {
    blocking_task_runner_->ReleaseSoon(FROM_HERE, std::move(map_entry.second));
  }

  device_->HandleClosed(this);
  device_ = nullptr;

  // The device handle cannot be closed here. When libusb_cancel_transfer is
  // finished the last references to this device will be released and the
  // destructor will close the handle.
}

void UsbDeviceHandleImpl::SetConfiguration(int configuration_value,
                                           ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  for (Transfer* transfer : transfers_) {
    transfer->Cancel();
  }
  claimed_interfaces_.clear();

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceHandleImpl::SetConfigurationBlocking, this,
                     configuration_value, std::move(callback)));
}

void UsbDeviceHandleImpl::ClaimInterface(int interface_number,
                                         ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    std::move(callback).Run(false);
    return;
  }
  if (base::Contains(claimed_interfaces_, interface_number)) {
    std::move(callback).Run(true);
    return;
  }

  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbDeviceHandleImpl::ClaimInterfaceBlocking,
                                this, interface_number, std::move(callback)));
}

void UsbDeviceHandleImpl::ReleaseInterface(int interface_number,
                                           ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_ || !base::Contains(claimed_interfaces_, interface_number)) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // Cancel all the transfers on that interface.
  InterfaceClaimer* interface_claimer =
      claimed_interfaces_[interface_number].get();
  for (Transfer* transfer : transfers_) {
    if (transfer->claimed_interface() == interface_claimer) {
      transfer->Cancel();
    }
  }
  interface_claimer->set_release_callback(std::move(callback));
  blocking_task_runner_->ReleaseSoon(
      FROM_HERE, std::move(claimed_interfaces_[interface_number]));
  claimed_interfaces_.erase(interface_number);

  RefreshEndpointMap();
}

void UsbDeviceHandleImpl::SetInterfaceAlternateSetting(
    int interface_number,
    int alternate_setting,
    ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_ || !base::Contains(claimed_interfaces_, interface_number)) {
    std::move(callback).Run(false);
    return;
  }

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceHandleImpl::SetInterfaceAlternateSettingBlocking,
                     this, interface_number, alternate_setting,
                     std::move(callback)));
}

void UsbDeviceHandleImpl::ResetDevice(ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbDeviceHandleImpl::ResetDeviceBlocking, this,
                                std::move(callback)));
}

void UsbDeviceHandleImpl::ClearHalt(UsbTransferDirection direction,
                                    uint8_t endpoint_number,
                                    ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  uint8_t endpoint_address =
      ConvertTransferDirection(direction) | endpoint_number;

  InterfaceClaimer* interface_claimer =
      GetClaimedInterfaceForEndpoint(endpoint_address).get();
  for (Transfer* transfer : transfers_) {
    if (transfer->claimed_interface() == interface_claimer) {
      transfer->Cancel();
    }
  }

  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbDeviceHandleImpl::ClearHaltBlocking, this,
                                endpoint_address, std::move(callback)));
}

void UsbDeviceHandleImpl::ControlTransfer(
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
                                  UsbTransferStatus::DISCONNECT, buffer, 0));
    return;
  }

  if (!base::IsValueInRangeForNumericType<uint16_t>(buffer->size())) {
    USB_LOG(USER) << "Transfer too long.";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       buffer, 0));
    return;
  }

  const size_t resized_length = LIBUSB_CONTROL_SETUP_SIZE + buffer->size();
  auto resized_buffer =
      base::MakeRefCounted<base::RefCountedBytes>(resized_length);
  base::span(resized_buffer->as_vector())
      .subspan(LIBUSB_CONTROL_SETUP_SIZE)
      .copy_from(base::span(*buffer));

  std::unique_ptr<Transfer> transfer = Transfer::CreateControlTransfer(
      this, CreateRequestType(direction, request_type, recipient), request,
      value, index, static_cast<uint16_t>(buffer->size()), resized_buffer,
      timeout, &callback);
  if (!transfer) {
    DCHECK(callback);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       buffer, 0));
    return;
  }

  SubmitTransfer(std::move(transfer));
}

void UsbDeviceHandleImpl::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   UsbTransferStatus::DISCONNECT);
    return;
  }

  uint8_t endpoint_address =
      ConvertTransferDirection(UsbTransferDirection::INBOUND) | endpoint_number;
  size_t length =
      std::accumulate(packet_lengths.begin(), packet_lengths.end(), 0u);
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(length);
  std::unique_ptr<Transfer> transfer = Transfer::CreateIsochronousTransfer(
      this, endpoint_address, buffer, length, packet_lengths, timeout,
      &callback);
  DCHECK(transfer);
  SubmitTransfer(std::move(transfer));
}

void UsbDeviceHandleImpl::IsochronousTransferOut(
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    ReportIsochronousTransferError(std::move(callback), packet_lengths,
                                   UsbTransferStatus::DISCONNECT);
    return;
  }

  uint8_t endpoint_address =
      ConvertTransferDirection(UsbTransferDirection::OUTBOUND) |
      endpoint_number;
  size_t length =
      std::accumulate(packet_lengths.begin(), packet_lengths.end(), 0u);
  std::unique_ptr<Transfer> transfer = Transfer::CreateIsochronousTransfer(
      this, endpoint_address, buffer, length, packet_lengths, timeout,
      &callback);
  DCHECK(transfer);
  SubmitTransfer(std::move(transfer));
}

void UsbDeviceHandleImpl::GenericTransfer(
    UsbTransferDirection direction,
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  UsbTransferStatus::DISCONNECT, buffer, 0));
    return;
  }

  uint8_t endpoint_address =
      ConvertTransferDirection(direction) | endpoint_number;
  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it == endpoint_map_.end()) {
    USB_LOG(DEBUG) << "Failed to submit transfer because endpoint "
                   << static_cast<int>(endpoint_address)
                   << " not part of a claimed interface.";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       buffer, 0));
    return;
  }

  if (!base::IsValueInRangeForNumericType<int>(buffer->size())) {
    USB_LOG(DEBUG) << "Transfer too long.";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       buffer, 0));
    return;
  }

  std::unique_ptr<Transfer> transfer;
  UsbTransferType transfer_type = endpoint_it->second.endpoint->type;
  if (transfer_type == UsbTransferType::BULK) {
    transfer = Transfer::CreateBulkTransfer(this, endpoint_address, buffer,
                                            static_cast<int>(buffer->size()),
                                            timeout, &callback);
  } else if (transfer_type == UsbTransferType::INTERRUPT) {
    transfer = Transfer::CreateInterruptTransfer(
        this, endpoint_address, buffer, static_cast<int>(buffer->size()),
        timeout, &callback);
  } else {
    USB_LOG(DEBUG) << "Endpoint " << static_cast<int>(endpoint_address)
                   << " is not a bulk or interrupt endpoint.";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       buffer, 0));
    return;
  }
  DCHECK(transfer);
  SubmitTransfer(std::move(transfer));
}

const mojom::UsbInterfaceInfo* UsbDeviceHandleImpl::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it != endpoint_map_.end())
    return endpoint_it->second.interface;
  return nullptr;
}

UsbDeviceHandleImpl::UsbDeviceHandleImpl(
    scoped_refptr<UsbDeviceImpl> device,
    ScopedLibusbDeviceHandle handle,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : device_(std::move(device)),
      handle_(std::move(handle)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      blocking_task_runner_(blocking_task_runner) {
  DCHECK(handle_.IsValid()) << "Cannot create device with an invalid handle.";
}

UsbDeviceHandleImpl::~UsbDeviceHandleImpl() {
  DCHECK(!device_) << "UsbDeviceHandle must be closed before it is destroyed.";

  // This class is RefCountedThreadSafe and so the destructor may be called on
  // any thread. libusb is not safe to reentrancy so be sure not to try to close
  // the device from inside a transfer completion callback.
  blocking_task_runner_->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(handle_)));
}

void UsbDeviceHandleImpl::SetConfigurationBlocking(int configuration_value,
                                                   ResultCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rv = libusb_set_configuration(handle(), configuration_value);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to set configuration " << configuration_value
                   << ": " << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceHandleImpl::SetConfigurationComplete, this,
                     rv == LIBUSB_SUCCESS, std::move(callback)));
}

void UsbDeviceHandleImpl::SetConfigurationComplete(bool success,
                                                   ResultCallback callback) {
  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  if (success) {
    device_->RefreshActiveConfiguration();
    RefreshEndpointMap();
  }
  std::move(callback).Run(success);
}

void UsbDeviceHandleImpl::ClaimInterfaceBlocking(int interface_number,
                                                 ResultCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rv = libusb_claim_interface(handle(), interface_number);
  scoped_refptr<InterfaceClaimer> interface_claimer;
  if (rv == LIBUSB_SUCCESS) {
    interface_claimer =
        new InterfaceClaimer(this, interface_number, task_runner_);
  } else {
    USB_LOG(EVENT) << "Failed to claim interface: "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbDeviceHandleImpl::ClaimInterfaceComplete,
                                this, interface_claimer, std::move(callback)));
}

void UsbDeviceHandleImpl::ClaimInterfaceComplete(
    scoped_refptr<InterfaceClaimer> interface_claimer,
    ResultCallback callback) {
  if (!device_) {
    if (interface_claimer) {
      // Ensure that the InterfaceClaimer is released on the blocking thread.
      blocking_task_runner_->ReleaseSoon(FROM_HERE,
                                         std::move(interface_claimer));
    }

    std::move(callback).Run(false);
    return;
  }

  if (interface_claimer) {
    claimed_interfaces_[interface_claimer->interface_number()] =
        interface_claimer;
    RefreshEndpointMap();
  }
  std::move(callback).Run(interface_claimer != nullptr);
}

void UsbDeviceHandleImpl::SetInterfaceAlternateSettingBlocking(
    int interface_number,
    int alternate_setting,
    ResultCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rv = libusb_set_interface_alt_setting(handle(), interface_number,
                                            alternate_setting);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to set interface " << interface_number
                   << " to alternate setting " << alternate_setting << ": "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceHandleImpl::SetInterfaceAlternateSettingComplete,
                     this, interface_number, alternate_setting,
                     rv == LIBUSB_SUCCESS, std::move(callback)));
}

void UsbDeviceHandleImpl::SetInterfaceAlternateSettingComplete(
    int interface_number,
    int alternate_setting,
    bool success,
    ResultCallback callback) {
  if (!device_) {
    std::move(callback).Run(false);
    return;
  }

  if (success) {
    claimed_interfaces_[interface_number]->set_alternate_setting(
        alternate_setting);
    RefreshEndpointMap();
  }
  std::move(callback).Run(success);
}

void UsbDeviceHandleImpl::ResetDeviceBlocking(ResultCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rv = libusb_reset_device(handle());
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to reset device: "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), rv == LIBUSB_SUCCESS));
}

void UsbDeviceHandleImpl::ClearHaltBlocking(uint8_t endpoint_address,
                                            ResultCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rv = libusb_clear_halt(handle(), endpoint_address);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to clear halt: "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), rv == LIBUSB_SUCCESS));
}

void UsbDeviceHandleImpl::RefreshEndpointMap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(device_);
  endpoint_map_.clear();
  const mojom::UsbConfigurationInfo* config = device_->GetActiveConfiguration();
  if (!config)
    return;

  for (const auto& map_entry : claimed_interfaces_) {
    CombinedInterfaceInfo interface_info = FindInterfaceInfoFromConfig(
        config, map_entry.first, map_entry.second->alternate_setting());

    if (!interface_info.IsValid())
      return;

    for (const auto& endpoint : interface_info.alternate->endpoints) {
      endpoint_map_[ConvertEndpointNumberToAddress(*endpoint)] = {
          interface_info.interface.get(), endpoint.get()};
    }
  }
}

scoped_refptr<UsbDeviceHandleImpl::InterfaceClaimer>
UsbDeviceHandleImpl::GetClaimedInterfaceForEndpoint(uint8_t endpoint_address) {
  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it != endpoint_map_.end())
    return claimed_interfaces_[endpoint_it->second.interface->interface_number];
  return nullptr;
}

void UsbDeviceHandleImpl::ReportIsochronousTransferError(
    UsbDeviceHandle::IsochronousTransferCallback callback,
    const std::vector<uint32_t>& packet_lengths,
    UsbTransferStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<UsbIsochronousPacketPtr> packets(packet_lengths.size());
  for (size_t i = 0; i < packet_lengths.size(); ++i) {
    packets[i] = mojom::UsbIsochronousPacket::New();
    packets[i]->length = packet_lengths[i];
    packets[i]->transferred_length = 0;
    packets[i]->status = status;
  }
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), nullptr,
                                                   std::move(packets)));
}

void UsbDeviceHandleImpl::SubmitTransfer(std::unique_ptr<Transfer> transfer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transfer);

  // Transfer is owned by libusb until its completion callback is run. This
  // object holds a weak reference.
  transfers_.insert(transfer.get());
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Transfer::Submit, base::Unretained(transfer.release())));
}

void UsbDeviceHandleImpl::TransferComplete(Transfer* transfer,
                                           base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(transfers_, transfer)) << "Missing transfer completed";
  transfers_.erase(transfer);

  std::move(callback).Run();

  // libusb_free_transfer races with libusb_submit_transfer and only work-
  // around is to make sure to call them on the same thread.
  blocking_task_runner_->DeleteSoon(FROM_HERE, transfer);
}

}  // namespace device
