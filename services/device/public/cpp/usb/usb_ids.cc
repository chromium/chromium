// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/usb/usb_ids.h"

#include <stdlib.h>

namespace device {

namespace {

static int CompareVendors(const void* a, const void* b) {
  const UsbVendor* vendor_a = static_cast<const UsbVendor*>(a);
  const UsbVendor* vendor_b = static_cast<const UsbVendor*>(b);
  return vendor_a->id - vendor_b->id;
}

static int CompareProducts(const void* a, const void* b) {
  const UsbProduct* product_a = static_cast<const UsbProduct*>(a);
  const UsbProduct* product_b = static_cast<const UsbProduct*>(b);
  return product_a->id - product_b->id;
}

}  // namespace

const UsbVendor* UsbIds::FindVendor(uint16_t vendor_id) {
  const UsbVendor key = {nullptr, nullptr, vendor_id, 0};
  void* result = bsearch(&key, vendors_, vendor_size_, sizeof(vendors_[0]),
                         &CompareVendors);
  if (!result)
    return NULL;
  return static_cast<const UsbVendor*>(result);
}

const char* UsbIds::GetVendorName(uint16_t vendor_id) {
  const UsbVendor* vendor = FindVendor(vendor_id);
  if (!vendor)
    return NULL;
  return vendor->name;
}

const char* UsbIds::GetProductName(uint16_t vendor_id, uint16_t product_id) {
  const UsbVendor* vendor = FindVendor(vendor_id);
  if (!vendor || !vendor->products)
    return NULL;

  const UsbProduct key = {product_id, nullptr};
  void* result = bsearch(&key, vendor->products, vendor->product_size,
                         sizeof(vendor->products[0]), &CompareProducts);
  if (!result)
    return NULL;
  return static_cast<const UsbProduct*>(result)->name;
}

}  // namespace device
