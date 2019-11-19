// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_FAKE_USB_DEVICE_HANDLE_H_
#define SERVICES_DEVICE_USB_FAKE_USB_DEVICE_HANDLE_H_

#include <vector>

#include "services/device/usb/usb_device_handle.h"

namespace device {

// This class implements a fake USB device handle that will handle control
// requests by responding with data out of the provided buffer. The format of
// each record in the buffer is:
//
// 0         1         2         3...
// +---------+---------+---------+---------- - - -
// | success |      length       | data...
// +---------+---------+---------+---------- - - -
//
// If |success| is 1 the transfer will succeed and respond with |length| bytes
// of the following data (no data is consumed for outgoing transfers). If
// |success| is 0 the transfer will fail with USB_TRANSFER_ERROR. If the buffer
// is exhausted all following transfers will fail with USB_TRANSFER_DISCONNECT.
class FakeUsbDeviceHandle : public UsbDeviceHandle {
 public:
  FakeUsbDeviceHandle(const uint8_t* data, size_t size);

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
  void ClearHalt(uint8_t endpoint, ResultCallback callback) override;

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

 private:
  ~FakeUsbDeviceHandle() override;

  const uint8_t* const data_;
  const size_t size_;
  size_t position_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_FAKE_USB_DEVICE_HANDLE_H_
