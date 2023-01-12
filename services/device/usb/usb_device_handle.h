// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/usb/usb_descriptors.h"

namespace base {
class RefCountedBytes;
}

namespace device {

class UsbDevice;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandle : public base::RefCountedThreadSafe<UsbDeviceHandle> {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;
  using TransferCallback =
      base::OnceCallback<void(mojom::UsbTransferStatus,
                              scoped_refptr<base::RefCountedBytes>,
                              size_t)>;
  using IsochronousTransferCallback = base::OnceCallback<void(
      scoped_refptr<base::RefCountedBytes>,
      std::vector<mojom::UsbIsochronousPacketPtr> packets)>;

  UsbDeviceHandle(const UsbDeviceHandle&) = delete;
  UsbDeviceHandle& operator=(const UsbDeviceHandle&) = delete;

  virtual scoped_refptr<UsbDevice> GetDevice() const = 0;

  // Notifies UsbDevice to drop the reference of this object; cancels all the
  // flying transfers.
  // It is possible that the object has no other reference after this call. So
  // if it is called using a raw pointer, it could be invalidated.
  // The platform device handle will be closed when UsbDeviceHandle destructs.
  virtual void Close() = 0;

  // Device manipulation operations.
  virtual void SetConfiguration(int configuration_value,
                                ResultCallback callback) = 0;
  virtual void ClaimInterface(int interface_number,
                              ResultCallback callback) = 0;
  virtual void ReleaseInterface(int interface_number,
                                ResultCallback callback) = 0;
  virtual void SetInterfaceAlternateSetting(int interface_number,
                                            int alternate_setting,
                                            ResultCallback callback) = 0;
  virtual void ResetDevice(ResultCallback callback) = 0;
  virtual void ClearHalt(mojom::UsbTransferDirection direction,
                         uint8_t endpoint_number,
                         ResultCallback callback) = 0;

  virtual void ControlTransfer(mojom::UsbTransferDirection direction,
                               mojom::UsbControlTransferType request_type,
                               mojom::UsbControlTransferRecipient recipient,
                               uint8_t request,
                               uint16_t value,
                               uint16_t index,
                               scoped_refptr<base::RefCountedBytes> buffer,
                               unsigned int timeout,
                               TransferCallback callback) = 0;

  virtual void IsochronousTransferIn(
      uint8_t endpoint_number,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      IsochronousTransferCallback callback) = 0;

  virtual void IsochronousTransferOut(
      uint8_t endpoint_number,
      scoped_refptr<base::RefCountedBytes> buffer,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      IsochronousTransferCallback callback) = 0;

  virtual void GenericTransfer(mojom::UsbTransferDirection direction,
                               uint8_t endpoint_number,
                               scoped_refptr<base::RefCountedBytes> buffer,
                               unsigned int timeout,
                               TransferCallback callback) = 0;

  // Gets the interface containing |endpoint_address|. Returns nullptr if no
  // claimed interface contains that endpoint.
  virtual const mojom::UsbInterfaceInfo* FindInterfaceByEndpoint(
      uint8_t endpoint_address) = 0;

 protected:
  friend class base::RefCountedThreadSafe<UsbDeviceHandle>;

  UsbDeviceHandle();
  virtual ~UsbDeviceHandle();
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_H_
