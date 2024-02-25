// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/usb_device_permission_data.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"

namespace extensions {

namespace {

const char kProductIdKey[] = "productId";
const char kVendorIdKey[] = "vendorId";
const char kInterfaceIdKey[] = "interfaceId";
const char kInterfaceClassKey[] = "interfaceClass";

bool ExtractFromDict(const std::string& key,
                     const base::Value::Dict* dict_value,
                     int max,
                     int* value) {
  std::optional<int> temp = dict_value->FindInt(key);
  if (!temp) {
    *value = UsbDevicePermissionData::SPECIAL_VALUE_ANY;
    return true;
  }

  if (*temp < UsbDevicePermissionData::SPECIAL_VALUE_ANY || *temp > max)
    return false;

  *value = *temp;
  return true;
}

}  // namespace

UsbDevicePermissionData::UsbDevicePermissionData() = default;

UsbDevicePermissionData::UsbDevicePermissionData(int vendor_id,
                                                 int product_id,
                                                 int interface_id,
                                                 int interface_class)
    : vendor_id_(vendor_id),
      product_id_(product_id),
      interface_id_(interface_id),
      interface_class_(interface_class) {}

bool UsbDevicePermissionData::Check(
    const APIPermission::CheckParam* param) const {
  if (!param)
    return false;
  const UsbDevicePermission::CheckParam& specific_param =
      *static_cast<const UsbDevicePermission::CheckParam*>(param);

  // The permission should be ignored if it filters by interface class when
  // filtering by interface class is not allowed.
  if (!specific_param.interface_class_allowed &&
      interface_class_ != SPECIAL_VALUE_ANY) {
    return false;
  }
  DCHECK(specific_param.interface_class_allowed ||
         (vendor_id_ != SPECIAL_VALUE_ANY && product_id_ != SPECIAL_VALUE_ANY));

  return (vendor_id_ == SPECIAL_VALUE_ANY ||
          vendor_id_ == specific_param.vendor_id) &&
         (product_id_ == SPECIAL_VALUE_ANY ||
          product_id_ == specific_param.product_id) &&
         (specific_param.interface_id == SPECIAL_VALUE_UNSPECIFIED ||
          interface_id_ == specific_param.interface_id) &&
         (!specific_param.interface_classes ||
          interface_class_ == SPECIAL_VALUE_ANY ||
          specific_param.interface_classes->count(interface_class_) > 0);
}

std::unique_ptr<base::Value> UsbDevicePermissionData::ToValue() const {
  base::Value::Dict result;
  result.Set(kVendorIdKey, vendor_id_);
  result.Set(kProductIdKey, product_id_);
  result.Set(kInterfaceIdKey, interface_id_);
  result.Set(kInterfaceClassKey, interface_class_);
  return std::make_unique<base::Value>(std::move(result));
}

bool UsbDevicePermissionData::FromValue(const base::Value* value) {
  if (!value)
    return false;

  const base::Value::Dict* dict_value = value->GetIfDict();
  if (!dict_value)
    return false;

  const int kMaxId = std::numeric_limits<uint16_t>::max();
  if (!ExtractFromDict(kVendorIdKey, dict_value, kMaxId, &vendor_id_))
    return false;

  if (!ExtractFromDict(kProductIdKey, dict_value, kMaxId, &product_id_))
    return false;
  // If product ID is specified, so should be vendor ID.
  if (product_id_ != SPECIAL_VALUE_ANY && vendor_id_ == SPECIAL_VALUE_ANY)
    return false;

  const int kMaxInterfaceData = std::numeric_limits<uint8_t>::max();
  if (!ExtractFromDict(kInterfaceIdKey, dict_value, kMaxInterfaceData,
                       &interface_id_)) {
    return false;
  }
  // If interface ID is specified, so should be vendor ID and product ID (note
  // that product ID being set implies that vendor ID is set).
  if (interface_id_ != SPECIAL_VALUE_ANY && product_id_ == SPECIAL_VALUE_ANY)
    return false;

  if (!ExtractFromDict(kInterfaceClassKey, dict_value, kMaxInterfaceData,
                       &interface_class_)) {
    return false;
  }

  // Reject the permission if neither interface class nor vendor ID, product ID
  // pair is specified in the permission.
  // Note that set product ID implies that vendor ID is set as well, so only
  // product ID has to be checked.
  if (interface_class_ == SPECIAL_VALUE_ANY && product_id_ == SPECIAL_VALUE_ANY)
    return false;

  // Interface ID is ignored, but kept for backward compatibility - don't allow
  // it's usage with interface class, as interface class property was introduced
  // after interface ID support was dropped.
  if (interface_class_ != SPECIAL_VALUE_ANY &&
      interface_id_ != SPECIAL_VALUE_ANY) {
    return false;
  }

  return true;
}

bool UsbDevicePermissionData::operator<(
    const UsbDevicePermissionData& rhs) const {
  return std::tie(vendor_id_, product_id_, interface_id_, interface_class_) <
         std::tie(rhs.vendor_id_, rhs.product_id_, rhs.interface_id_,
                  rhs.interface_class_);
}

bool UsbDevicePermissionData::operator==(
    const UsbDevicePermissionData& rhs) const {
  return vendor_id_ == rhs.vendor_id_ && product_id_ == rhs.product_id_ &&
         interface_id_ == rhs.interface_id_ &&
         interface_class_ == rhs.interface_class_;
}

}  // namespace extensions
