// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_MAC_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_MAC_H_

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/memory/raw_ptr.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/usb/usb_device_handle.h"

namespace base {
class RefCountedBytes;
}

namespace device {

class UsbDeviceMac;
struct Transfer;

class UsbDeviceHandleMac : public UsbDeviceHandle {
 public:
  // Requires Mac OS X version 10.1.2 or later.
  using ScopedIOUSBDeviceInterface =
      base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface187>;
  // Requires Mac OS X version 10.0.4 or later.
  using ScopedIOUSBInterfaceInterface =
      base::mac::ScopedIOPluginInterface<IOUSBInterfaceInterface182>;

  // UsbDeviceHandle implementation:
  UsbDeviceHandleMac(scoped_refptr<UsbDeviceMac> device,
                     ScopedIOUSBDeviceInterface device_interface);
  UsbDeviceHandleMac(const UsbDeviceHandleMac&) = delete;
  UsbDeviceHandleMac& operator=(const UsbDeviceHandleMac&) = delete;
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
  ~UsbDeviceHandleMac() override;

 private:
  struct EndpointMapValue {
    raw_ptr<const mojom::UsbInterfaceInfo> interface;
    raw_ptr<const mojom::UsbEndpointInfo> endpoint;
    uint8_t pipe_reference;
  };

  void BulkIn(const ScopedIOUSBInterfaceInterface& interface_interface,
              uint8_t pipe_reference,
              scoped_refptr<base::RefCountedBytes> buffer,
              uint32_t timeout,
              std::unique_ptr<Transfer> transfer);
  void BulkOut(const ScopedIOUSBInterfaceInterface& interface_interface,
               uint8_t pipe_reference,
               scoped_refptr<base::RefCountedBytes> buffer,
               uint32_t timeout,
               std::unique_ptr<Transfer> transfer);
  void InterruptIn(const ScopedIOUSBInterfaceInterface& interface_interface,
                   uint8_t pipe_reference,
                   scoped_refptr<base::RefCountedBytes> buffer,
                   std::unique_ptr<Transfer> transfer);
  void InterruptOut(const ScopedIOUSBInterfaceInterface& interface_interface,
                    uint8_t pipe_reference,
                    scoped_refptr<base::RefCountedBytes> buffer,
                    std::unique_ptr<Transfer> transfer);
  // Refresh endpoint_map_ after ClaimInterface, ReleaseInterface and
  // SetInterfaceAlternateSetting. It is needed so that endpoints can be mapped
  // to their respective mojom Interface.
  void RefreshEndpointMap();

  void ReportIsochronousTransferError(
      UsbDeviceHandle::IsochronousTransferCallback callback,
      std::vector<uint32_t> packet_lengths,
      mojom::UsbTransferStatus status);

  void Clear();

  void OnAsyncGeneric(IOReturn result, size_t size, Transfer* transfer);
  void OnAsyncIsochronous(IOReturn result, size_t size, Transfer* transfer);
  static void AsyncIoCallback(void* refcon, IOReturn result, void* arg0);

  // A map from the endpoint indices to its corresponding EndpointMapValue,
  // which contains the Interface and Endpoint Mojo structures.
  using EndpointMap = base::flat_map<int, EndpointMapValue>;
  EndpointMap endpoint_map_;

  base::flat_set<std::unique_ptr<Transfer>, base::UniquePtrComparator>
      transfers_;

  ScopedIOUSBDeviceInterface device_interface_;
  base::ScopedCFTypeRef<CFRunLoopSourceRef> device_source_;
  scoped_refptr<UsbDeviceMac> device_;

  // Both maps take the interface number in as the respective key.
  base::flat_map<uint8_t, ScopedIOUSBInterfaceInterface> interfaces_;
  base::flat_map<uint8_t, base::ScopedCFTypeRef<CFRunLoopSourceRef>> sources_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_MAC_H_
