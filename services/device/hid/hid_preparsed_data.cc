// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_preparsed_data.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"
#include "base/notreached.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

namespace {

using PreparsedDataHeader = HidPreparsedData::PreparsedDataHeader;
using PreparsedDataItem = HidPreparsedData::PreparsedDataItem;

static_assert(sizeof(PreparsedDataHeader) == 44);
static_assert(sizeof(PreparsedDataItem::ButtonData) == 8);
static_assert(sizeof(PreparsedDataItem::ValueData) == 20);
static_assert(sizeof(PreparsedDataItem::Data) == 20);
static_assert(sizeof(PreparsedDataItem) == 104);
static_assert(offsetof(PreparsedDataItem, flags) == 24);
static_assert(offsetof(PreparsedDataItem, data) == 76);

constexpr uint32_t kPreparsedDataItemIsButton = 1 << 2;

bool ValidatePreparsedDataHeader(const PreparsedDataHeader& header) {
  static bool has_dumped_without_crashing = false;

  // Require a matching magic value. The details of _HIDP_PREPARSED_DATA are
  // proprietary and the magic constant may change. If DCHECKS are on, trigger
  // a CHECK failure and crash. Otherwise, generate a non-crash dump.
  DCHECK_EQ(header.magic, HidPreparsedData::kHidPreparsedDataMagic);
  if (header.magic != HidPreparsedData::kHidPreparsedDataMagic) {
    HID_LOG(ERROR) << "Unexpected magic value.";
    if (has_dumped_without_crashing) {
      base::debug::DumpWithoutCrashing();
      has_dumped_without_crashing = true;
    }
    return false;
  }

  if (header.input_report_byte_length == 0 && header.input_item_count > 0)
    return false;
  if (header.output_report_byte_length == 0 && header.output_item_count > 0)
    return false;
  if (header.feature_report_byte_length == 0 && header.feature_item_count > 0)
    return false;

  // Calculate the expected total size of report items in the
  // _HIDP_PREPARSED_DATA object. Use the individual item counts for each report
  // type instead of the total |item_count|. In some cases additional items are
  // allocated but are not used for any reports. Unused items are excluded from
  // |item_count| but are included in the item counts for each report type and
  // contribute to the total size of the object. See crbug.com/1199890 for more
  // information.
  uint16_t total_item_size =
      (header.input_item_count + header.output_item_count +
       header.feature_item_count) *
      sizeof(PreparsedDataItem);
  if (total_item_size != header.size_bytes)
    return false;
  return true;
}

bool ValidatePreparsedDataItem(const PreparsedDataItem& item) {
  // Check that the item does not overlap with the report ID byte.
  if (item.byte_index == 0)
    return false;

  // Check that the bit index does not exceed the maximum bit index in one byte.
  if (item.bit_index >= CHAR_BIT)
    return false;

  // Check that the item occupies at least one bit in the report.
  if (item.report_count == 0 || item.bit_size == 0 || item.bit_count == 0)
    return false;

  return true;
}

HidServiceWin::PreparsedData::ReportItem MakeReportItemFromPreparsedData(
    const PreparsedDataItem& item) {
  int32_t logical_minimum;
  int32_t logical_maximum;
  int32_t physical_minimum;
  int32_t physical_maximum;
  if (item.flags & kPreparsedDataItemIsButton) {
    logical_minimum = item.data.button.logical_minimum;
    logical_maximum = item.data.button.logical_maximum;
    physical_minimum = 0;
    physical_maximum = 0;
  } else {
    logical_minimum = item.data.value.logical_minimum;
    logical_maximum = item.data.value.logical_maximum;
    physical_minimum = item.data.value.physical_minimum;
    physical_maximum = item.data.value.physical_maximum;
  }

  size_t bit_index = (item.byte_index - 1) * CHAR_BIT + item.bit_index;
  return {item.report_id,      item.bit_field,          item.bit_size,
          item.report_count,   item.usage_page,         item.usage_minimum,
          item.usage_maximum,  item.designator_minimum, item.designator_maximum,
          item.string_minimum, item.string_maximum,     logical_minimum,
          logical_maximum,     physical_minimum,        physical_maximum,
          item.unit,           item.unit_exponent,      bit_index};
}

}  // namespace

// static
std::unique_ptr<HidPreparsedData> HidPreparsedData::Create(
    HANDLE device_handle) {
  PHIDP_PREPARSED_DATA preparsed_data;
  if (!HidD_GetPreparsedData(device_handle, &preparsed_data) ||
      !preparsed_data) {
    HID_PLOG(EVENT) << "Failed to get device data";
    return nullptr;
  }

  HIDP_CAPS capabilities;
  if (HidP_GetCaps(preparsed_data, &capabilities) != HIDP_STATUS_SUCCESS) {
    HID_PLOG(EVENT) << "Failed to get device capabilities";
    HidD_FreePreparsedData(preparsed_data);
    return nullptr;
  }

  return std::make_unique<HidPreparsedData>(preparsed_data, capabilities,
                                            /*owns_preparsed_data=*/true);
}

// static
std::unique_ptr<HidPreparsedData> HidPreparsedData::CreateForTesting(
    PHIDP_PREPARSED_DATA preparsed_data,
    HIDP_CAPS capabilities) {
  return std::make_unique<HidPreparsedData>(preparsed_data, capabilities,
                                            /*owns_preparsed_data=*/false);
}

HidPreparsedData::HidPreparsedData(PHIDP_PREPARSED_DATA preparsed_data,
                                   HIDP_CAPS capabilities,
                                   bool owns_preparsed_data)
    : preparsed_data_(preparsed_data),
      capabilities_(capabilities),
      owns_preparsed_data_(owns_preparsed_data) {
  DCHECK(preparsed_data_);
}

HidPreparsedData::~HidPreparsedData() {
  if (owns_preparsed_data_) {
    HidD_FreePreparsedData(preparsed_data_);
  }
}

const HIDP_CAPS& HidPreparsedData::GetCaps() const {
  return capabilities_;
}

uint32_t HidPreparsedData::GetCollectionType() const {
  ULONG node_count = capabilities_.NumberLinkCollectionNodes;
  if (node_count == 0) {
    return mojom::kHIDCollectionTypeApplication;
  }

  std::vector<HIDP_LINK_COLLECTION_NODE> nodes(node_count);
  if (HidP_GetLinkCollectionNodes(nodes.data(), &node_count, preparsed_data_) !=
          HIDP_STATUS_SUCCESS ||
      node_count == 0) {
    HID_LOG(ERROR) << "Failed to get HID link collection nodes.";
    return mojom::kHIDCollectionTypeApplication;
  }
  return nodes[0].CollectionType;
}

std::vector<HidServiceWin::PreparsedData::ReportItem>
HidPreparsedData::GetReportItems(HIDP_REPORT_TYPE report_type) const {
  const auto& header =
      *reinterpret_cast<const PreparsedDataHeader*>(preparsed_data_);
  if (!ValidatePreparsedDataHeader(header))
    return {};

  size_t min_index;
  size_t item_count;
  switch (report_type) {
    case HidP_Input:
      min_index = 0;
      item_count = header.input_item_count;
      break;
    case HidP_Output:
      min_index = header.input_item_count;
      item_count = header.output_item_count;
      break;
    case HidP_Feature:
      min_index = header.input_item_count + header.output_item_count;
      item_count = header.feature_item_count;
      break;
    default:
      return {};
  }
  if (item_count == 0)
    return {};

  const auto* data = reinterpret_cast<const uint8_t*>(preparsed_data_);
  const auto* items = reinterpret_cast<const PreparsedDataItem*>(
      UNSAFE_TODO(data + sizeof(PreparsedDataHeader)));
  std::vector<ReportItem> report_items;
  for (size_t i = min_index; i < min_index + item_count; ++i) {
    if (ValidatePreparsedDataItem(UNSAFE_TODO(items[i]))) {
      report_items.push_back(
          MakeReportItemFromPreparsedData(UNSAFE_TODO(items[i])));
    }
  }

  return report_items;
}

}  // namespace device
