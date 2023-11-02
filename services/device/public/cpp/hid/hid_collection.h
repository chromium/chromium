// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_HID_HID_COLLECTION_H_
#define SERVICES_DEVICE_PUBLIC_CPP_HID_HID_COLLECTION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "services/device/public/cpp/hid/hid_report_descriptor_item.h"
#include "services/device/public/cpp/hid/hid_report_item.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

class HidItemStateTable;

// Information about a single HID collection.
class HidCollection {
 public:
  using HidReport = std::vector<std::unique_ptr<HidReportItem>>;

  HidCollection(HidCollection* parent,
                uint32_t usage_page,
                uint32_t usage,
                uint32_t collection_type);
  ~HidCollection();

  static std::vector<std::unique_ptr<HidCollection>> BuildCollections(
      const std::vector<std::unique_ptr<HidReportDescriptorItem>>& items);

  uint16_t GetUsagePage() const { return usage_.usage_page; }

  uint16_t GetUsage() const { return usage_.usage; }

  uint32_t GetCollectionType() const { return collection_type_; }

  // Return true if there are one or more report IDs associated with this
  // collection.
  bool HasReportId() const { return !report_ids_.empty(); }

  // Compute the maximum size of any input, output, or feature report described
  // by this collection.
  void GetMaxReportSizes(size_t* max_input_report_bits,
                         size_t* max_output_report_bits,
                         size_t* max_feature_report_bits) const;

  mojom::HidCollectionInfoPtr ToMojo() const;

  const HidCollection* GetParent() const { return parent_; }

  const std::vector<std::unique_ptr<HidCollection>>& GetChildren() const {
    return children_;
  }

  const std::unordered_map<uint8_t, HidReport>& GetInputReports() const {
    return input_reports_;
  }

  const std::unordered_map<uint8_t, HidReport>& GetOutputReports() const {
    return output_reports_;
  }

  const std::unordered_map<uint8_t, HidReport>& GetFeatureReports() const {
    return feature_reports_;
  }

  // Exposed for testing.
  void AddChildForTesting(std::unique_ptr<HidCollection> child);
  void AddReportItem(const HidReportDescriptorItem::Tag tag,
                     uint32_t report_info,
                     const HidItemStateTable& state);

 private:
  static void AddCollection(
      const HidReportDescriptorItem& item,
      std::vector<std::unique_ptr<HidCollection>>& collections,
      HidItemStateTable& state);

  // The parent collection, or nullptr if this is a top level collection.
  const raw_ptr<HidCollection> parent_;

  // The children of this collection in the order they were encountered in the
  // report descriptor.
  std::vector<std::unique_ptr<HidCollection>> children_;

  // The usage page and usage ID associated with this collection.
  const mojom::HidUsageAndPage usage_;

  // The type of this collection. Stored as an integer type rather than an enum
  // to preserve reserved and vendor-defined values.
  const uint32_t collection_type_;

  // The sequence of report IDs associated with this collection in the order
  // they were encountered in the report descriptor.
  std::vector<uint8_t> report_ids_;

  // Maps from report IDs to sequences of report items. Reports are divided by
  // type (input, output, or feature).
  std::unordered_map<uint8_t, HidReport> input_reports_;
  std::unordered_map<uint8_t, HidReport> output_reports_;
  std::unordered_map<uint8_t, HidReport> feature_reports_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_HID_HID_COLLECTION_H_
