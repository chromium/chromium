// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/win/scoped_handle.h"
#include "services/device/usb/scoped_winusb_handle.h"
#include "services/device/usb/usb_device_handle.h"

namespace base {
class RefCountedBytes;
class SequencedTaskRunner;
}  // namespace base

namespace device {

class UsbDeviceWin;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandleWin : public UsbDeviceHandle {
 public:
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

 protected:
  friend class UsbDeviceWin;

  // Constructor used to build a connection to the device.
  UsbDeviceHandleWin(scoped_refptr<UsbDeviceWin> device, bool composite);

  // Constructor used to build a connection to the device's parent hub.
  UsbDeviceHandleWin(scoped_refptr<UsbDeviceWin> device,
                     base::win::ScopedHandle handle);

  ~UsbDeviceHandleWin() override;

 private:
  class Request;

  struct Interface {
    Interface();
    ~Interface();

    uint8_t interface_number;
    uint8_t first_interface;
    ScopedWinUsbHandle handle;
    bool claimed = false;
    uint8_t alternate_setting = 0;

    DISALLOW_COPY_AND_ASSIGN(Interface);
  };

  struct Endpoint {
    const mojom::UsbInterfaceInfo* interface;
    mojom::UsbTransferType type;
  };

  bool OpenInterfaceHandle(Interface* interface);
  void RegisterEndpoints(const CombinedInterfaceInfo& interface);
  void UnregisterEndpoints(const CombinedInterfaceInfo& interface);
  WINUSB_INTERFACE_HANDLE GetInterfaceForControlTransfer(
      mojom::UsbControlTransferRecipient recipient,
      uint16_t index);
  void SetInterfaceAlternateSettingBlocking(uint8_t interface_number,
                                            uint8_t alternate_setting,
                                            const ResultCallback& callback);
  void SetInterfaceAlternateSettingComplete(uint8_t interface_number,
                                            uint8_t alternate_setting,
                                            const ResultCallback& callback);
  Request* MakeRequest(bool winusb_handle);
  std::unique_ptr<Request> UnlinkRequest(Request* request);
  void GotNodeConnectionInformation(TransferCallback callback,
                                    void* node_connection_info,
                                    scoped_refptr<base::RefCountedBytes> buffer,
                                    Request* request_ptr,
                                    DWORD win32_result,
                                    size_t bytes_transferred);
  void GotDescriptorFromNodeConnection(
      TransferCallback callback,
      scoped_refptr<base::RefCountedBytes> request_buffer,
      scoped_refptr<base::RefCountedBytes> original_buffer,
      Request* request_ptr,
      DWORD win32_result,
      size_t bytes_transferred);
  void TransferComplete(TransferCallback callback,
                        scoped_refptr<base::RefCountedBytes> buffer,
                        Request* request_ptr,
                        DWORD win32_result,
                        size_t bytes_transferred);
  void ReportIsochronousError(const std::vector<uint32_t>& packet_lengths,
                              IsochronousTransferCallback callback,
                              mojom::UsbTransferStatus status);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<UsbDeviceWin> device_;

  // |hub_handle_| or all the handles for claimed interfaces in |interfaces_|
  // must outlive their associated |requests_| because individual Request
  // objects hold on to the raw handles for the purpose of calling
  // GetOverlappedResult().
  base::win::ScopedHandle hub_handle_;
  base::win::ScopedHandle function_handle_;

  // The handle returned by WinUsb_Initialize is special.
  WINUSB_INTERFACE_HANDLE first_interface_handle_ = INVALID_HANDLE_VALUE;

  std::map<uint8_t, Interface> interfaces_;
  std::map<uint8_t, Endpoint> endpoints_;
  std::map<Request*, std::unique_ptr<Request>> requests_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::WeakPtrFactory<UsbDeviceHandleWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandleWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_
