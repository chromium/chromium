// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_USB_USB_API_H_
#define EXTENSIONS_BROWSER_API_USB_USB_API_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/usb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace extensions {

class DevicePermissionEntry;
class DevicePermissionsPrompt;
class DevicePermissionsManager;
class UsbDeviceResource;

class UsbExtensionFunction : public ExtensionFunction {
 protected:
  UsbExtensionFunction();
  ~UsbExtensionFunction() override;

  UsbDeviceManager* usb_device_manager();

 private:
  UsbDeviceManager* usb_device_manager_ = nullptr;
};

class UsbPermissionCheckingFunction : public UsbExtensionFunction {
 protected:
  UsbPermissionCheckingFunction();
  ~UsbPermissionCheckingFunction() override;

  bool HasDevicePermission(const device::mojom::UsbDeviceInfo& device);
  void RecordDeviceLastUsed();

 private:
  DevicePermissionsManager* device_permissions_manager_;
  scoped_refptr<DevicePermissionEntry> permission_entry_;
};

class UsbConnectionFunction : public UsbExtensionFunction {
 protected:
  UsbConnectionFunction();
  ~UsbConnectionFunction() override;

  UsbDeviceResource* GetResourceFromHandle(
      const api::usb::ConnectionHandle& handle);
  device::mojom::UsbDevice* GetDeviceFromHandle(
      const api::usb::ConnectionHandle& handle);
  const device::mojom::UsbDeviceInfo* GetDeviceInfoFromHandle(
      const api::usb::ConnectionHandle& handle);
  void ReleaseDeviceResource(const api::usb::ConnectionHandle& handle);
};

class UsbTransferFunction : public UsbConnectionFunction {
 protected:
  UsbTransferFunction();
  ~UsbTransferFunction() override;

  void OnCompleted(device::mojom::UsbTransferStatus status,
                   std::unique_ptr<base::DictionaryValue> transfer_info);
  void OnTransferInCompleted(device::mojom::UsbTransferStatus status,
                             const std::vector<uint8_t>& data);
  void OnTransferOutCompleted(device::mojom::UsbTransferStatus status);
  void OnDisconnect();
};

class UsbGenericTransferFunction : public UsbTransferFunction {
 protected:
  UsbGenericTransferFunction();
  ~UsbGenericTransferFunction() override;

  // InterruptTransfer::Params and BulkTransfer::Params
  template <typename T>
  ExtensionFunction::ResponseAction DoTransfer(T params);
};

class UsbFindDevicesFunction : public UsbExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.findDevices", USB_FINDDEVICES)

  UsbFindDevicesFunction();

 private:
  ~UsbFindDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetDevicesComplete(
      std::vector<device::mojom::UsbDeviceInfoPtr> devices);
  void OnDeviceOpened(const std::string& guid,
                      mojo::Remote<device::mojom::UsbDevice> device_ptr,
                      device::mojom::UsbOpenDeviceError error);
  void OpenComplete();
  void OnDisconnect();

  uint16_t vendor_id_;
  uint16_t product_id_;
  std::unique_ptr<base::ListValue> result_;
  base::Closure barrier_;

  DISALLOW_COPY_AND_ASSIGN(UsbFindDevicesFunction);
};

class UsbGetDevicesFunction : public UsbPermissionCheckingFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.getDevices", USB_GETDEVICES)

  UsbGetDevicesFunction();

 private:
  ~UsbGetDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetDevicesComplete(
      std::vector<device::mojom::UsbDeviceInfoPtr> devices);

  std::vector<device::mojom::UsbDeviceFilterPtr> filters_;

  DISALLOW_COPY_AND_ASSIGN(UsbGetDevicesFunction);
};

class UsbGetUserSelectedDevicesFunction : public UsbExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.getUserSelectedDevices",
                             USB_GETUSERSELECTEDDEVICES)

  UsbGetUserSelectedDevicesFunction();

 private:
  ~UsbGetUserSelectedDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnDevicesChosen(std::vector<device::mojom::UsbDeviceInfoPtr> devices);

  std::unique_ptr<DevicePermissionsPrompt> prompt_;

  DISALLOW_COPY_AND_ASSIGN(UsbGetUserSelectedDevicesFunction);
};

class UsbGetConfigurationsFunction : public UsbPermissionCheckingFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.getConfigurations", USB_GETCONFIGURATIONS)

  UsbGetConfigurationsFunction();

 private:
  ~UsbGetConfigurationsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UsbGetConfigurationsFunction);
};

class UsbRequestAccessFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.requestAccess", USB_REQUESTACCESS)

  UsbRequestAccessFunction();

 private:
  ~UsbRequestAccessFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UsbRequestAccessFunction);
};

class UsbOpenDeviceFunction : public UsbPermissionCheckingFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.openDevice", USB_OPENDEVICE)

  UsbOpenDeviceFunction();

 private:
  ~UsbOpenDeviceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnDeviceOpened(std::string guid,
                      mojo::Remote<device::mojom::UsbDevice> device,
                      device::mojom::UsbOpenDeviceError error);
  void OnDisconnect();

  DISALLOW_COPY_AND_ASSIGN(UsbOpenDeviceFunction);
};

class UsbSetConfigurationFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.setConfiguration", USB_SETCONFIGURATION)

  UsbSetConfigurationFunction();

 private:
  ~UsbSetConfigurationFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(const std::string& guid, uint8_t config_value, bool success);

  DISALLOW_COPY_AND_ASSIGN(UsbSetConfigurationFunction);
};

class UsbGetConfigurationFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.getConfiguration", USB_GETCONFIGURATION)

  UsbGetConfigurationFunction();

 private:
  ~UsbGetConfigurationFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UsbGetConfigurationFunction);
};

class UsbListInterfacesFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.listInterfaces", USB_LISTINTERFACES)

  UsbListInterfacesFunction();

 private:
  ~UsbListInterfacesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UsbListInterfacesFunction);
};

class UsbCloseDeviceFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.closeDevice", USB_CLOSEDEVICE)

  UsbCloseDeviceFunction();

 private:
  ~UsbCloseDeviceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UsbCloseDeviceFunction);
};

class UsbClaimInterfaceFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.claimInterface", USB_CLAIMINTERFACE)

  UsbClaimInterfaceFunction();

 private:
  ~UsbClaimInterfaceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(bool success);

  DISALLOW_COPY_AND_ASSIGN(UsbClaimInterfaceFunction);
};

class UsbReleaseInterfaceFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.releaseInterface", USB_RELEASEINTERFACE)

  UsbReleaseInterfaceFunction();

 private:
  ~UsbReleaseInterfaceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(bool success);

  DISALLOW_COPY_AND_ASSIGN(UsbReleaseInterfaceFunction);
};

class UsbSetInterfaceAlternateSettingFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.setInterfaceAlternateSetting",
                             USB_SETINTERFACEALTERNATESETTING)

  UsbSetInterfaceAlternateSettingFunction();

 private:
  ~UsbSetInterfaceAlternateSettingFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(bool success);

  DISALLOW_COPY_AND_ASSIGN(UsbSetInterfaceAlternateSettingFunction);
};

class UsbControlTransferFunction : public UsbTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.controlTransfer", USB_CONTROLTRANSFER)

  UsbControlTransferFunction();

 private:
  ~UsbControlTransferFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UsbControlTransferFunction);
};

class UsbBulkTransferFunction : public UsbGenericTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.bulkTransfer", USB_BULKTRANSFER)

  UsbBulkTransferFunction();

 private:
  ~UsbBulkTransferFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UsbBulkTransferFunction);
};

class UsbInterruptTransferFunction : public UsbGenericTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.interruptTransfer", USB_INTERRUPTTRANSFER)

  UsbInterruptTransferFunction();

 private:
  ~UsbInterruptTransferFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UsbInterruptTransferFunction);
};

class UsbIsochronousTransferFunction : public UsbTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.isochronousTransfer", USB_ISOCHRONOUSTRANSFER)

  UsbIsochronousTransferFunction();

 private:
  ~UsbIsochronousTransferFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnTransferInCompleted(
      const std::vector<uint8_t>& data,
      std::vector<device::mojom::UsbIsochronousPacketPtr> packets);
  void OnTransferOutCompleted(
      std::vector<device::mojom::UsbIsochronousPacketPtr> packets);

  DISALLOW_COPY_AND_ASSIGN(UsbIsochronousTransferFunction);
};

class UsbResetDeviceFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.resetDevice", USB_RESETDEVICE)

  UsbResetDeviceFunction();

 private:
  ~UsbResetDeviceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(bool success);

  std::unique_ptr<api::usb::ResetDevice::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(UsbResetDeviceFunction);
};
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_USB_USB_API_H_
