// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_USB_MOJO_DEVICE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_USB_MOJO_DEVICE_H_

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

// This class provides mock implementation for device::mojom::UsbDevice.
// It should be used together with FakeUsbDeviceManager and
// FakeUsbDeviceInfo just for testing.
class MockUsbMojoDevice : public mojom::UsbDevice {
 public:
  MockUsbMojoDevice();

  MockUsbMojoDevice(const MockUsbMojoDevice&) = delete;
  MockUsbMojoDevice& operator=(const MockUsbMojoDevice&) = delete;

  ~MockUsbMojoDevice() override;

  MOCK_METHOD1(Open, void(OpenCallback));
  MOCK_METHOD1(Close, void(CloseCallback));
  MOCK_METHOD2(SetConfiguration, void(uint8_t, SetConfigurationCallback));
  MOCK_METHOD2(ClaimInterface, void(uint8_t, ClaimInterfaceCallback));
  MOCK_METHOD2(ReleaseInterface, void(uint8_t, ReleaseInterfaceCallback));
  MOCK_METHOD3(SetInterfaceAlternateSetting,
               void(uint8_t, uint8_t, SetInterfaceAlternateSettingCallback));
  MOCK_METHOD1(Reset, void(ResetCallback));
  MOCK_METHOD3(ClearHalt,
               void(mojom::UsbTransferDirection, uint8_t, ClearHaltCallback));
  MOCK_METHOD4(ControlTransferIn,
               void(mojom::UsbControlTransferParamsPtr,
                    uint32_t,
                    uint32_t,
                    ControlTransferInCallback));
  MOCK_METHOD4(ControlTransferOut,
               void(mojom::UsbControlTransferParamsPtr,
                    base::span<const uint8_t>,
                    uint32_t,
                    ControlTransferOutCallback));
  MOCK_METHOD4(GenericTransferIn,
               void(uint8_t, uint32_t, uint32_t, GenericTransferInCallback));
  MOCK_METHOD4(GenericTransferOut,
               void(uint8_t,
                    base::span<const uint8_t>,
                    uint32_t,
                    GenericTransferOutCallback));

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
                              base::span<const uint8_t> data,
                              const std::vector<uint32_t>& packet_lengths,
                              uint32_t timeout,
                              IsochronousTransferOutCallback callback) override;
  MOCK_METHOD4(
      IsochronousTransferOutInternal,
      std::vector<mojom::UsbIsochronousPacket>(uint8_t,
                                               base::span<const uint8_t>,
                                               const std::vector<uint32_t>&,
                                               uint32_t));
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_USB_MOJO_DEVICE_H_
