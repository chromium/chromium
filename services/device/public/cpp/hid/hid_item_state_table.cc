// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_item_state_table.h"

#include <limits>

namespace device {

namespace {

bool IsGlobalItem(HidReportDescriptorItem::Tag tag) {
  switch (tag) {
    case HidReportDescriptorItem::kTagUsagePage:
    case HidReportDescriptorItem::kTagLogicalMinimum:
    case HidReportDescriptorItem::kTagLogicalMaximum:
    case HidReportDescriptorItem::kTagPhysicalMinimum:
    case HidReportDescriptorItem::kTagPhysicalMaximum:
    case HidReportDescriptorItem::kTagUnitExponent:
    case HidReportDescriptorItem::kTagUnit:
    case HidReportDescriptorItem::kTagReportSize:
    case HidReportDescriptorItem::kTagReportId:
    case HidReportDescriptorItem::kTagReportCount:
      return true;
    default:
      break;
  }
  return false;
}

uint32_t MaybeCombineUsageAndUsagePage(
    uint32_t usage,
    const std::vector<HidItemStateTable::HidGlobalItemState>& global_stack) {
  // Check if the usage value already has a usage page in the upper bytes.
  if (usage > std::numeric_limits<uint16_t>::max())
    return usage;
  // No global state, just return the usage value.
  if (global_stack.empty())
    return usage;
  // Combine the global usage page with the usage value.
  return (global_stack.back().usage_page << (sizeof(uint16_t) * 8)) | usage;
}

// Interprets |value| as a two's complement signed integer |payload_size| bytes
// wide. The value is returned as a 32-bit signed integer, extending the sign
// bit as needed.
int32_t Int32FromValueAndSize(uint32_t value, size_t payload_size) {
  if (payload_size == 0)
    return 0;

  if (payload_size == 1)
    return static_cast<int8_t>(value & 0xFF);

  if (payload_size == 2)
    return static_cast<int16_t>(value & 0xFFFF);

  DCHECK_EQ(payload_size, 4u);
  return value;
}

}  // namespace

HidItemStateTable::HidItemStateTable() = default;
HidItemStateTable::~HidItemStateTable() = default;

void HidItemStateTable::SetItemValue(HidReportDescriptorItem::Tag tag,
                                     uint32_t value,
                                     size_t payload_size) {
  if (IsGlobalItem(tag)) {
    if (global_stack.empty())
      global_stack.emplace_back();
    auto& global = global_stack.back();
    switch (tag) {
      case HidReportDescriptorItem::kTagUsagePage:
        global.usage_page = value;
        break;
      case HidReportDescriptorItem::kTagLogicalMinimum:
        global.logical_minimum = Int32FromValueAndSize(value, payload_size);
        break;
      case HidReportDescriptorItem::kTagLogicalMaximum:
        global.logical_maximum = Int32FromValueAndSize(value, payload_size);
        break;
      case HidReportDescriptorItem::kTagPhysicalMinimum:
        global.physical_minimum = Int32FromValueAndSize(value, payload_size);
        break;
      case HidReportDescriptorItem::kTagPhysicalMaximum:
        global.physical_maximum = Int32FromValueAndSize(value, payload_size);
        break;
      case HidReportDescriptorItem::kTagUnitExponent:
        global.unit_exponent = value;
        break;
      case HidReportDescriptorItem::kTagUnit:
        global.unit = value;
        break;
      case HidReportDescriptorItem::kTagReportSize:
        global.report_size = value;
        break;
      case HidReportDescriptorItem::kTagReportCount:
        global.report_count = value;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unexpected global item in HID report descriptor";
        break;
    }
  } else {
    switch (tag) {
      case HidReportDescriptorItem::kTagUsage:
        local.usages.push_back(
            MaybeCombineUsageAndUsagePage(value, global_stack));
        break;
      case HidReportDescriptorItem::kTagUsageMinimum:
        local.usage_minimum =
            MaybeCombineUsageAndUsagePage(value, global_stack);
        break;
      case HidReportDescriptorItem::kTagUsageMaximum:
        local.usage_maximum =
            MaybeCombineUsageAndUsagePage(value, global_stack);
        break;
      case HidReportDescriptorItem::kTagDesignatorIndex:
        local.designator_index = value;
        break;
      case HidReportDescriptorItem::kTagDesignatorMinimum:
        local.designator_minimum = value;
        break;
      case HidReportDescriptorItem::kTagDesignatorMaximum:
        local.designator_maximum = value;
        break;
      case HidReportDescriptorItem::kTagStringIndex:
        local.string_index = value;
        break;
      case HidReportDescriptorItem::kTagStringMinimum:
        local.string_minimum = value;
        break;
      case HidReportDescriptorItem::kTagStringMaximum:
        local.string_maximum = value;
        break;
      case HidReportDescriptorItem::kTagDelimiter:
        local.delimiter = value;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unexpected local item in HID report descriptor";
        break;
    }
  }
}

HidItemStateTable::HidGlobalItemState::HidGlobalItemState() = default;
HidItemStateTable::HidGlobalItemState::HidGlobalItemState(
    const HidGlobalItemState&) = default;
HidItemStateTable::HidGlobalItemState::~HidGlobalItemState() = default;

HidItemStateTable::HidLocalItemState::HidLocalItemState() = default;
HidItemStateTable::HidLocalItemState::HidLocalItemState(
    const HidLocalItemState&) = default;
HidItemStateTable::HidLocalItemState::~HidLocalItemState() = default;

void HidItemStateTable::HidLocalItemState::Reset() {
  usages.clear();
  usage_minimum = 0;
  usage_maximum = 0;
  designator_index = 0;
  designator_minimum = 0;
  designator_maximum = 0;
  string_index = 0;
  string_minimum = 0;
  string_maximum = 0;
  delimiter = 0;
}

}  // namespace device
