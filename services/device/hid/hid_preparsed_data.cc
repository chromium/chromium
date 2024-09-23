// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/hid/hid_preparsed_data.h"

#include <cstddef>
#include <cstdint>

#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/device_event_log/device_event_log.h"

namespace device {

namespace {

// Windows parses HID report descriptors into opaque _HIDP_PREPARSED_DATA
// objects. The internal structure of _HIDP_PREPARSED_DATA is reserved for
// internal system use. The structs below are inferred and may be wrong or
// incomplete.
// https://docs.microsoft.com/en-us/windows-hardware/drivers/hid/preparsed-data
//
// _HIDP_PREPARSED_DATA begins with a fixed-sized header containing information
// about a single top-level HID collection. The header is followed by a
// variable-sized array describing the fields that make up each report.
//
// Input report items appear first in the array, followed by output report items
// and feature report items. The number of items of each type is given by
// |input_item_count|, |output_item_count| and |feature_item_count|. The sum of
// these counts should equal |item_count|. The total size in bytes of all report
// items is |size_bytes|.
#pragma pack(push, 1)
struct PreparsedDataHeader {
  // Unknown constant value. _HIDP_PREPARSED_DATA identifier?
  uint64_t magic;

  // Top-level collection usage information.
  uint16_t usage;
  uint16_t usage_page;

  uint16_t unknown[3];

  // Number of report items for input reports. Includes unused items.
  uint16_t input_item_count;

  uint16_t unknown2;

  // Maximum input report size, in bytes. Includes the report ID byte. Zero if
  // there are no input reports.
  uint16_t input_report_byte_length;

  uint16_t unknown3;

  // Number of report items for output reports. Includes unused items.
  uint16_t output_item_count;

  uint16_t unknown4;

  // Maximum output report size, in bytes. Includes the report ID byte. Zero if
  // there are no output reports.
  uint16_t output_report_byte_length;

  uint16_t unknown5;

  // Number of report items for feature reports. Includes unused items.
  uint16_t feature_item_count;

  // Total number of report items (input, output, and feature). Unused items are
  // excluded.
  uint16_t item_count;

  // Maximum feature report size, in bytes. Includes the report ID byte. Zero if
  // there are no feature reports.
  uint16_t feature_report_byte_length;

  // Total size of all report items, in bytes.
  uint16_t size_bytes;

  uint16_t unknown6;
};
#pragma pack(pop)
static_assert(sizeof(PreparsedDataHeader) == 44,
              "PreparsedDataHeader has incorrect size");

#pragma pack(push, 1)
struct PreparsedDataItem {
  // Usage page for |usage_minimum| and |usage_maximum|.
  uint16_t usage_page;

  // Report ID for the report containing this item.
  uint8_t report_id;

  // Bit offset from |byte_index|.
  uint8_t bit_index;

  // Bit width of a single field defined by this item.
  uint16_t bit_size;

  // The number of fields defined by this item.
  uint16_t report_count;

  // Byte offset from the start of the report containing this item, including
  // the report ID byte.
  uint16_t byte_index;

  // The total number of bits for all fields defined by this item.
  uint16_t bit_count;

  // The bit field for the corresponding main item in the HID report. This bit
  // field is defined in the Device Class Definition for HID v1.11 section
  // 6.2.2.5.
  // https://www.usb.org/document-library/device-class-definition-hid-111
  uint32_t bit_field;

  uint32_t unknown;

  // Usage information for the collection containing this item.
  uint16_t link_usage_page;
  uint16_t link_usage;

  uint32_t unknown2[9];

  // The usage range for this item.
  uint16_t usage_minimum;
  uint16_t usage_maximum;

  // The string descriptor index range associated with this item. If the item
  // has no string descriptors, |string_minimum| and |string_maximum| are set to
  // zero.
  uint16_t string_minimum;
  uint16_t string_maximum;

  // The designator index range associated with this item. If the item has no
  // designators, |designator_minimum| and |designator_maximum| are set to zero.
  uint16_t designator_minimum;
  uint16_t designator_maximum;

  // The data index range associated with this item.
  uint16_t data_index_minimum;
  uint16_t data_index_maximum;

  uint32_t unknown3;

  // The range of fields defined by this item in logical units.
  int32_t logical_minimum;
  int32_t logical_maximum;

  // The range of fields defined by this item in units defined by |unit| and
  // |unit_exponent|. If this item does not use physical units,
  // |physical_minimum| and |physical_maximum| are set to zero.
  int32_t physical_minimum;
  int32_t physical_maximum;

  // The unit definition for this item. The format for this definition is
  // described in the Device Class Definition for HID v1.11 section 6.2.2.7.
  // https://www.usb.org/document-library/device-class-definition-hid-111
  uint32_t unit;
  uint32_t unit_exponent;
};
#pragma pack(pop)
static_assert(sizeof(PreparsedDataItem) == 104,
              "PreparsedDataItem has incorrect size");

bool ValidatePreparsedDataHeader(const PreparsedDataHeader& header) {
  static bool has_dumped_without_crashing = false;

  // _HIDP_PREPARSED_DATA objects are expected to start with a known constant
  // value.
  constexpr uint64_t kHidPreparsedDataMagic = 0x52444B2050646948;

  // Require a matching magic value. The details of _HIDP_PREPARSED_DATA are
  // proprietary and the magic constant may change. If DCHECKS are on, trigger
  // a CHECK failure and crash. Otherwise, generate a non-crash dump.
  DCHECK_EQ(header.magic, kHidPreparsedDataMagic);
  if (header.magic != kHidPreparsedDataMagic) {
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
  size_t bit_index = (item.byte_index - 1) * CHAR_BIT + item.bit_index;
  return {item.report_id,          item.bit_field,
          item.bit_size,           item.report_count,
          item.usage_page,         item.usage_minimum,
          item.usage_maximum,      item.designator_minimum,
          item.designator_maximum, item.string_minimum,
          item.string_maximum,     item.logical_minimum,
          item.logical_maximum,    item.physical_minimum,
          item.physical_maximum,   item.unit,
          item.unit_exponent,      bit_index};
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

  return base::WrapUnique(new HidPreparsedData(preparsed_data, capabilities));
}

HidPreparsedData::HidPreparsedData(PHIDP_PREPARSED_DATA preparsed_data,
                                   HIDP_CAPS capabilities)
    : preparsed_data_(preparsed_data), capabilities_(capabilities) {
  DCHECK(preparsed_data_);
}

HidPreparsedData::~HidPreparsedData() {
  HidD_FreePreparsedData(preparsed_data_);
}

const HIDP_CAPS& HidPreparsedData::GetCaps() const {
  return capabilities_;
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
      data + sizeof(PreparsedDataHeader));
  std::vector<ReportItem> report_items;
  for (size_t i = min_index; i < min_index + item_count; ++i) {
    if (ValidatePreparsedDataItem(items[i]))
      report_items.push_back(MakeReportItemFromPreparsedData(items[i]));
  }

  return report_items;
}

}  // namespace device
