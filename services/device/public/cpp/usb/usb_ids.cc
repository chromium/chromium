// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/usb/usb_ids.h"

#include "base/ranges/algorithm.h"

namespace device {

// static
const UsbVendor* UsbIds::FindVendor(uint16_t vendor_id) {
  const UsbVendor key = {/*name=*/{}, /*products=*/{}, vendor_id};
  auto it = base::ranges::lower_bound(
      vendors_, key, [](const auto& a, const auto& b) { return a.id < b.id; });
  if (it == vendors_.end() || it->id != vendor_id) {
    return nullptr;
  }
  return &*it;
}

// static
const char* UsbIds::GetVendorName(uint16_t vendor_id) {
  const UsbVendor* vendor = FindVendor(vendor_id);
  if (!vendor) {
    return nullptr;
  }
  return vendor->name;
}

// static
const char* UsbIds::GetProductName(uint16_t vendor_id, uint16_t product_id) {
  const UsbVendor* vendor = FindVendor(vendor_id);
  if (!vendor || vendor->products.empty()) {
    return nullptr;
  }

  const UsbProduct key = {product_id, /*name=*/{}};
  auto it = base::ranges::lower_bound(
      vendor->products, key,
      [](const auto& a, const auto& b) { return a.id < b.id; });
  if (it == vendor->products.end() || it->id != product_id) {
    return nullptr;
  }
  return it->name;
}

}  // namespace device
