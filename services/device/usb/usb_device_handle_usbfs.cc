// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_usbfs.h"

#include <linux/usb/ch9.h>

#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>

#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_device_linux.h"

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbIsochronousPacketPtr;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;
using mojom::UsbTransferType;

namespace {

uint8_t ConvertEndpointDirection(UsbTransferDirection direction) {
  switch (direction) {
    case UsbTransferDirection::INBOUND:
      return USB_DIR_IN;
    case UsbTransferDirection::OUTBOUND:
      return USB_DIR_OUT;
  }
  NOTREACHED();
  return 0;
}

uint8_t ConvertRequestType(UsbControlTransferType request_type) {
  switch (request_type) {
    case UsbControlTransferType::STANDARD:
      return USB_TYPE_STANDARD;
    case UsbControlTransferType::CLASS:
      return USB_TYPE_CLASS;
    case UsbControlTransferType::VENDOR:
      return USB_TYPE_VENDOR;
    case UsbControlTransferType::RESERVED:
      return USB_TYPE_RESERVED;
  }
  NOTREACHED();
  return 0;
}

uint8_t ConvertRecipient(UsbControlTransferRecipient recipient) {
  switch (recipient) {
    case UsbControlTransferRecipient::DEVICE:
      return USB_RECIP_DEVICE;
    case UsbControlTransferRecipient::INTERFACE:
      return USB_RECIP_INTERFACE;
    case UsbControlTransferRecipient::ENDPOINT:
      return USB_RECIP_ENDPOINT;
    case UsbControlTransferRecipient::OTHER:
      return USB_RECIP_OTHER;
  }
  NOTREACHED();
  return 0;
}

scoped_refptr<base::RefCountedBytes> BuildControlTransferBuffer(
    UsbTransferDirection direction,
    UsbControlTransferType request_type,
    UsbControlTransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<base::RefCountedBytes> original_buffer) {
  auto new_buffer = base::MakeRefCounted<base::RefCountedBytes>(
      original_buffer->size() + sizeof(usb_ctrlrequest));
  usb_ctrlrequest* setup = new_buffer->front_as<usb_ctrlrequest>();
  setup->bRequestType = ConvertEndpointDirection(direction) |
                        ConvertRequestType(request_type) |
                        ConvertRecipient(recipient);
  setup->bRequest = request;
  setup->wValue = value;
  setup->wIndex = index;
  setup->wLength = original_buffer->size();
  memcpy(new_buffer->front() + sizeof(usb_ctrlrequest),
         original_buffer->front(), original_buffer->size());
  return new_buffer;
}

uint8_t ConvertTransferType(UsbTransferType type) {
  switch (type) {
    case UsbTransferType::CONTROL:
      return USBDEVFS_URB_TYPE_CONTROL;
    case UsbTransferType::ISOCHRONOUS:
      return USBDEVFS_URB_TYPE_ISO;
    case UsbTransferType::BULK:
      return USBDEVFS_URB_TYPE_BULK;
    case UsbTransferType::INTERRUPT:
      return USBDEVFS_URB_TYPE_INTERRUPT;
  }
  NOTREACHED();
  return 0;
}

UsbTransferStatus ConvertTransferResult(int rc) {
  switch (rc) {
    case 0:
      return UsbTransferStatus::COMPLETED;
    case EOVERFLOW:
      return UsbTransferStatus::BABBLE;
    case EPIPE:
      return UsbTransferStatus::STALLED;
    default:
      // Other errors are difficult to map to UsbTransferStatus and may be
      // emitted in situations that vary by host controller. Log the specific
      // error and return a generic one.
      USB_LOG(ERROR) << "Low-level transfer error: "
                     << logging::SystemErrorCodeToString(rc);
      return UsbTransferStatus::TRANSFER_ERROR;
  }
}

}  // namespace

class UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper {
 public:
  BlockingTaskRunnerHelper(
      base::ScopedFD fd,
      scoped_refptr<UsbDeviceHandleUsbfs> device_handle,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~BlockingTaskRunnerHelper();

  void Start();
  void ReleaseFileDescriptor();

  void SetConfiguration(int configuration_value, ResultCallback callback);
  void ReleaseInterface(int interface_number, ResultCallback callback);
  void SetInterface(int interface_number,
                    int alternate_setting,
                    ResultCallback callback);
  void ResetDevice(ResultCallback callback);
  void ClearHalt(uint8_t endpoint_address, ResultCallback callback);
  void DiscardUrb(Transfer* transfer);

 private:
  // Called when |fd_| is writable without blocking.
  void OnFileCanWriteWithoutBlocking();

  base::ScopedFD fd_;
  scoped_refptr<UsbDeviceHandleUsbfs> device_handle_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watch_controller_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(BlockingTaskRunnerHelper);
};

struct UsbDeviceHandleUsbfs::Transfer {
  Transfer() = delete;
  Transfer(scoped_refptr<base::RefCountedBytes> buffer,
           TransferCallback callback);
  Transfer(scoped_refptr<base::RefCountedBytes> buffer,
           IsochronousTransferCallback callback);
  ~Transfer();

  void* operator new(std::size_t size, size_t number_of_iso_packets);
  void RunCallback(UsbTransferStatus status, size_t bytes_transferred);
  void RunIsochronousCallback(std::vector<UsbIsochronousPacketPtr> packets);

  scoped_refptr<base::RefCountedBytes> control_transfer_buffer;
  scoped_refptr<base::RefCountedBytes> buffer;
  base::CancelableClosure timeout_closure;
  bool cancelled = false;

  // When the URB is |cancelled| these two flags track whether the URB has both
  // been |discarded| and |reaped| since the possiblity of last-minute
  // completion makes these two conditions race.
  bool discarded = false;
  bool reaped = false;

  TransferCallback callback;
  IsochronousTransferCallback isoc_callback;

 private:
  DISALLOW_COPY_AND_ASSIGN(Transfer);

 public:
  // The |urb| field must be the last in the struct so that the extra space
  // allocated by the overridden new function above extends the length of its
  // |iso_frame_desc| field.
  usbdevfs_urb urb;
};

UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper(
    base::ScopedFD fd,
    scoped_refptr<UsbDeviceHandleUsbfs> device_handle,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : fd_(std::move(fd)),
      device_handle_(std::move(device_handle)),
      task_runner_(std::move(task_runner)) {}

UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::Start() {
  sequence_checker_.DetachFromSequence();
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // Linux indicates that URBs are available to reap by marking the file
  // descriptor writable.
  watch_controller_ = base::FileDescriptorWatcher::WatchWritable(
      fd_.get(), base::BindRepeating(
                     &BlockingTaskRunnerHelper::OnFileCanWriteWithoutBlocking,
                     base::Unretained(this)));
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ReleaseFileDescriptor() {
  // This method intentionally leaks the file descriptor.
  DCHECK(sequence_checker_.CalledOnValidSequence());
  watch_controller_.reset();
  ignore_result(fd_.release());
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::SetConfiguration(
    int configuration_value,
    ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rc = HANDLE_EINTR(
      ioctl(fd_.get(), USBDEVFS_SETCONFIGURATION, &configuration_value));
  if (rc)
    USB_PLOG(DEBUG) << "Failed to set configuration " << configuration_value;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbDeviceHandleUsbfs::SetConfigurationComplete,
                                device_handle_, configuration_value, rc == 0,
                                std::move(callback)));
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ReleaseInterface(
    int interface_number,
    ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rc = HANDLE_EINTR(
      ioctl(fd_.get(), USBDEVFS_RELEASEINTERFACE, &interface_number));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to release interface " << interface_number;
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UsbDeviceHandleUsbfs::ReleaseInterfaceComplete,
                       device_handle_, interface_number, std::move(callback)));
  }
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::SetInterface(
    int interface_number,
    int alternate_setting,
    ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  usbdevfs_setinterface cmd = {0};
  cmd.interface = interface_number;
  cmd.altsetting = alternate_setting;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_SETINTERFACE, &cmd));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to set interface " << interface_number
                    << " to alternate setting " << alternate_setting;
  }
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), rc == 0));
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ResetDevice(
    ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // TODO(reillyg): libusb releases interfaces before and then reclaims
  // interfaces after a reset. We should probably do this too or document that
  // callers have to call ClaimInterface as well.
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_RESET, nullptr));
  if (rc)
    USB_PLOG(DEBUG) << "Failed to reset the device";
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), rc == 0));
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ClearHalt(
    uint8_t endpoint_address,
    ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  int tmp_endpoint = endpoint_address;
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_CLEAR_HALT, &tmp_endpoint));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to clear the stall condition on endpoint "
                    << static_cast<int>(endpoint_address);
  }
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), rc == 0));
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::DiscardUrb(
    Transfer* transfer) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_DISCARDURB, &transfer->urb));

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&UsbDeviceHandleUsbfs::UrbDiscarded,
                                        device_handle_, transfer));
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::
    OnFileCanWriteWithoutBlocking() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  const size_t MAX_URBS_PER_EVENT = 10;
  std::vector<usbdevfs_urb*> urbs;
  urbs.reserve(MAX_URBS_PER_EVENT);
  for (size_t i = 0; i < MAX_URBS_PER_EVENT; ++i) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    usbdevfs_urb* urb;
    int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_REAPURBNDELAY, &urb));
    if (rc) {
      if (errno == EAGAIN)
        break;
      USB_PLOG(DEBUG) << "Failed to reap urbs";
      if (errno == ENODEV) {
        // Device has disconnected. Stop watching the file descriptor to avoid
        // looping until |device_handle_| is closed.
        watch_controller_.reset();
        break;
      }
    } else {
      urbs.push_back(urb);
    }
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceHandleUsbfs::ReapedUrbs, device_handle_, urbs));
}

UsbDeviceHandleUsbfs::Transfer::Transfer(
    scoped_refptr<base::RefCountedBytes> buffer,
    TransferCallback callback)
    : buffer(buffer), callback(std::move(callback)) {
  memset(&urb, 0, sizeof(urb));
  urb.usercontext = this;
  urb.buffer = buffer->front();
}

UsbDeviceHandleUsbfs::Transfer::Transfer(
    scoped_refptr<base::RefCountedBytes> buffer,
    IsochronousTransferCallback callback)
    : buffer(buffer), isoc_callback(std::move(callback)) {
  memset(
      &urb, 0,
      sizeof(urb) + sizeof(usbdevfs_iso_packet_desc) * urb.number_of_packets);
  urb.usercontext = this;
  urb.buffer = buffer->front();
}

UsbDeviceHandleUsbfs::Transfer::~Transfer() = default;

void* UsbDeviceHandleUsbfs::Transfer::operator new(
    std::size_t size,
    size_t number_of_iso_packets) {
  void* p = ::operator new(size + sizeof(usbdevfs_iso_packet_desc) *
                                      number_of_iso_packets);
  Transfer* transfer = static_cast<Transfer*>(p);
  transfer->urb.number_of_packets = number_of_iso_packets;
  return p;
}

void UsbDeviceHandleUsbfs::Transfer::RunCallback(UsbTransferStatus status,
                                                 size_t bytes_transferred) {
  DCHECK_NE(urb.type, USBDEVFS_URB_TYPE_ISO);
  DCHECK(callback);
  std::move(callback).Run(status, buffer, bytes_transferred);
}

void UsbDeviceHandleUsbfs::Transfer::RunIsochronousCallback(
    std::vector<UsbIsochronousPacketPtr> packets) {
  DCHECK_EQ(urb.type, USBDEVFS_URB_TYPE_ISO);
  DCHECK(isoc_callback);
  std::move(isoc_callback).Run(buffer, std::move(packets));
}

UsbDeviceHandleUsbfs::UsbDeviceHandleUsbfs(
    scoped_refptr<UsbDevice> device,
    base::ScopedFD fd,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : device_(device),
      fd_(fd.get()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(blocking_task_runner) {
  DCHECK(device_);
  DCHECK(fd.is_valid());
  DCHECK(blocking_task_runner_);

  helper_.reset(
      new BlockingTaskRunnerHelper(std::move(fd), this, task_runner_));
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskRunnerHelper::Start,
                                base::Unretained(helper_.get())));
}

scoped_refptr<UsbDevice> UsbDeviceHandleUsbfs::GetDevice() const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return device_;
}

void UsbDeviceHandleUsbfs::Close() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_)
    return;  // Already closed.

  // Cancelling transfers may run or destroy callbacks holding the last
  // reference to this object so hold a reference for the rest of this method.
  scoped_refptr<UsbDeviceHandleUsbfs> self(this);
  for (const auto& transfer : transfers_)
    CancelTransfer(transfer.get(), UsbTransferStatus::CANCELLED);

  // On the |task_runner_| thread check |device_| to see if the handle is
  // closed. On the |blocking_task_runner_| thread check |fd_.is_valid()| to
  // see if the handle is closed.
  device_->HandleClosed(this);
  device_ = nullptr;
  // The device is no longer attached so we don't have any endpoints either.
  endpoints_.clear();

  // Releases |helper_|.
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbDeviceHandleUsbfs::CloseBlocking, this));
}

void UsbDeviceHandleUsbfs::SetConfiguration(int configuration_value,
                                            ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_SETCONFIGURATION synchronously issues a SET_CONFIGURATION request
  // to the device so it must be performed on a thread where it is okay to
  // block.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::SetConfiguration,
          base::Unretained(helper_.get()), configuration_value,
          std::move(callback)));
}

void UsbDeviceHandleUsbfs::ClaimInterface(int interface_number,
                                          ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  if (base::Contains(interfaces_, interface_number)) {
    USB_LOG(DEBUG) << "Interface " << interface_number << " already claimed.";
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // It appears safe to assume that this ioctl will not block.
  int rc = HANDLE_EINTR(ioctl(fd_, USBDEVFS_CLAIMINTERFACE, &interface_number));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to claim interface " << interface_number;
  } else {
    interfaces_[interface_number].alternate_setting = 0;
    RefreshEndpointInfo();
  }
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), rc == 0));
}

void UsbDeviceHandleUsbfs::ReleaseInterface(int interface_number,
                                            ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_RELEASEINTERFACE may issue a SET_INTERFACE request to the
  // device to restore alternate setting 0 so it must be performed on a thread
  // where it is okay to block.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ReleaseInterface,
          base::Unretained(helper_.get()), interface_number,
          std::move(callback)));
}

void UsbDeviceHandleUsbfs::SetInterfaceAlternateSetting(
    int interface_number,
    int alternate_setting,
    ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_SETINTERFACE is synchronous because it issues a SET_INTERFACE
  // request to the device so it must be performed on a thread where it is okay
  // to block.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::SetInterface,
          base::Unretained(helper_.get()), interface_number, alternate_setting,
          std::move(callback)));
}

void UsbDeviceHandleUsbfs::ResetDevice(ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_RESET is synchronous because it waits for the port to be reset
  // and the device re-enumerated so it must be performed on a thread where it
  // is okay to block.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ResetDevice,
          base::Unretained(helper_.get()), std::move(callback)));
}

void UsbDeviceHandleUsbfs::ClearHalt(uint8_t endpoint_address,
                                     ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_CLEAR_HALT is synchronous because it issues a CLEAR_FEATURE
  // request to the device so it must be performed on a thread where it is okay
  // to block.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ClearHalt,
                     base::Unretained(helper_.get()), endpoint_address,
                     std::move(callback)));
}

void UsbDeviceHandleUsbfs::ControlTransfer(
    UsbTransferDirection direction,
    UsbControlTransferType request_type,
    UsbControlTransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  UsbTransferStatus::DISCONNECT, nullptr, 0));
    return;
  }

  std::unique_ptr<Transfer> transfer(new (0)
                                         Transfer(buffer, std::move(callback)));
  transfer->control_transfer_buffer = BuildControlTransferBuffer(
      direction, request_type, recipient, request, value, index, buffer);
  transfer->urb.type = USBDEVFS_URB_TYPE_CONTROL;
  transfer->urb.endpoint = 0;
  transfer->urb.buffer = transfer->control_transfer_buffer->front();
  transfer->urb.buffer_length = transfer->control_transfer_buffer->size();

  // USBDEVFS_SUBMITURB appears to be non-blocking as completion is reported
  // by USBDEVFS_REAPURBNDELAY.
  int rc = HANDLE_EINTR(ioctl(fd_, USBDEVFS_SUBMITURB, &transfer->urb));
  if (rc) {
    rc = logging::GetLastSystemErrorCode();
    USB_PLOG(DEBUG) << "Failed to submit control transfer";
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(transfer->callback),
                                  ConvertTransferResult(rc), nullptr, 0));
  } else {
    SetUpTimeoutCallback(transfer.get(), timeout);
    transfers_.push_back(std::move(transfer));
  }
}

void UsbDeviceHandleUsbfs::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  uint8_t endpoint_address = USB_DIR_IN | endpoint_number;
  size_t total_length =
      std::accumulate(packet_lengths.begin(), packet_lengths.end(), 0u);
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(total_length);
  IsochronousTransferInternal(endpoint_address, buffer, total_length,
                              packet_lengths, timeout, std::move(callback));
}

void UsbDeviceHandleUsbfs::IsochronousTransferOut(
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  uint8_t endpoint_address = USB_DIR_OUT | endpoint_number;
  size_t total_length =
      std::accumulate(packet_lengths.begin(), packet_lengths.end(), 0u);
  IsochronousTransferInternal(endpoint_address, buffer, total_length,
                              packet_lengths, timeout, std::move(callback));
}

void UsbDeviceHandleUsbfs::GenericTransfer(
    UsbTransferDirection direction,
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  UsbTransferStatus::DISCONNECT, nullptr, 0));
    return;
  }

  uint8_t endpoint_address =
      ConvertEndpointDirection(direction) | endpoint_number;
  auto it = endpoints_.find(endpoint_address);
  if (it == endpoints_.end()) {
    USB_LOG(USER) << "Endpoint address " << static_cast<int>(endpoint_address)
                  << " is not part of a claimed interface.";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       nullptr, 0));
    return;
  }

  std::unique_ptr<Transfer> transfer(new (0)
                                         Transfer(buffer, std::move(callback)));
  transfer->urb.endpoint = endpoint_address;
  transfer->urb.buffer_length = buffer->size();
  transfer->urb.type = ConvertTransferType(it->second.type);

  // USBDEVFS_SUBMITURB appears to be non-blocking as completion is reported
  // by USBDEVFS_REAPURBNDELAY. This code assumes a recent kernel that can
  // accept arbitrarily large transfer requests, hopefully also using a scatter-
  // gather list.
  int rc = HANDLE_EINTR(ioctl(fd_, USBDEVFS_SUBMITURB, &transfer->urb));
  if (rc) {
    rc = logging::GetLastSystemErrorCode();
    USB_PLOG(DEBUG) << "Failed to submit transfer";
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(transfer->callback),
                                  ConvertTransferResult(rc), nullptr, 0));
  } else {
    SetUpTimeoutCallback(transfer.get(), timeout);
    transfers_.push_back(std::move(transfer));
  }
}

const mojom::UsbInterfaceInfo* UsbDeviceHandleUsbfs::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  auto it = endpoints_.find(endpoint_address);
  if (it != endpoints_.end())
    return it->second.interface;
  return nullptr;
}

UsbDeviceHandleUsbfs::~UsbDeviceHandleUsbfs() {
  DCHECK(!device_) << "Handle must be closed before it is destroyed.";
}

void UsbDeviceHandleUsbfs::ReleaseFileDescriptor() {
  // Calls to this method must be posted to |blocking_task_runner_|.
  helper_->ReleaseFileDescriptor();
  helper_.reset();
}

void UsbDeviceHandleUsbfs::CloseBlocking() {
  // Calls to this method must be posted to |blocking_task_runner_|.
  helper_.reset();
}

void UsbDeviceHandleUsbfs::SetConfigurationComplete(int configuration_value,
                                                    bool success,
                                                    ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (success && device_) {
    device_->ActiveConfigurationChanged(configuration_value);
    // TODO(reillyg): If all interfaces are unclaimed before a new configuration
    // is set then this will do nothing. Investigate.
    RefreshEndpointInfo();
  }
  std::move(callback).Run(success);
}

void UsbDeviceHandleUsbfs::ReleaseInterfaceComplete(int interface_number,
                                                    ResultCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  auto it = interfaces_.find(interface_number);
  DCHECK(it != interfaces_.end());
  interfaces_.erase(it);
  if (device_) {
    // Only refresh endpoints if a device is still attached.
    RefreshEndpointInfo();
  }
  std::move(callback).Run(true);
}

void UsbDeviceHandleUsbfs::IsochronousTransferInternal(
    uint8_t endpoint_address,
    scoped_refptr<base::RefCountedBytes> buffer,
    size_t total_length,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!device_) {
    ReportIsochronousError(packet_lengths, std::move(callback),
                           UsbTransferStatus::DISCONNECT);
    return;
  }

  auto it = endpoints_.find(endpoint_address);
  if (it == endpoints_.end()) {
    USB_LOG(USER) << "Endpoint address " << static_cast<int>(endpoint_address)
                  << " is not part of a claimed interface.";
    ReportIsochronousError(packet_lengths, std::move(callback),
                           UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  DCHECK_GE(buffer->size(), total_length);
  std::unique_ptr<Transfer> transfer(new (packet_lengths.size())
                                         Transfer(buffer, std::move(callback)));
  transfer->urb.type = USBDEVFS_URB_TYPE_ISO;
  transfer->urb.endpoint = endpoint_address;
  transfer->urb.buffer_length = total_length;

  for (size_t i = 0; i < packet_lengths.size(); ++i)
    transfer->urb.iso_frame_desc[i].length = packet_lengths[i];

  // USBDEVFS_SUBMITURB appears to be non-blocking as completion is reported
  // by USBDEVFS_REAPURBNDELAY. This code assumes a recent kernel that can
  // accept arbitrarily large transfer requests, hopefully also using a scatter-
  // gather list.
  int rc = HANDLE_EINTR(ioctl(fd_, USBDEVFS_SUBMITURB, &transfer->urb));
  if (rc) {
    rc = logging::GetLastSystemErrorCode();
    USB_PLOG(DEBUG) << "Failed to submit transfer";
    ReportIsochronousError(packet_lengths, std::move(transfer->isoc_callback),
                           ConvertTransferResult(rc));
  } else {
    SetUpTimeoutCallback(transfer.get(), timeout);
    transfers_.push_back(std::move(transfer));
  }
}

void UsbDeviceHandleUsbfs::ReapedUrbs(const std::vector<usbdevfs_urb*>& urbs) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  for (auto* urb : urbs) {
    Transfer* transfer = static_cast<Transfer*>(urb->usercontext);
    DCHECK_EQ(urb, &transfer->urb);

    if (transfer->cancelled) {
      transfer->reaped = true;
      if (transfer->discarded)
        RemoveFromTransferList(transfer);
    } else {
      TransferComplete(RemoveFromTransferList(transfer));
    }
  }
}

void UsbDeviceHandleUsbfs::TransferComplete(
    std::unique_ptr<Transfer> transfer) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (transfer->cancelled)
    return;

  // The transfer will soon be freed. Cancel the timeout callback so that the
  // raw pointer it holds to |transfer| is not used.
  transfer->timeout_closure.Cancel();

  if (transfer->urb.type == USBDEVFS_URB_TYPE_ISO) {
    std::vector<UsbIsochronousPacketPtr> packets(
        transfer->urb.number_of_packets);
    for (size_t i = 0; i < packets.size(); ++i) {
      packets[i] = mojom::UsbIsochronousPacket::New();
      packets[i]->length = transfer->urb.iso_frame_desc[i].length;
      packets[i]->transferred_length =
          transfer->urb.iso_frame_desc[i].actual_length;
      packets[i]->status = ConvertTransferResult(
          transfer->urb.status == 0 ? transfer->urb.iso_frame_desc[i].status
                                    : transfer->urb.status);
    }

    transfer->RunIsochronousCallback(std::move(packets));
  } else {
    if (transfer->urb.status == 0 &&
        transfer->urb.type == USBDEVFS_URB_TYPE_CONTROL) {
      // Copy the result of the control transfer back into the original buffer.
      memcpy(transfer->buffer->front(),
             transfer->control_transfer_buffer->front() + 8,
             transfer->urb.actual_length);
    }

    transfer->RunCallback(ConvertTransferResult(-transfer->urb.status),
                          transfer->urb.actual_length);
  }
}

void UsbDeviceHandleUsbfs::RefreshEndpointInfo() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(device_);
  endpoints_.clear();

  const mojom::UsbConfigurationInfo* config = device_->GetActiveConfiguration();
  if (!config)
    return;

  for (const auto& entry : interfaces_) {
    CombinedInterfaceInfo interface = FindInterfaceInfoFromConfig(
        config, entry.first, entry.second.alternate_setting);

    DCHECK(interface.IsValid());

    for (const auto& endpoint : interface.alternate->endpoints) {
      EndpointInfo& info =
          endpoints_[ConvertEndpointNumberToAddress(*endpoint)];
      info.type = endpoint->type;
      info.interface = interface.interface;
    }
  }
}

void UsbDeviceHandleUsbfs::ReportIsochronousError(
    const std::vector<uint32_t>& packet_lengths,
    UsbDeviceHandle::IsochronousTransferCallback callback,
    UsbTransferStatus status) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
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

void UsbDeviceHandleUsbfs::SetUpTimeoutCallback(Transfer* transfer,
                                                unsigned int timeout) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (timeout == 0)
    return;

  transfer->timeout_closure.Reset(
      base::Bind(&UsbDeviceHandleUsbfs::OnTimeout, this, transfer));
  task_runner_->PostDelayedTask(FROM_HERE, transfer->timeout_closure.callback(),
                                base::TimeDelta::FromMilliseconds(timeout));
}

void UsbDeviceHandleUsbfs::OnTimeout(Transfer* transfer) {
  CancelTransfer(transfer, UsbTransferStatus::TIMEOUT);
}

std::unique_ptr<UsbDeviceHandleUsbfs::Transfer>
UsbDeviceHandleUsbfs::RemoveFromTransferList(Transfer* transfer_ptr) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  auto it = std::find_if(
      transfers_.begin(), transfers_.end(),
      [transfer_ptr](const std::unique_ptr<Transfer>& transfer) -> bool {
        return transfer.get() == transfer_ptr;
      });
  DCHECK(it != transfers_.end());
  std::unique_ptr<Transfer> transfer = std::move(*it);
  transfers_.erase(it);
  return transfer;
}

void UsbDeviceHandleUsbfs::CancelTransfer(Transfer* transfer,
                                          UsbTransferStatus status) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(device_);

  if (transfer->cancelled)
    return;

  // |transfer| must stay in |transfers_| as it is still being processed by the
  // kernel and will be reaped later.
  transfer->cancelled = true;

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::DiscardUrb,
          base::Unretained(helper_.get()), transfer));

  // Cancelling |timeout_closure| and running completion callbacks may free
  // |this| so these operations must be performed at the end of this function.
  transfer->timeout_closure.Cancel();

  if (transfer->urb.type == USBDEVFS_URB_TYPE_ISO) {
    std::vector<UsbIsochronousPacketPtr> packets(
        transfer->urb.number_of_packets);
    for (size_t i = 0; i < packets.size(); ++i) {
      packets[i] = mojom::UsbIsochronousPacket::New();
      packets[i]->length = transfer->urb.iso_frame_desc[i].length;
      packets[i]->transferred_length = 0;
      packets[i]->status = status;
    }
    transfer->RunIsochronousCallback(std::move(packets));
  } else {
    transfer->RunCallback(status, 0);
  }
}

void UsbDeviceHandleUsbfs::UrbDiscarded(Transfer* transfer) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  transfer->discarded = true;
  if (transfer->reaped)
    RemoveFromTransferList(transfer);
}

}  // namespace device
