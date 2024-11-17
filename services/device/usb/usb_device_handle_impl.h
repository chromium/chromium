// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/usb/scoped_libusb_device_handle.h"
#include "services/device/usb/usb_device_handle.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace base {
class RefCountedBytes;
class SequencedTaskRunner;
}  // namespace base

namespace device {

struct EndpointMapValue {
  raw_ptr<const mojom::UsbInterfaceInfo> interface;
  raw_ptr<const mojom::UsbEndpointInfo> endpoint;
};

class UsbDeviceImpl;

typedef libusb_iso_packet_descriptor* PlatformUsbIsoPacketDescriptor;
typedef libusb_transfer* PlatformUsbTransferHandle;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandleImpl : public UsbDeviceHandle {
 public:
  UsbDeviceHandleImpl(const UsbDeviceHandleImpl&) = delete;
  UsbDeviceHandleImpl& operator=(const UsbDeviceHandleImpl&) = delete;

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

  void IsochronousTransferIn(uint8_t endpoint,
                             const std::vector<uint32_t>& packet_lengths,
                             unsigned int timeout,
                             IsochronousTransferCallback callback) override;

  void IsochronousTransferOut(uint8_t endpoint,
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
  friend class UsbDeviceImpl;

  // This constructor is called by UsbDeviceImpl.
  UsbDeviceHandleImpl(
      scoped_refptr<UsbDeviceImpl> device,
      ScopedLibusbDeviceHandle handle,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  ~UsbDeviceHandleImpl() override;

  libusb_device_handle* handle() const { return handle_.get(); }

 private:
  class InterfaceClaimer;
  class Transfer;

  void SetConfigurationBlocking(int configuration_value,
                                ResultCallback callback);
  void SetConfigurationComplete(bool success, ResultCallback callback);
  void ClaimInterfaceBlocking(int interface_number, ResultCallback callback);
  void ClaimInterfaceComplete(scoped_refptr<InterfaceClaimer> interface_claimer,
                              ResultCallback callback);
  void SetInterfaceAlternateSettingBlocking(int interface_number,
                                            int alternate_setting,
                                            ResultCallback callback);
  void SetInterfaceAlternateSettingComplete(int interface_number,
                                            int alternate_setting,
                                            bool success,
                                            ResultCallback callback);
  void ResetDeviceBlocking(ResultCallback callback);
  void ClearHaltBlocking(uint8_t endpoint_address, ResultCallback callback);

  // Refresh endpoint_map_ after ClaimInterface, ReleaseInterface and
  // SetInterfaceAlternateSetting.
  void RefreshEndpointMap();

  // Look up the claimed interface by endpoint address. Returns nullptr if an
  // interface containing the endpoint is not found.
  scoped_refptr<InterfaceClaimer> GetClaimedInterfaceForEndpoint(
      uint8_t endpoint_address);

  void ReportIsochronousTransferError(
      UsbDeviceHandle::IsochronousTransferCallback callback,
      const std::vector<uint32_t>& packet_lengths,
      mojom::UsbTransferStatus status);

  // Submits a transfer and starts tracking it. Retains the buffer and copies
  // the completion callback until the transfer finishes, whereupon it invokes
  // the callback then releases the buffer.
  void SubmitTransfer(std::unique_ptr<Transfer> transfer);

  // Removes the transfer from the in-flight transfer set and invokes the
  // completion callback.
  void TransferComplete(Transfer* transfer, base::OnceClosure callback);

  scoped_refptr<UsbDeviceImpl> device_;

  ScopedLibusbDeviceHandle handle_;

  typedef std::map<int, scoped_refptr<InterfaceClaimer>> ClaimedInterfaceMap;
  ClaimedInterfaceMap claimed_interfaces_;

  // This set holds weak pointers to pending transfers.
  std::set<Transfer*> transfers_;

  // A map from endpoints to EndpointMapValue
  typedef std::map<int, EndpointMapValue> EndpointMap;
  EndpointMap endpoint_map_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_
