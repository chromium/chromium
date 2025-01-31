// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_USBFS_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_USBFS_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "services/device/usb/usb_device_handle.h"

struct usbdevfs_urb;

namespace base {
class SequencedTaskRunner;
}

namespace device {

// Implementation of a USB device handle on top of the Linux USBFS ioctl
// interface available on Linux, Chrome OS and Android.
class UsbDeviceHandleUsbfs : public UsbDeviceHandle {
 public:
  // Constructs a new device handle from an existing |device| and open file
  // descriptor to that device. |blocking_task_runner| must run tasks on a
  // thread that supports FileDescriptorWatcher.
  UsbDeviceHandleUsbfs(
      scoped_refptr<UsbDevice> device,
      base::ScopedFD fd,
      base::ScopedFD lifeline_fd,
      const std::string& client_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  // UsbDeviceHandle implementation.
  scoped_refptr<UsbDevice> GetDevice() const override;
  void Close() override;
  void SetConfiguration(int configuration_value,
                        ResultCallback callback) override;
  void ClaimInterface(int interface_number, ResultCallback callback) override;
  void ReleaseInterface(int interface_number, ResultCallback callback) override;
  void SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting,
                                    ResultCallback callback) override;
  void ResetDevice(ResultCallback callback) override;
  void ClearHalt(mojom::UsbTransferDirection direction,
                 uint8_t endpoint_number,
                 ResultCallback callback) override;
  void ControlTransfer(mojom::UsbTransferDirection direction,
                       mojom::UsbControlTransferType request_type,
                       mojom::UsbControlTransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override;
  void IsochronousTransferIn(uint8_t endpoint_number,
                             const std::vector<uint32_t>& packet_lengths,
                             unsigned int timeout,
                             IsochronousTransferCallback callback) override;
  void IsochronousTransferOut(uint8_t endpoint_number,
                              scoped_refptr<base::RefCountedBytes> buffer,
                              const std::vector<uint32_t>& packet_lengths,
                              unsigned int timeout,
                              IsochronousTransferCallback callback) override;
  void GenericTransfer(mojom::UsbTransferDirection direction,
                       uint8_t endpoint_number,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override;
  const mojom::UsbInterfaceInfo* FindInterfaceByEndpoint(
      uint8_t endpoint_address) override;

 protected:
  ~UsbDeviceHandleUsbfs() override;

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  // Destroys |helper_| and releases ownership of |fd_| without closing it.
  void ReleaseFileDescriptor(base::OnceClosure callback);

  // Destroys |helper_| and closes |fd_|. Override to call
  // ReleaseFileDescriptor() if necessary.
  virtual void FinishClose();

 private:
  class BlockingTaskRunnerHelper;
  struct Transfer;
  struct InterfaceInfo {
    uint8_t alternate_setting;
  };
  struct EndpointInfo {
    mojom::UsbTransferType type;
    raw_ptr<const mojom::UsbInterfaceInfo> interface;
  };

  void SetConfigurationComplete(int configuration_value,
                                ResultCallback callback,
                                bool success);
  void SetAlternateInterfaceSettingComplete(int interface_number,
                                            int alternate_setting,
                                            ResultCallback callback,
                                            bool success);
  void DetachInterfaceComplete(int interface_number,
                               ResultCallback callback,
                               bool success);
  void ReleaseInterfaceComplete(int interface_number,
                                ResultCallback callback,
                                bool success);
  void IsochronousTransferInternal(uint8_t endpoint_address,
                                   scoped_refptr<base::RefCountedBytes> buffer,
                                   size_t total_length,
                                   const std::vector<uint32_t>& packet_lengths,
                                   unsigned int timeout,
                                   IsochronousTransferCallback callback);
  void ReapedUrbs(const std::vector<usbdevfs_urb*>& urbs);
  void TransferComplete(std::unique_ptr<Transfer> transfer);
  void RefreshEndpointInfo();
  void ReportIsochronousError(
      const std::vector<uint32_t>& packet_lengths,
      UsbDeviceHandle::IsochronousTransferCallback callback,
      mojom::UsbTransferStatus status);
  void SetUpTimeoutCallback(Transfer* transfer, unsigned int timeout);
  void OnTimeout(Transfer* transfer);
  std::unique_ptr<Transfer> RemoveFromTransferList(Transfer* transfer);
  void CancelTransfer(Transfer* transfer, mojom::UsbTransferStatus status);
  void DiscardUrbBlocking(Transfer* transfer);
  void UrbDiscarded(Transfer* transfer);

  scoped_refptr<UsbDevice> device_;
  int fd_;  // Copy of the base::ScopedFD held by |helper_|. Valid if |device_|.
  std::optional<std::string>
      client_id_;  // Client ID assigned by the Permission Broker.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Maps claimed interfaces by interface number to their current alternate
  // setting.
  std::map<uint8_t, InterfaceInfo> interfaces_;

  // Maps endpoints (by endpoint address) to the interface they are a part of.
  // Only endpoints of currently claimed and selected interface alternates are
  // included in the map.
  std::map<uint8_t, EndpointInfo> endpoints_;

  // Helper object exists on the blocking task thread and all calls to it and
  // its destruction must be posted there.
  base::SequenceBound<BlockingTaskRunnerHelper> helper_;

  std::list<std::unique_ptr<Transfer>> transfers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UsbDeviceHandleUsbfs> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_USBFS_H_
