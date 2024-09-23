// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_MOCK_USB_DEVICE_HANDLE_H_
#define SERVICES_DEVICE_USB_MOCK_USB_DEVICE_HANDLE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "services/device/usb/usb_device_handle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockUsbDeviceHandle : public UsbDeviceHandle {
 public:
  explicit MockUsbDeviceHandle(UsbDevice* device);

  scoped_refptr<UsbDevice> GetDevice() const override;
  MOCK_METHOD0(Close, void());

  // TODO(crbug.com/40524294): Use MOCK_METHOD directly once GMock gets the
  // move-only type support.
  void SetConfiguration(int configuration_value,
                        ResultCallback callback) override {
    SetConfigurationInternal(configuration_value, callback);
  }
  MOCK_METHOD2(SetConfigurationInternal,
               void(int configuration_value, ResultCallback& callback));

  void ClaimInterface(int interface_number, ResultCallback callback) override {
    ClaimInterfaceInternal(interface_number, callback);
  }
  MOCK_METHOD2(ClaimInterfaceInternal,
               void(int interface_number, ResultCallback& callback));

  void ReleaseInterface(int interface_number,
                        ResultCallback callback) override {
    ReleaseInterfaceInternal(interface_number, callback);
  }
  MOCK_METHOD2(ReleaseInterfaceInternal,
               void(int interface_number, ResultCallback& callback));

  void SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting,
                                    ResultCallback callback) override {
    SetInterfaceAlternateSettingInternal(interface_number, alternate_setting,
                                         callback);
  }
  MOCK_METHOD3(SetInterfaceAlternateSettingInternal,
               void(int interface_number,
                    int alternate_setting,
                    ResultCallback& callback));

  void ResetDevice(ResultCallback callback) override {
    ResetDeviceInternal(callback);
  }
  MOCK_METHOD1(ResetDeviceInternal, void(ResultCallback& callback));

  void ClearHalt(mojom::UsbTransferDirection direction,
                 uint8_t endpoint_number,
                 ResultCallback callback) override {
    ClearHaltInternal(direction, endpoint_number, callback);
  }
  MOCK_METHOD3(ClearHaltInternal,
               void(mojom::UsbTransferDirection direction,
                    uint8_t endpoint_number,
                    ResultCallback& callback));

  void ControlTransfer(mojom::UsbTransferDirection direction,
                       mojom::UsbControlTransferType request_type,
                       mojom::UsbControlTransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override {
    ControlTransferInternal(direction, request_type, recipient, request, value,
                            index, buffer, timeout, callback);
  }
  MOCK_METHOD9(ControlTransferInternal,
               void(mojom::UsbTransferDirection direction,
                    mojom::UsbControlTransferType request_type,
                    mojom::UsbControlTransferRecipient recipient,
                    uint8_t request,
                    uint16_t value,
                    uint16_t index,
                    scoped_refptr<base::RefCountedBytes> buffer,
                    unsigned int timeout,
                    TransferCallback& callback));

  void IsochronousTransferIn(uint8_t endpoint,
                             const std::vector<uint32_t>& packet_lengths,
                             unsigned int timeout,
                             IsochronousTransferCallback callback) override {
    IsochronousTransferInInternal(endpoint, packet_lengths, timeout, callback);
  }
  MOCK_METHOD4(IsochronousTransferInInternal,
               void(uint8_t endpoint,
                    const std::vector<uint32_t>& packet_lengths,
                    unsigned int timeout,
                    IsochronousTransferCallback& callback));

  void IsochronousTransferOut(uint8_t endpoint,
                              scoped_refptr<base::RefCountedBytes> buffer,
                              const std::vector<uint32_t>& packet_lengths,
                              unsigned int timeout,
                              IsochronousTransferCallback callback) override {
    IsochronousTransferOutInternal(endpoint, buffer, packet_lengths, timeout,
                                   callback);
  }
  MOCK_METHOD5(IsochronousTransferOutInternal,
               void(uint8_t endpoint,
                    scoped_refptr<base::RefCountedBytes> buffer,
                    const std::vector<uint32_t>& packet_lengths,
                    unsigned int timeout,
                    IsochronousTransferCallback& callback));

  void GenericTransfer(mojom::UsbTransferDirection direction,
                       uint8_t endpoint,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override {
    GenericTransferInternal(direction, endpoint, buffer, timeout, callback);
  }
  MOCK_METHOD5(GenericTransferInternal,
               void(mojom::UsbTransferDirection direction,
                    uint8_t endpoint,
                    scoped_refptr<base::RefCountedBytes> buffer,
                    unsigned int timeout,
                    TransferCallback& callback));

  MOCK_METHOD1(FindInterfaceByEndpoint,
               const mojom::UsbInterfaceInfo*(uint8_t endpoint_address));

 protected:
  ~MockUsbDeviceHandle() override;

 private:
  raw_ptr<UsbDevice, DanglingUntriaged> device_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_MOCK_USB_DEVICE_HANDLE_H_
