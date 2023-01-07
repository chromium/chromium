// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_device_filter.h"

namespace device {

HidDeviceFilter::HidDeviceFilter()
    : vendor_id_set_(false),
      product_id_set_(false),
      usage_page_set_(false),
      usage_set_(false) {}

HidDeviceFilter::~HidDeviceFilter() {}

void HidDeviceFilter::SetVendorId(uint16_t vendor_id) {
  vendor_id_set_ = true;
  vendor_id_ = vendor_id;
}

void HidDeviceFilter::SetProductId(uint16_t product_id) {
  product_id_set_ = true;
  product_id_ = product_id;
}

void HidDeviceFilter::SetUsagePage(uint16_t usage_page) {
  usage_page_set_ = true;
  usage_page_ = usage_page;
}

void HidDeviceFilter::SetUsage(uint16_t usage) {
  usage_set_ = true;
  usage_ = usage;
}

bool HidDeviceFilter::Matches(const mojom::HidDeviceInfo& device_info) const {
  if (vendor_id_set_) {
    if (device_info.vendor_id != vendor_id_) {
      return false;
    }

    if (product_id_set_ && device_info.product_id != product_id_) {
      return false;
    }
  }

  if (usage_page_set_) {
    bool found_matching_collection = false;
    for (const auto& collection : device_info.collections) {
      if (collection->usage->usage_page != usage_page_) {
        continue;
      }
      if (usage_set_ && collection->usage->usage != usage_) {
        continue;
      }
      found_matching_collection = true;
    }
    if (!found_matching_collection) {
      return false;
    }
  }

  return true;
}

// static
bool HidDeviceFilter::MatchesAny(const mojom::HidDeviceInfo& device_info,
                                 const std::vector<HidDeviceFilter>& filters) {
  for (const HidDeviceFilter& filter : filters) {
    if (filter.Matches(device_info)) {
      return true;
    }
  }
  return false;
}

}  // namespace device
