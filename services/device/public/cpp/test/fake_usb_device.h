// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace device {

// This class provides fake implementation for device::mojom::UsbDevice.
// It should be used together with FakeUsbDeviceManager and FakeUsbDeviceInfo
// just for testing.
class FakeUsbDevice : public mojom::UsbDevice,
                      public FakeUsbDeviceInfo::Observer {
 public:
  static void Create(scoped_refptr<FakeUsbDeviceInfo> device,
                     base::span<const uint8_t> blocked_interface_classes,
                     mojo::PendingReceiver<device::mojom::UsbDevice> receiver,
                     mojo::PendingRemote<mojom::UsbDeviceClient> client);

  FakeUsbDevice(const FakeUsbDevice&) = delete;
  FakeUsbDevice& operator=(const FakeUsbDevice&) = delete;

  ~FakeUsbDevice() override;

 protected:
  FakeUsbDevice(scoped_refptr<FakeUsbDeviceInfo> device,
                base::span<const uint8_t> blocked_interface_classes,
                mojo::PendingRemote<mojom::UsbDeviceClient> client);

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
  void ClearHalt(mojom::UsbTransferDirection direction,
                 uint8_t endpoint_number,
                 ClearHaltCallback callback) override;
  void ControlTransferIn(mojom::UsbControlTransferParamsPtr params,
                         uint32_t length,
                         uint32_t timeout,
                         ControlTransferInCallback callback) override;
  void ControlTransferOut(mojom::UsbControlTransferParamsPtr params,
                          base::span<const uint8_t> data,
                          uint32_t timeout,
                          ControlTransferOutCallback callback) override;
  void GenericTransferIn(uint8_t endpoint_number,
                         uint32_t length,
                         uint32_t timeout,
                         GenericTransferInCallback callback) override;
  void GenericTransferOut(uint8_t endpoint_number,
                          base::span<const uint8_t> data,
                          uint32_t timeout,
                          GenericTransferOutCallback callback) override;
  void IsochronousTransferIn(uint8_t endpoint_number,
                             const std::vector<uint32_t>& packet_lengths,
                             uint32_t timeout,
                             IsochronousTransferInCallback callback) override;
  void IsochronousTransferOut(uint8_t endpoint_number,
                              base::span<const uint8_t> data,
                              const std::vector<uint32_t>& packet_lengths,
                              uint32_t timeout,
                              IsochronousTransferOutCallback callback) override;

  // FakeUsbDeviceInfo::Observer implementation:
  void OnDeviceRemoved(scoped_refptr<FakeUsbDeviceInfo> device) override;

  void OnClientConnectionError();

  void CloseHandle();

  mojo::SelfOwnedReceiverRef<mojom::UsbDevice> receiver_;

 private:
  void FinishOpen(OpenCallback callback, mojom::UsbOpenDeviceResultPtr result);

  const scoped_refptr<FakeUsbDeviceInfo> device_;
  const base::flat_set<uint8_t> blocked_interface_classes_;

  base::ScopedObservation<FakeUsbDeviceInfo, FakeUsbDeviceInfo::Observer>
      observation_{this};

  bool is_opened_ = false;

  // Recording the claimed interface_number list.
  std::set<uint8_t> claimed_interfaces_;
  mojo::Remote<device::mojom::UsbDeviceClient> client_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_H_
