// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_USB_USB_API_H_
#define EXTENSIONS_BROWSER_API_USB_USB_API_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
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

  bool IsUsbDeviceAllowedByPolicy(int vendor_id, int product_id);

 private:
  raw_ptr<UsbDeviceManager> usb_device_manager_ = nullptr;
};

class UsbPermissionCheckingFunction : public UsbExtensionFunction {
 protected:
  UsbPermissionCheckingFunction();
  ~UsbPermissionCheckingFunction() override;

  bool HasDevicePermission(const device::mojom::UsbDeviceInfo& device);
  void RecordDeviceLastUsed();

 private:
  raw_ptr<DevicePermissionsManager> device_permissions_manager_;
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
                   base::Value::Dict transfer_info);
  void OnTransferInCompleted(device::mojom::UsbTransferStatus status,
                             base::span<const uint8_t> data);
  void OnTransferOutCompleted(device::mojom::UsbTransferStatus status);
  void OnDisconnect();
};

class UsbGenericTransferFunction : public UsbTransferFunction {
 protected:
  UsbGenericTransferFunction();
  ~UsbGenericTransferFunction() override;

  // InterruptTransfer::Params and BulkTransfer::Params
  template <typename T>
  ExtensionFunction::ResponseAction DoTransfer(const T& params);
};

class UsbFindDevicesFunction : public UsbExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.findDevices", USB_FINDDEVICES)

  UsbFindDevicesFunction();

  UsbFindDevicesFunction(const UsbFindDevicesFunction&) = delete;
  UsbFindDevicesFunction& operator=(const UsbFindDevicesFunction&) = delete;

 private:
  ~UsbFindDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetDevicesComplete(
      std::vector<device::mojom::UsbDeviceInfoPtr> devices);
  void OnDeviceOpened(const std::string& guid,
                      mojo::Remote<device::mojom::UsbDevice> device_ptr,
                      device::mojom::UsbOpenDeviceResultPtr result);
  void OpenComplete();
  void OnDisconnect();

  uint16_t vendor_id_;
  uint16_t product_id_;
  base::Value::List result_;
  base::RepeatingClosure barrier_;
};

class UsbGetDevicesFunction : public UsbPermissionCheckingFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.getDevices", USB_GETDEVICES)

  UsbGetDevicesFunction();

  UsbGetDevicesFunction(const UsbGetDevicesFunction&) = delete;
  UsbGetDevicesFunction& operator=(const UsbGetDevicesFunction&) = delete;

 private:
  ~UsbGetDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetDevicesComplete(
      std::vector<device::mojom::UsbDeviceInfoPtr> devices);

  std::vector<device::mojom::UsbDeviceFilterPtr> filters_;
};

class UsbGetUserSelectedDevicesFunction : public UsbExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.getUserSelectedDevices",
                             USB_GETUSERSELECTEDDEVICES)

  UsbGetUserSelectedDevicesFunction();

  UsbGetUserSelectedDevicesFunction(const UsbGetUserSelectedDevicesFunction&) =
      delete;
  UsbGetUserSelectedDevicesFunction& operator=(
      const UsbGetUserSelectedDevicesFunction&) = delete;

 private:
  ~UsbGetUserSelectedDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnDevicesChosen(std::vector<device::mojom::UsbDeviceInfoPtr> devices);

  std::unique_ptr<DevicePermissionsPrompt> prompt_;
};

class UsbGetConfigurationsFunction : public UsbPermissionCheckingFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.getConfigurations", USB_GETCONFIGURATIONS)

  UsbGetConfigurationsFunction();

  UsbGetConfigurationsFunction(const UsbGetConfigurationsFunction&) = delete;
  UsbGetConfigurationsFunction& operator=(const UsbGetConfigurationsFunction&) =
      delete;

 private:
  ~UsbGetConfigurationsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UsbRequestAccessFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.requestAccess", USB_REQUESTACCESS)

  UsbRequestAccessFunction();

  UsbRequestAccessFunction(const UsbRequestAccessFunction&) = delete;
  UsbRequestAccessFunction& operator=(const UsbRequestAccessFunction&) = delete;

 private:
  ~UsbRequestAccessFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UsbOpenDeviceFunction : public UsbPermissionCheckingFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.openDevice", USB_OPENDEVICE)

  UsbOpenDeviceFunction();

  UsbOpenDeviceFunction(const UsbOpenDeviceFunction&) = delete;
  UsbOpenDeviceFunction& operator=(const UsbOpenDeviceFunction&) = delete;

 private:
  ~UsbOpenDeviceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnDeviceOpened(std::string guid,
                      mojo::Remote<device::mojom::UsbDevice> device,
                      device::mojom::UsbOpenDeviceResultPtr result);
  void OnDisconnect();
};

class UsbSetConfigurationFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.setConfiguration", USB_SETCONFIGURATION)

  UsbSetConfigurationFunction();

  UsbSetConfigurationFunction(const UsbSetConfigurationFunction&) = delete;
  UsbSetConfigurationFunction& operator=(const UsbSetConfigurationFunction&) =
      delete;

 private:
  ~UsbSetConfigurationFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(const std::string& guid, uint8_t config_value, bool success);
};

class UsbGetConfigurationFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.getConfiguration", USB_GETCONFIGURATION)

  UsbGetConfigurationFunction();

  UsbGetConfigurationFunction(const UsbGetConfigurationFunction&) = delete;
  UsbGetConfigurationFunction& operator=(const UsbGetConfigurationFunction&) =
      delete;

 private:
  ~UsbGetConfigurationFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UsbListInterfacesFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.listInterfaces", USB_LISTINTERFACES)

  UsbListInterfacesFunction();

  UsbListInterfacesFunction(const UsbListInterfacesFunction&) = delete;
  UsbListInterfacesFunction& operator=(const UsbListInterfacesFunction&) =
      delete;

 private:
  ~UsbListInterfacesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UsbCloseDeviceFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.closeDevice", USB_CLOSEDEVICE)

  UsbCloseDeviceFunction();

  UsbCloseDeviceFunction(const UsbCloseDeviceFunction&) = delete;
  UsbCloseDeviceFunction& operator=(const UsbCloseDeviceFunction&) = delete;

 private:
  ~UsbCloseDeviceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UsbClaimInterfaceFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.claimInterface", USB_CLAIMINTERFACE)

  UsbClaimInterfaceFunction();

  UsbClaimInterfaceFunction(const UsbClaimInterfaceFunction&) = delete;
  UsbClaimInterfaceFunction& operator=(const UsbClaimInterfaceFunction&) =
      delete;

 private:
  ~UsbClaimInterfaceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(device::mojom::UsbClaimInterfaceResult result);
};

class UsbReleaseInterfaceFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.releaseInterface", USB_RELEASEINTERFACE)

  UsbReleaseInterfaceFunction();

  UsbReleaseInterfaceFunction(const UsbReleaseInterfaceFunction&) = delete;
  UsbReleaseInterfaceFunction& operator=(const UsbReleaseInterfaceFunction&) =
      delete;

 private:
  ~UsbReleaseInterfaceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(bool success);
};

class UsbSetInterfaceAlternateSettingFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.setInterfaceAlternateSetting",
                             USB_SETINTERFACEALTERNATESETTING)

  UsbSetInterfaceAlternateSettingFunction();

  UsbSetInterfaceAlternateSettingFunction(
      const UsbSetInterfaceAlternateSettingFunction&) = delete;
  UsbSetInterfaceAlternateSettingFunction& operator=(
      const UsbSetInterfaceAlternateSettingFunction&) = delete;

 private:
  ~UsbSetInterfaceAlternateSettingFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(bool success);
};

class UsbControlTransferFunction : public UsbTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.controlTransfer", USB_CONTROLTRANSFER)

  UsbControlTransferFunction();

  UsbControlTransferFunction(const UsbControlTransferFunction&) = delete;
  UsbControlTransferFunction& operator=(const UsbControlTransferFunction&) =
      delete;

 private:
  ~UsbControlTransferFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UsbBulkTransferFunction : public UsbGenericTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.bulkTransfer", USB_BULKTRANSFER)

  UsbBulkTransferFunction();

  UsbBulkTransferFunction(const UsbBulkTransferFunction&) = delete;
  UsbBulkTransferFunction& operator=(const UsbBulkTransferFunction&) = delete;

 private:
  ~UsbBulkTransferFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UsbInterruptTransferFunction : public UsbGenericTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.interruptTransfer", USB_INTERRUPTTRANSFER)

  UsbInterruptTransferFunction();

  UsbInterruptTransferFunction(const UsbInterruptTransferFunction&) = delete;
  UsbInterruptTransferFunction& operator=(const UsbInterruptTransferFunction&) =
      delete;

 private:
  ~UsbInterruptTransferFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UsbIsochronousTransferFunction : public UsbTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.isochronousTransfer", USB_ISOCHRONOUSTRANSFER)

  UsbIsochronousTransferFunction();

  UsbIsochronousTransferFunction(const UsbIsochronousTransferFunction&) =
      delete;
  UsbIsochronousTransferFunction& operator=(
      const UsbIsochronousTransferFunction&) = delete;

 private:
  ~UsbIsochronousTransferFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnTransferInCompleted(
      base::span<const uint8_t> data,
      std::vector<device::mojom::UsbIsochronousPacketPtr> packets);
  void OnTransferOutCompleted(
      std::vector<device::mojom::UsbIsochronousPacketPtr> packets);
};

class UsbResetDeviceFunction : public UsbConnectionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.resetDevice", USB_RESETDEVICE)

  UsbResetDeviceFunction();

  UsbResetDeviceFunction(const UsbResetDeviceFunction&) = delete;
  UsbResetDeviceFunction& operator=(const UsbResetDeviceFunction&) = delete;

 private:
  ~UsbResetDeviceFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComplete(bool success);

  std::optional<api::usb::ResetDevice::Params> parameters_;
};
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_USB_USB_API_H_
