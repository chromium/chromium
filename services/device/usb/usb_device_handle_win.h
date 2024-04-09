// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/win/scoped_handle.h"
#include "services/device/usb/scoped_winusb_handle.h"
#include "services/device/usb/usb_device_handle.h"

struct _USB_NODE_CONNECTION_INFORMATION_EX;

namespace base {
class RefCountedBytes;
class SequencedTaskRunner;
}  // namespace base

namespace device {

class UsbDeviceWin;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandleWin : public UsbDeviceHandle {
 public:
  UsbDeviceHandleWin(const UsbDeviceHandleWin&) = delete;
  UsbDeviceHandleWin& operator=(const UsbDeviceHandleWin&) = delete;

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
  friend class UsbDeviceWin;

  // Constructor used to build a connection to the device.
  UsbDeviceHandleWin(scoped_refptr<UsbDeviceWin> device);

  // Constructor used to build a connection to the device's parent hub. To avoid
  // bugs in USB hub drivers a single global sequenced task runner is used for
  // all calls to the driver.
  UsbDeviceHandleWin(
      scoped_refptr<UsbDeviceWin> device,
      base::win::ScopedHandle handle,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  ~UsbDeviceHandleWin() override;

  void UpdateFunction(int interface_number,
                      const std::wstring& function_driver,
                      const std::wstring& function_path);

 private:
  struct Interface;
  class Request;

  using OpenInterfaceCallback = base::OnceCallback<void(Interface*)>;

  struct Interface {
    Interface();

    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;

    ~Interface();

    // This may be nullptr in the rare case of a device which doesn't have any
    // interfaces. In that case the Windows API still considers the device to
    // have a single function which is represented here by initializing
    // |interface_number| and |first_interface| to create a fake interface 0.
    raw_ptr<const mojom::UsbInterfaceInfo> info = nullptr;

    // These fields are copied from |info| and initialized to 0 in case it is
    // nullptr.
    uint8_t interface_number = 0;
    uint8_t first_interface = 0;

    // In a composite device each function has its own driver and path to open.
    std::wstring function_driver;
    std::wstring function_path;
    base::win::ScopedHandle function_handle;

    ScopedWinUsbHandle handle;
    bool claimed = false;

    // The currently selected alternative interface setting. This is assumed to
    // be the first alternate when the device is opened.
    uint8_t alternate_setting = 0;

    // The count of outstanding requests, including associated interfaces
    // claimed which require keeping the handles owned by this object open.
    int reference_count = 0;

    // Closures to execute when |function_path| has been populated.
    std::vector<OpenInterfaceCallback> ready_callbacks;
  };

  struct Endpoint {
    raw_ptr<const mojom::UsbInterfaceInfo, DanglingUntriaged> interface;
    mojom::UsbTransferType type;
  };

  void OpenInterfaceHandle(Interface* interface,
                           OpenInterfaceCallback callback);

  // Interfaces on a USB device are organized into "functions". When making a
  // request to a device the first interface of each function is the one that
  // has a valid |function_handle|. This function finds the correct interface
  // for making requests to the provided interface based on the device's driver
  // configuration.
  Interface* GetFirstInterfaceForFunction(Interface* interface);
  void OnFunctionAvailable(OpenInterfaceCallback callback,
                           Interface* interface);
  void OnFirstInterfaceOpened(int interface_number,
                              OpenInterfaceCallback callback,
                              Interface* first_interface);
  void OnInterfaceClaimed(ResultCallback callback, Interface* interface);
  void OnSetAlternateInterfaceSetting(int interface_number,
                                      int alternate_setting,
                                      ResultCallback callback,
                                      bool result);
  void RegisterEndpoints(const mojom::UsbInterfaceInfo* interface,
                         const mojom::UsbAlternateInterfaceInfo& alternate);
  void UnregisterEndpoints(const mojom::UsbAlternateInterfaceInfo& alternate);
  void OnClearHalt(int interface_number, ResultCallback callback, bool result);
  void OpenInterfaceForControlTransfer(
      mojom::UsbControlTransferRecipient recipient,
      uint16_t index,
      OpenInterfaceCallback callback);
  void OnFunctionAvailableForEp0(OpenInterfaceCallback callback,
                                 Interface* interface);
  void OnInterfaceOpenedForControlTransfer(
      mojom::UsbTransferDirection direction,
      mojom::UsbControlTransferType request_type,
      mojom::UsbControlTransferRecipient recipient,
      uint8_t request,
      uint16_t value,
      uint16_t index,
      scoped_refptr<base::RefCountedBytes> buffer,
      unsigned int timeout,
      TransferCallback callback,
      Interface* interface);
  Request* MakeRequest(Interface* interface);
  std::unique_ptr<Request> UnlinkRequest(Request* request);
  void GotNodeConnectionInformation(
      TransferCallback callback,
      std::unique_ptr<_USB_NODE_CONNECTION_INFORMATION_EX> node_connection_info,
      scoped_refptr<base::RefCountedBytes> buffer,
      std::pair<DWORD, DWORD> result_and_bytes_transferred);
  void GotDescriptorFromNodeConnection(
      TransferCallback callback,
      scoped_refptr<base::RefCountedBytes> request_buffer,
      scoped_refptr<base::RefCountedBytes> original_buffer,
      std::pair<DWORD, DWORD> result_and_bytes_transferred);
  void TransferComplete(TransferCallback callback,
                        scoped_refptr<base::RefCountedBytes> buffer,
                        Request* request_ptr,
                        DWORD win32_result,
                        size_t bytes_transferred);
  void ReportIsochronousError(const std::vector<uint32_t>& packet_lengths,
                              IsochronousTransferCallback callback,
                              mojom::UsbTransferStatus status);
  bool AllFunctionsEnumerated() const;
  void ReleaseInterfaceReference(Interface* interface);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<UsbDeviceWin> device_;

  // |hub_handle_| or all the handles for claimed interfaces in |interfaces_|
  // must outlive their associated |requests_| because individual Request
  // objects hold on to the raw handles for the purpose of calling
  // GetOverlappedResult().
  base::win::ScopedHandle hub_handle_;

  std::map<uint8_t, Interface> interfaces_;
  std::map<uint8_t, Endpoint> endpoints_;
  std::list<std::unique_ptr<Request>> requests_;

  // Control transfers which are waiting for a function handle to be ready.
  std::vector<OpenInterfaceCallback> ep0_ready_callbacks_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::WeakPtrFactory<UsbDeviceHandleWin> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_
