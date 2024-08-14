// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/usb/usb_device_handle_usbfs.h"

#include <linux/usb/ch9.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>

#include <numeric>
#include <tuple>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/containers/contains.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/not_fatal_until.h"
#include "base/numerics/checked_math.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_device_linux.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#endif

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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
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
  usb_ctrlrequest setup;
  setup.bRequestType = ConvertEndpointDirection(direction) |
                       ConvertRequestType(request_type) |
                       ConvertRecipient(recipient);
  setup.bRequest = request;
  setup.wValue = value;
  setup.wIndex = index;
  setup.wLength = original_buffer->size();
  auto [setup_span, remain] =
      base::span(new_buffer->as_vector()).split_at<sizeof(setup)>();
  setup_span.copy_from(base::byte_span_from_ref(setup));
  remain.copy_from(base::span(*original_buffer));
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
  NOTREACHED_IN_MIGRATION();
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
      base::ScopedFD lifeline_fd,
      base::WeakPtr<UsbDeviceHandleUsbfs> device_handle,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  ~BlockingTaskRunnerHelper();

  void Start();
  void ReleaseFileDescriptor();

  bool SetConfiguration(int configuration_value);
  bool ReleaseInterface(int interface_number);
  bool SetInterface(int interface_number, int alternate_setting);
  bool ResetDevice();
  bool ClearHalt(uint8_t endpoint_address);
  void DiscardUrb(Transfer* transfer);

 private:
  // Called when |fd_| is writable without blocking.
  void OnFileCanWriteWithoutBlocking();

  base::ScopedFD fd_;
  base::ScopedFD lifeline_fd_;
  base::WeakPtr<UsbDeviceHandleUsbfs> device_handle_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watch_controller_;
  SEQUENCE_CHECKER(sequence_checker_);
};

struct UsbDeviceHandleUsbfs::Transfer final {
  Transfer() = delete;
  Transfer(scoped_refptr<base::RefCountedBytes> buffer,
           TransferCallback callback);
  Transfer(scoped_refptr<base::RefCountedBytes> buffer,
           IsochronousTransferCallback callback);

  Transfer(const Transfer&) = delete;
  Transfer& operator=(const Transfer&) = delete;

  ~Transfer();

  void* operator new(std::size_t size, size_t number_of_iso_packets);
  void RunCallback(UsbTransferStatus status, size_t bytes_transferred);
  void RunIsochronousCallback(std::vector<UsbIsochronousPacketPtr> packets);

  scoped_refptr<base::RefCountedBytes> control_transfer_buffer;
  scoped_refptr<base::RefCountedBytes> buffer;
  base::CancelableOnceClosure timeout_closure;
  bool cancelled = false;

  // When the URB is |cancelled| these two flags track whether the URB has both
  // been |discarded| and |reaped| since the possiblity of last-minute
  // completion makes these two conditions race.
  bool discarded = false;
  bool reaped = false;

  TransferCallback callback;
  IsochronousTransferCallback isoc_callback;

 public:
  // The |urb| field must be the last in the struct so that the extra space
  // allocated by the overridden new function above extends the length of its
  // |iso_frame_desc| field.
  usbdevfs_urb urb;
};

UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper(
    base::ScopedFD fd,
    base::ScopedFD lifeline_fd,
    base::WeakPtr<UsbDeviceHandleUsbfs> device_handle,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : fd_(std::move(fd)),
      lifeline_fd_(std::move(lifeline_fd)),
      device_handle_(std::move(device_handle)),
      task_runner_(std::move(task_runner)) {
  // Linux indicates that URBs are available to reap by marking the file
  // descriptor writable.
  watch_controller_ = base::FileDescriptorWatcher::WatchWritable(
      fd_.get(), base::BindRepeating(
                     &BlockingTaskRunnerHelper::OnFileCanWriteWithoutBlocking,
                     base::Unretained(this)));
}

UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ReleaseFileDescriptor() {
  // This method intentionally leaks the file descriptor.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  watch_controller_.reset();
  std::ignore = fd_.release();
}

bool UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::SetConfiguration(
    int configuration_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rc = HANDLE_EINTR(
      ioctl(fd_.get(), USBDEVFS_SETCONFIGURATION, &configuration_value));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to set configuration " << configuration_value;
    return false;
  }

  return true;
}

bool UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ReleaseInterface(
    int interface_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rc = HANDLE_EINTR(
      ioctl(fd_.get(), USBDEVFS_RELEASEINTERFACE, &interface_number));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to release interface " << interface_number;
    return false;
  }

  return true;
}

bool UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::SetInterface(
    int interface_number,
    int alternate_setting) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  usbdevfs_setinterface cmd = {0};
  cmd.interface = interface_number;
  cmd.altsetting = alternate_setting;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_SETINTERFACE, &cmd));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to set interface " << interface_number
                    << " to alternate setting " << alternate_setting;
    return false;
  }

  return true;
}

bool UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ResetDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // TODO(reillyg): libusb releases interfaces before and then reclaims
  // interfaces after a reset. We should probably do this too or document that
  // callers have to call ClaimInterface as well.
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_RESET, nullptr));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to reset the device";
    return false;
  }

  return true;
}

bool UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::ClearHalt(
    uint8_t endpoint_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int tmp_endpoint = endpoint_address;
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_CLEAR_HALT, &tmp_endpoint));
  if (rc) {
    USB_PLOG(DEBUG) << "Failed to clear the stall condition on endpoint "
                    << static_cast<int>(endpoint_address);
    return false;
  }

  return true;
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::DiscardUrb(
    Transfer* transfer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_DISCARDURB, &transfer->urb));
}

void UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper::
    OnFileCanWriteWithoutBlocking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const size_t MAX_URBS_PER_EVENT = 10;
  std::vector<usbdevfs_urb*> urbs;
  urbs.reserve(MAX_URBS_PER_EVENT);
  for (size_t i = 0; i < MAX_URBS_PER_EVENT; ++i) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    usbdevfs_urb* urb = nullptr;
    int rc = HANDLE_EINTR(ioctl(fd_.get(), USBDEVFS_REAPURBNDELAY, &urb));
    if (rc || !urb) {
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
    scoped_refptr<base::RefCountedBytes> in_buffer,
    TransferCallback callback)
    : buffer(std::move(in_buffer)), callback(std::move(callback)) {
  urb.usercontext = this;
  urb.buffer = buffer->as_vector().data();
}

UsbDeviceHandleUsbfs::Transfer::Transfer(
    scoped_refptr<base::RefCountedBytes> in_buffer,
    IsochronousTransferCallback callback)
    : buffer(std::move(in_buffer)), isoc_callback(std::move(callback)) {
  urb.usercontext = this;
  urb.buffer = buffer->as_vector().data();
}

UsbDeviceHandleUsbfs::Transfer::~Transfer() = default;

void* UsbDeviceHandleUsbfs::Transfer::operator new(
    size_t size,
    size_t number_of_iso_packets) {
  // The checked math should pass as long as Mojo message size limits are being
  // enforced.
  size_t total_size =
      base::CheckAdd(size, base::CheckMul(sizeof(urb.iso_frame_desc[0]),
                                          number_of_iso_packets))
          .ValueOrDie();
  void* p = ::operator new(total_size);
  Transfer* transfer = static_cast<Transfer*>(p);
  memset(&transfer->urb, 0,
         sizeof(urb) + sizeof(urb.iso_frame_desc[0]) * number_of_iso_packets);
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
    base::ScopedFD lifeline_fd,
    const std::string& client_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : device_(std::move(device)),
      fd_(fd.get()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(device_);
  DCHECK(fd.is_valid());

  if (!client_id.empty()) {
    client_id_ = client_id;
  }

  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      std::move(blocking_task_runner), std::move(fd), std::move(lifeline_fd),
      weak_factory_.GetWeakPtr(), task_runner_);
}

scoped_refptr<UsbDevice> UsbDeviceHandleUsbfs::GetDevice() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return device_;
}

void UsbDeviceHandleUsbfs::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device_)
    return;  // Already closed.

  // Cancelling transfers may run or destroy callbacks holding the last
  // reference to this object so hold a reference for the rest of this method.
  scoped_refptr<UsbDeviceHandleUsbfs> self(this);
  for (const auto& transfer : transfers_)
    CancelTransfer(transfer.get(), UsbTransferStatus::CANCELLED);

  // On the |task_runner_| thread check |device_| to see if the handle is
  // closed. In |helper_| thread check |fd_.is_valid()| to see if the handle is
  // closed.
  device_->HandleClosed(this);
  device_ = nullptr;
  // The device is no longer attached so we don't have any endpoints either.
  endpoints_.clear();

  // The destruction of the |helper_| below will close the lifeline pipe if it
  // exists and re-attach kernel driver.
  FinishClose();
}

void UsbDeviceHandleUsbfs::SetConfiguration(int configuration_value,
                                            ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_SETCONFIGURATION synchronously issues a SET_CONFIGURATION request
  // to the device so it must be performed on a thread where it is okay to
  // block.
  helper_.AsyncCall(&BlockingTaskRunnerHelper::SetConfiguration)
      .WithArgs(configuration_value)
      .Then(base::BindOnce(&UsbDeviceHandleUsbfs::SetConfigurationComplete,
                           this, configuration_value, std::move(callback)));
}

void UsbDeviceHandleUsbfs::ClaimInterface(int interface_number,
                                          ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

#if BUILDFLAG(IS_CHROMEOS)
  if (client_id_.has_value()) {
    chromeos::PermissionBrokerClient::Get()->DetachInterface(
        client_id_.value(), interface_number,
        base::BindOnce(&UsbDeviceHandleUsbfs::DetachInterfaceComplete, this,
                       interface_number, std::move(callback)));
    return;
  }
#endif
  DetachInterfaceComplete(interface_number, std::move(callback), true);
}

void UsbDeviceHandleUsbfs::ReleaseInterface(int interface_number,
                                            ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_RELEASEINTERFACE may issue a SET_INTERFACE request to the
  // device to restore alternate setting 0 so it must be performed on a thread
  // where it is okay to block.
  helper_.AsyncCall(&BlockingTaskRunnerHelper::ReleaseInterface)
      .WithArgs(interface_number)
      .Then(base::BindOnce(&UsbDeviceHandleUsbfs::ReleaseInterfaceComplete,
                           this, interface_number, std::move(callback)));
}

void UsbDeviceHandleUsbfs::SetInterfaceAlternateSetting(
    int interface_number,
    int alternate_setting,
    ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_SETINTERFACE is synchronous because it issues a SET_INTERFACE
  // request to the device so it must be performed on a thread where it is okay
  // to block.
  helper_.AsyncCall(&BlockingTaskRunnerHelper::SetInterface)
      .WithArgs(interface_number, alternate_setting)
      .Then(base::BindOnce(
          &UsbDeviceHandleUsbfs::SetAlternateInterfaceSettingComplete, this,
          interface_number, alternate_setting, std::move(callback)));
}

void UsbDeviceHandleUsbfs::ResetDevice(ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_RESET is synchronous because it waits for the port to be reset
  // and the device re-enumerated so it must be performed on a thread where it
  // is okay to block.
  helper_.AsyncCall(&BlockingTaskRunnerHelper::ResetDevice)
      .Then(std::move(callback));
}

void UsbDeviceHandleUsbfs::ClearHalt(mojom::UsbTransferDirection direction,
                                     uint8_t endpoint_number,
                                     ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  uint8_t endpoint_address =
      ConvertEndpointDirection(direction) | endpoint_number;
  auto it = endpoints_.find(endpoint_address);
  if (it == endpoints_.end()) {
    USB_LOG(USER) << "Endpoint address " << static_cast<int>(endpoint_address)
                  << " is not part of a claimed interface.";
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // USBDEVFS_CLEAR_HALT is synchronous because it issues a CLEAR_FEATURE
  // request to the device so it must be performed on a thread where it is okay
  // to block.
  helper_.AsyncCall(&BlockingTaskRunnerHelper::ClearHalt)
      .WithArgs(endpoint_address)
      .Then(std::move(callback));
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  transfer->urb.buffer = transfer->control_transfer_buffer->as_vector().data();
  transfer->urb.buffer_length =
      transfer->control_transfer_buffer->as_vector().size();

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = endpoints_.find(endpoint_address);
  if (it != endpoints_.end())
    return it->second.interface;
  return nullptr;
}

UsbDeviceHandleUsbfs::~UsbDeviceHandleUsbfs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!device_) << "Handle must be closed before it is destroyed.";
}

void UsbDeviceHandleUsbfs::ReleaseFileDescriptor(base::OnceClosure callback) {
  helper_.AsyncCall(&BlockingTaskRunnerHelper::ReleaseFileDescriptor)
      .Then(std::move(callback));
  helper_.Reset();
}

void UsbDeviceHandleUsbfs::FinishClose() {
  helper_.Reset();
}

void UsbDeviceHandleUsbfs::SetConfigurationComplete(int configuration_value,
                                                    ResultCallback callback,
                                                    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success && device_) {
    device_->ActiveConfigurationChanged(configuration_value);
    // TODO(reillyg): If all interfaces are unclaimed before a new configuration
    // is set then this will do nothing. Investigate.
    RefreshEndpointInfo();
  }
  std::move(callback).Run(success);
}

void UsbDeviceHandleUsbfs::SetAlternateInterfaceSettingComplete(
    int interface_number,
    int alternate_setting,
    ResultCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success && device_) {
    auto it = interfaces_.find(interface_number);
    if (it != interfaces_.end()) {
      it->second.alternate_setting = alternate_setting;
      RefreshEndpointInfo();
    }
  }
  std::move(callback).Run(success);
}

void UsbDeviceHandleUsbfs::DetachInterfaceComplete(int interface_number,
                                                   ResultCallback callback,
                                                   bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
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

void UsbDeviceHandleUsbfs::ReleaseInterfaceComplete(int interface_number,
                                                    ResultCallback callback,
                                                    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    std::move(callback).Run(false);
    return;
  }

  auto it = interfaces_.find(interface_number);
  CHECK(it != interfaces_.end(), base::NotFatalUntil::M130);
  interfaces_.erase(it);
  if (device_) {
    // Only refresh endpoints if a device is still attached.
    RefreshEndpointInfo();
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (client_id_.has_value()) {
    chromeos::PermissionBrokerClient::Get()->ReattachInterface(
        client_id_.value(), interface_number, std::move(callback));
    return;
  }
#endif
  std::move(callback).Run(true);
}

void UsbDeviceHandleUsbfs::IsochronousTransferInternal(
    uint8_t endpoint_address,
    scoped_refptr<base::RefCountedBytes> buffer,
    size_t total_length,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
      const auto actual_length =
          base::checked_cast<size_t>(transfer->urb.actual_length);
      base::span(transfer->buffer->as_vector())
          .copy_prefix_from(base::span(*transfer->control_transfer_buffer)
                                .subspan(8u, actual_length));
    }

    transfer->RunCallback(ConvertTransferResult(-transfer->urb.status),
                          transfer->urb.actual_length);
  }
}

void UsbDeviceHandleUsbfs::RefreshEndpointInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
      info.interface = interface.interface.get();
    }
  }
}

void UsbDeviceHandleUsbfs::ReportIsochronousError(
    const std::vector<uint32_t>& packet_lengths,
    UsbDeviceHandle::IsochronousTransferCallback callback,
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

void UsbDeviceHandleUsbfs::SetUpTimeoutCallback(Transfer* transfer,
                                                unsigned int timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timeout == 0)
    return;

  transfer->timeout_closure.Reset(
      base::BindOnce(&UsbDeviceHandleUsbfs::OnTimeout, this, transfer));
  task_runner_->PostDelayedTask(FROM_HERE, transfer->timeout_closure.callback(),
                                base::Milliseconds(timeout));
}

void UsbDeviceHandleUsbfs::OnTimeout(Transfer* transfer) {
  CancelTransfer(transfer, UsbTransferStatus::TIMEOUT);
}

std::unique_ptr<UsbDeviceHandleUsbfs::Transfer>
UsbDeviceHandleUsbfs::RemoveFromTransferList(Transfer* transfer_ptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = base::ranges::find(transfers_, transfer_ptr,
                               &std::unique_ptr<Transfer>::get);
  CHECK(it != transfers_.end(), base::NotFatalUntil::M130);
  std::unique_ptr<Transfer> transfer = std::move(*it);
  transfers_.erase(it);
  return transfer;
}

void UsbDeviceHandleUsbfs::CancelTransfer(Transfer* transfer,
                                          UsbTransferStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(device_);

  if (transfer->cancelled)
    return;

  // |transfer| must stay in |transfers_| as it is still being processed by the
  // kernel and will be reaped later.
  transfer->cancelled = true;

  helper_.AsyncCall(&BlockingTaskRunnerHelper::DiscardUrb)
      .WithArgs(transfer)
      .Then(
          base::BindOnce(&UsbDeviceHandleUsbfs::UrbDiscarded, this, transfer));

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transfer->discarded = true;
  if (transfer->reaped)
    RemoveFromTransferList(transfer);
}

}  // namespace device
