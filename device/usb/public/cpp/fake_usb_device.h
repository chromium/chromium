// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_PUBLIC_CPP_FAKE_USB_DEVICE_H_
#define DEVICE_USB_PUBLIC_CPP_FAKE_USB_DEVICE_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "device/usb/public/cpp/fake_usb_device_info.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

// This class provides fake implementation for device::mojom::UsbDevice.
// It should be used together with FakeUsbDeviceManager and FakeUsbDeviceInfo
// just for testing.
class FakeUsbDevice : public mojom::UsbDevice,
                      public FakeUsbDeviceInfo::Observer {
 public:
  static void Create(scoped_refptr<FakeUsbDeviceInfo> device,
                     mojom::UsbDeviceRequest request,
                     mojom::UsbDeviceClientPtr client);
  ~FakeUsbDevice() override;

 private:
  FakeUsbDevice(scoped_refptr<FakeUsbDeviceInfo> device,
                mojom::UsbDeviceClientPtr client);

  // Device implementation:
  void Open(OpenCallback callback) override;
  void Close(CloseCallback callback) override;
  void SetConfiguration(uint8_t value,
                        SetConfigurationCallback callback) override;
  void ClaimInterface(uint8_t interface_number,
                      ClaimInterfaceCallback callback) override;
  void ReleaseInterface(uint8_t interface_number,
                        ReleaseInterfaceCallback callback) override;
  void SetInterfaceAlternateSetting(
      uint8_t interface_number,
      uint8_t alternate_setting,
      SetInterfaceAlternateSettingCallback callback) override;
  void Reset(ResetCallback callback) override;
  void ClearHalt(uint8_t endpoint, ClearHaltCallback callback) override;
  void ControlTransferIn(mojom::UsbControlTransferParamsPtr params,
                         uint32_t length,
                         uint32_t timeout,
                         ControlTransferInCallback callback) override;
  void ControlTransferOut(mojom::UsbControlTransferParamsPtr params,
                          const std::vector<uint8_t>& data,
                          uint32_t timeout,
                          ControlTransferOutCallback callback) override;
  void GenericTransferIn(uint8_t endpoint_number,
                         uint32_t length,
                         uint32_t timeout,
                         GenericTransferInCallback callback) override;
  void GenericTransferOut(uint8_t endpoint_number,
                          const std::vector<uint8_t>& data,
                          uint32_t timeout,
                          GenericTransferOutCallback callback) override;
  void IsochronousTransferIn(uint8_t endpoint_number,
                             const std::vector<uint32_t>& packet_lengths,
                             uint32_t timeout,
                             IsochronousTransferInCallback callback) override;
  void IsochronousTransferOut(uint8_t endpoint_number,
                              const std::vector<uint8_t>& data,
                              const std::vector<uint32_t>& packet_lengths,
                              uint32_t timeout,
                              IsochronousTransferOutCallback callback) override;

  // FakeUsbDeviceInfo::Observer implementation:
  void OnDeviceRemoved(scoped_refptr<FakeUsbDeviceInfo> device) override;

  void CloseHandle();

  const scoped_refptr<FakeUsbDeviceInfo> device_;

  ScopedObserver<FakeUsbDeviceInfo, FakeUsbDeviceInfo::Observer> observer_;

  bool is_opened_ = false;
  mojo::StrongBindingPtr<mojom::UsbDevice> binding_;
  device::mojom::UsbDeviceClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(FakeUsbDevice);
};

}  // namespace device

#endif  // DEVICE_USB_PUBLIC_CPP_FAKE_USB_DEVICE_H_
