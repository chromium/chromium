// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/set_disjunction_permission.h"
#include "extensions/common/permissions/usb_device_permission_data.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace extensions {

class Extension;

class UsbDevicePermission
  : public SetDisjunctionPermission<UsbDevicePermissionData,
                                    UsbDevicePermission> {
 public:
  struct CheckParam : public APIPermission::CheckParam {
    static std::unique_ptr<CheckParam> ForUsbDevice(
        const Extension* extension,
        const device::mojom::UsbDeviceInfo& device_info);
    // Creates check param that only checks vendor, product and interface ID
    // permission properties. It will accept all interfaceClass properties. For
    // example, created param would always accept {"intefaceClass": 3}
    // permission, and it would accept {"vendorId": 2, "interfaceClass": 4} iff
    // |vendor_id| is 2.
    // Created check param failing means there are no permissions allowing
    // access to the USB device. On the other hand, created check param passing
    // does not necessarily mean there is a permission allowing access to the
    // USB device - one should recheck the permission when complete USB device
    // info is known.
    // This is useful when trying to discard devices that do not match any
    // usbDevice permission when complete USB device info is not known, without
    // having to fetch available USB devices.
    static std::unique_ptr<CheckParam> ForDeviceWithAnyInterfaceClass(
        const Extension* extension,
        uint16_t vendor_id,
        uint16_t product_id,
        int interface_id);
    static std::unique_ptr<CheckParam> ForUsbDeviceAndInterface(
        const Extension* extension,
        const device::mojom::UsbDeviceInfo& device_info,
        int interface_id);
    static std::unique_ptr<CheckParam> ForHidDevice(const Extension* extension,
                                                    uint16_t vendor_id,
                                                    uint16_t product_id);

    CheckParam(const Extension* extension,
               uint16_t vendor_id,
               uint16_t product_id,
               std::unique_ptr<std::set<int>> interface_classes,
               int interface_id);

    CheckParam(const CheckParam&) = delete;
    CheckParam& operator=(const CheckParam&) = delete;

    ~CheckParam();

    const uint16_t vendor_id;
    const uint16_t product_id;
    const std::unique_ptr<std::set<int>> interface_classes;
    const int interface_id;
    const bool interface_class_allowed;
  };

  explicit UsbDevicePermission(const APIPermissionInfo* info);
  ~UsbDevicePermission() override;

  // SetDisjunctionPermission overrides.
  bool FromValue(const base::Value* value,
                 std::string* error,
                 std::vector<std::string>* unhandled_permissions) override;

  // APIPermission overrides
  PermissionIDSet GetPermissions() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_H_
