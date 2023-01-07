// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_DATA_H_
#define EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_DATA_H_

#include <memory>
#include <string>

#include "extensions/common/permissions/api_permission.h"

namespace base {

class Value;

}  // namespace base

namespace extensions {

// A pattern that can be used to match a USB device permission.
// Should be of the format: vendorId:productId, where both vendorId and
// productId are decimal strings representing uint16_t values.
class UsbDevicePermissionData {
 public:
  enum SpecialValues {
    // A special interface id for stating permissions for an entire USB device,
    // no specific interface. This value must match value of Rule::ANY_INTERFACE
    // from ChromeOS permission_broker project.
    SPECIAL_VALUE_ANY = -1,

    // A special interface id for |Check| to indicate that interface field is
    // not to be checked. Not used in manifest file.
    SPECIAL_VALUE_UNSPECIFIED = -2
  };

  UsbDevicePermissionData();
  UsbDevicePermissionData(int vendor_id,
                          int product_id,
                          int interface_id,
                          int interface_class);

  // Check if |param| (which must be a UsbDevicePermissionData::CheckParam)
  // matches the vendor and product IDs associated with |this|.
  bool Check(const APIPermission::CheckParam* param) const;

  // Convert |this| into a base::Value.
  std::unique_ptr<base::Value> ToValue() const;

  // Populate |this| from a base::Value.
  bool FromValue(const base::Value* value);

  bool operator<(const UsbDevicePermissionData& rhs) const;
  bool operator==(const UsbDevicePermissionData& rhs) const;

  const int& vendor_id() const { return vendor_id_; }
  const int& product_id() const { return product_id_; }
  const int& interface_class() const { return interface_class_; }

  // These accessors are provided for IPC_STRUCT_TRAITS_MEMBER.  Please
  // think twice before using them for anything else.
  int& vendor_id() { return vendor_id_; }
  int& product_id() { return product_id_; }
  int& interface_class() { return interface_class_; }

 private:
  int vendor_id_{SPECIAL_VALUE_ANY};
  int product_id_{SPECIAL_VALUE_ANY};
  // Not useful anymore. Kept around not to trigger permission change warnings
  // for existing apps.
  int interface_id_{SPECIAL_VALUE_ANY};
  int interface_class_{SPECIAL_VALUE_ANY};
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_DATA_H_
