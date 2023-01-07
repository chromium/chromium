// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_DESCRIPTOR_H_
#define SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_DESCRIPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "services/device/public/cpp/hid/hid_collection.h"
#include "services/device/public/cpp/hid/hid_report_descriptor_item.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

// HID report descriptor.
// See section 6.2.2 of HID specifications (v1.11).
class HidReportDescriptor {
 public:
  explicit HidReportDescriptor(base::span<const uint8_t> bytes);
  ~HidReportDescriptor();

  const std::vector<std::unique_ptr<HidReportDescriptorItem>>& items() const {
    return items_;
  }

  const std::vector<std::unique_ptr<HidCollection>>& collections() const {
    return collections_;
  }

  // Return the top-level collections present in the descriptor,
  // together with max report sizes.
  void GetDetails(
      std::vector<mojom::HidCollectionInfoPtr>* top_level_collections,
      bool* has_report_id,
      size_t* max_input_report_bytes,
      size_t* max_output_report_bytes,
      size_t* max_feature_report_bytes) const;

 private:
  // An ordered sequence of HidReportDescriptorItem objects representing the
  // items that make up a HID report descriptor.
  std::vector<std::unique_ptr<HidReportDescriptorItem>> items_;

  // A hierarchichal representation of the collections and reports described by
  // the HID report descriptor.
  std::vector<std::unique_ptr<HidCollection>> collections_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_DESCRIPTOR_H_
