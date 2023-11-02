// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_ITEM_H_
#define SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_ITEM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "services/device/public/cpp/hid/hid_item_state_table.h"
#include "services/device/public/cpp/hid/hid_report_descriptor_item.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

class HidReportItem {
 public:
  HidReportItem(HidReportDescriptorItem::Tag,
                uint32_t,
                const HidItemStateTable&);
  ~HidReportItem();

  static std::unique_ptr<HidReportItem> Create(HidReportDescriptorItem::Tag tag,
                                               uint32_t short_data,
                                               const HidItemStateTable& state) {
    return std::make_unique<HidReportItem>(tag, short_data, state);
  }

  HidReportDescriptorItem::Tag GetTag() const { return tag_; }

  const HidReportDescriptorItem::ReportInfo& GetReportInfo() const {
    return report_info_;
  }

  // Returns the report ID of the report this item is part of.
  uint8_t GetReportId() const { return report_id_; }

  // Returns true if this item defines a usage range (minimum and maximum), or
  // false if it defines a list of usages.
  bool IsRange() const { return is_range_; }

  // Returns true if the report item is an absolute type, or false if it is a
  // relative type.
  bool IsAbsolute() const { return !report_info_.absolute_or_relative; }

  // Returns true if the report item is an array type. (e.g., keyboard scan
  // codes)
  bool IsArray() const { return !report_info_.array_or_variable; }

  // If |is_range| is false, Usages returns the list of usages associated with
  // this item.
  const std::vector<uint32_t>& GetUsages() const { return local_.usages; }

  // If |is_range| is true, UsageMinimum and UsageMaximum return the minimum and
  // maximum for the range of usages associated with this item.
  uint16_t GetUsageMinimum() const { return local_.usage_minimum; }
  uint16_t GetUsageMaximum() const { return local_.usage_maximum; }

  // If |has_strings| is true, StringMinimum and StringMaximum return the
  // minimum and maximum string indices associated with this item.
  uint16_t GetStringMinimum() const { return local_.string_minimum; }
  uint16_t GetStringMaximum() const { return local_.string_maximum; }

  // If |has_designators| is true, DesignatorMinimum and DesignatorMaximum
  // return the minimum and maximum designator indices associated with this
  // item.
  uint16_t GetDesignatorMinimum() const { return local_.designator_minimum; }
  uint16_t GetDesignatorMaximum() const { return local_.designator_maximum; }

  // Returns true if the item supports reporting a value outside the logical
  // range as a null value.
  bool HasNull() const { return report_info_.null; }

  // Returns the width of each field defined by this item, in bits.
  uint32_t GetReportSize() const { return global_.report_size; }

  // Returns the number of fields defined by this item.
  uint32_t GetReportCount() const { return global_.report_count; }

  // Returns a 32-bit value representing a unit definition for the current item,
  // or 0 if the item is not assigned a unit.
  uint32_t GetUnit() const { return global_.unit; }

  // Returns a value representing the exponent applied to the assigned unit.
  uint32_t GetUnitExponent() const { return global_.unit_exponent; }

  // Returns signed values representing the minimum and maximum values for this
  // item.
  int32_t GetLogicalMinimum() const { return global_.logical_minimum; }
  int32_t GetLogicalMaximum() const { return global_.logical_maximum; }

  // Returns signed values representing the minimum and maximum values for this
  // item in the declared units, for scaling purposes.
  int32_t GetPhysicalMinimum() const { return global_.physical_minimum; }
  int32_t GetPhysicalMaximum() const { return global_.physical_maximum; }

  mojom::HidReportItemPtr ToMojo() const;

 private:
  // The tag of the main item that generated this report item. Must be
  // kItemInput, kItemOutput, or kItemFeature.
  HidReportDescriptorItem::Tag tag_;

  // Data associated with the main item that generated this report item.
  HidReportDescriptorItem::ReportInfo report_info_;

  // The report ID associated with this report, or 0 if none.
  uint8_t report_id_;

  // A copy of the local and global item state when this report item was
  // encountered.
  HidItemStateTable::HidLocalItemState local_;
  HidItemStateTable::HidGlobalItemState global_;

  // If true, the usages for this item are defined by |local.usage_minimum| and
  // |local.usage_maximum|. If false, the usages are defomed by |local.usages|.
  bool is_range_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_ITEM_H_
