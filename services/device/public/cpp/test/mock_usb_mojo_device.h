// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_USB_MOJO_DEVICE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_USB_MOJO_DEVICE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

// This class provides mock implementation for device::mojom::UsbDevice.
// It should be used together with FakeUsbDeviceManager and
// FakeUsbDeviceInfo just for testing.
class MockUsbMojoDevice : public mojom::UsbDevice {
 public:
  MockUsbMojoDevice();
  ~MockUsbMojoDevice() override;

  // As current version of gmock in Chromium doesn't support move-only types,
  // so it needs mock methods with OnceCallback pointer as parameter here.
  void Open(OpenCallback callback) override { OpenInternal(&callback); }
  MOCK_METHOD1(OpenInternal, void(OpenCallback*));

  void Close(CloseCallback callback) override { CloseInternal(&callback); }
  MOCK_METHOD1(CloseInternal, void(CloseCallback*));

  void SetConfiguration(uint8_t value,
                        SetConfigurationCallback callback) override {
    SetConfigurationInternal(value, &callback);
  }
  MOCK_METHOD2(SetConfigurationInternal,
               void(uint8_t, SetConfigurationCallback*));

  void ClaimInterface(uint8_t interface_number,
                      ClaimInterfaceCallback callback) override {
    ClaimInterfaceInternal(interface_number, &callback);
  }
  MOCK_METHOD2(ClaimInterfaceInternal, void(uint8_t, ClaimInterfaceCallback*));

  void ReleaseInterface(uint8_t interface_number,
                        ReleaseInterfaceCallback callback) override {
    ReleaseInterfaceInternal(interface_number, &callback);
  }
  MOCK_METHOD2(ReleaseInterfaceInternal,
               void(uint8_t, ReleaseInterfaceCallback*));

  void SetInterfaceAlternateSetting(
      uint8_t interface_number,
      uint8_t alternate_setting,
      SetInterfaceAlternateSettingCallback callback) override {
    SetInterfaceAlternateSettingInternal(interface_number, alternate_setting,
                                         &callback);
  }
  MOCK_METHOD3(SetInterfaceAlternateSettingInternal,
               void(uint8_t, uint8_t, SetInterfaceAlternateSettingCallback*));

  void Reset(ResetCallback callback) override { ResetInternal(&callback); }
  MOCK_METHOD1(ResetInternal, void(ResetCallback*));

  void ClearHalt(uint8_t endpoint, ClearHaltCallback callback) override {
    ClearHaltInternal(endpoint, &callback);
  }
  MOCK_METHOD2(ClearHaltInternal, void(uint8_t, ClearHaltCallback*));

  void ControlTransferIn(mojom::UsbControlTransferParamsPtr params,
                         uint32_t length,
                         uint32_t timeout,
                         ControlTransferInCallback callback) override {
    ControlTransferInInternal(*params, length, timeout, &callback);
  }
  MOCK_METHOD4(ControlTransferInInternal,
               void(const mojom::UsbControlTransferParams&,
                    uint32_t,
                    uint32_t,
                    ControlTransferInCallback*));

  void ControlTransferOut(mojom::UsbControlTransferParamsPtr params,
                          const std::vector<uint8_t>& data,
                          uint32_t timeout,
                          ControlTransferOutCallback callback) override {
    ControlTransferOutInternal(*params, data, timeout, &callback);
  }
  MOCK_METHOD4(ControlTransferOutInternal,
               void(const mojom::UsbControlTransferParams&,
                    const std::vector<uint8_t>&,
                    uint32_t,
                    ControlTransferOutCallback*));

  void GenericTransferIn(uint8_t endpoint_number,
                         uint32_t length,
                         uint32_t timeout,
                         GenericTransferInCallback callback) override {
    GenericTransferInInternal(endpoint_number, length, timeout, &callback);
  }
  MOCK_METHOD4(GenericTransferInInternal,
               void(uint8_t, uint32_t, uint32_t, GenericTransferInCallback*));

  void GenericTransferOut(uint8_t endpoint_number,
                          const std::vector<uint8_t>& data,
                          uint32_t timeout,
                          GenericTransferOutCallback callback) override {
    GenericTransferOutInternal(endpoint_number, data, timeout, &callback);
  }
  MOCK_METHOD4(GenericTransferOutInternal,
               void(uint8_t,
                    const std::vector<uint8_t>&,
                    uint32_t,
                    GenericTransferOutCallback*));

  void IsochronousTransferIn(uint8_t endpoint_number,
                             const std::vector<uint32_t>& packet_lengths,
                             uint32_t timeout,
                             IsochronousTransferInCallback callback) override;
  MOCK_METHOD3(
      IsochronousTransferInInternal,
      std::vector<mojom::UsbIsochronousPacket>(uint8_t,
                                               const std::vector<uint32_t>&,
                                               uint32_t));

  void IsochronousTransferOut(uint8_t endpoint_number,
                              const std::vector<uint8_t>& data,
                              const std::vector<uint32_t>& packet_lengths,
                              uint32_t timeout,
                              IsochronousTransferOutCallback callback) override;
  MOCK_METHOD4(
      IsochronousTransferOutInternal,
      std::vector<mojom::UsbIsochronousPacket>(uint8_t,
                                               const std::vector<uint8_t>&,
                                               const std::vector<uint32_t>&,
                                               uint32_t));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUsbMojoDevice);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_USB_MOJO_DEVICE_H_
