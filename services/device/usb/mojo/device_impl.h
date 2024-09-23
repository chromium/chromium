// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_MOJO_DEVICE_IMPL_H_
#define SERVICES_DEVICE_USB_MOJO_DEVICE_IMPL_H_

#include <stdint.h>
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
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_device_handle.h"

namespace device::usb {

// Implementation of the public Device interface. Instances of this class are
// constructed by DeviceManagerImpl and are strongly bound to their MessagePipe
// lifetime.
class DeviceImpl : public mojom::UsbDevice, public device::UsbDevice::Observer {
 public:
  static void Create(scoped_refptr<device::UsbDevice> device,
                     mojo::PendingReceiver<mojom::UsbDevice> receiver,
                     mojo::PendingRemote<mojom::UsbDeviceClient> client,
                     base::span<const uint8_t> blocked_interface_classes,
                     bool allow_security_key_requests);

  DeviceImpl(const DeviceImpl&) = delete;
  DeviceImpl& operator=(const DeviceImpl&) = delete;

  ~DeviceImpl() override;

 private:
  DeviceImpl(scoped_refptr<device::UsbDevice> device,
             mojo::PendingRemote<mojom::UsbDeviceClient> client,
             base::span<const uint8_t> blocked_interface_classes,
             bool allow_security_key_requests);

  // Closes the device if it's open. This will always set |device_handle_| to
  // null.
  void CloseHandle();

  // Checks interface permissions for control transfers.
  bool HasControlTransferPermission(
      mojom::UsbControlTransferRecipient recipient,
      uint16_t index);

  // Handles completion of an open request.
  static void OnOpen(base::WeakPtr<DeviceImpl> device,
                     OpenCallback callback,
                     scoped_refptr<device::UsbDeviceHandle> handle);
  void OnPermissionGrantedForOpen(OpenCallback callback, bool granted);

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

  // device::UsbDevice::Observer implementation:
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;

  void OnInterfaceClaimed(ClaimInterfaceCallback callback, bool success);
  void OnClientConnectionError();

  // Reject and report bad mojo messaage if `length` exceeds limit.
  bool ShouldRejectUsbTransferLengthAndReportBadMessage(size_t length);

  const scoped_refptr<device::UsbDevice> device_;
  base::ScopedObservation<device::UsbDevice, device::UsbDevice::Observer>
      observation_{this};

  // The device handle. Will be null before the device is opened and after it
  // has been closed. |opening_| is set to true while the asynchronous open is
  // in progress.
  bool opening_ = false;
  scoped_refptr<UsbDeviceHandle> device_handle_;

  const base::flat_set<uint8_t> blocked_interface_classes_;
  const bool allow_security_key_requests_;
  mojo::SelfOwnedReceiverRef<mojom::UsbDevice> receiver_;
  mojo::Remote<device::mojom::UsbDeviceClient> client_;
  base::WeakPtrFactory<DeviceImpl> weak_factory_{this};
};

}  // namespace device::usb

#endif  // SERVICES_DEVICE_USB_MOJO_DEVICE_IMPL_H_
