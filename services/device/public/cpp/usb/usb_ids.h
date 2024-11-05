// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_USB_USB_IDS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_USB_USB_IDS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"

namespace device {

struct UsbProduct {
  const uint16_t id;
  const char* name;
};

// This structure is used in an array so the cumulative size is significant.
// Order fields to minimize/eliminate alignment padding.
// Chose field size based on contained data to further reduce structure size.
// For example, uint16_t instead of size_t.
struct UsbVendor {
  const char* name;
  // TODO(367764863) Rewrite to base::raw_span.
  RAW_PTR_EXCLUSION const base::span<const UsbProduct> products;
  const uint16_t id;
};

// UsbIds provides a static mapping from a vendor ID to a name, as well as a
// mapping from a vendor/product ID pair to a product name.
class UsbIds {
 public:
  UsbIds(const UsbIds&) = delete;
  UsbIds& operator=(const UsbIds&) = delete;

  // Gets the name of the vendor who owns |vendor_id|. Returns nullptr if the
  // specified |vendor_id| does not exist.
  static const char* GetVendorName(uint16_t vendor_id);

  // Gets the name of a product belonging to a specific vendor. If either
  // |vendor_id| refers to a vendor that does not exist, or |vendor_id| is valid
  // but |product_id| refers to a product that does not exist, this method
  // returns nullptr.
  static const char* GetProductName(uint16_t vendor_id, uint16_t product_id);

 private:
  UsbIds();
  ~UsbIds();

  // Finds the static UsbVendor associated with |vendor_id|. Returns nullptr if
  // no such vendor exists.
  static const UsbVendor* FindVendor(uint16_t vendor_id);

  // This field is defined in a generated file. See
  // services/device/public/cpp/usb/BUILD.gn for more information on how it is
  // generated.
  static const base::span<const UsbVendor> vendors_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_USB_USB_IDS_H_
