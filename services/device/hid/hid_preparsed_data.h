// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_PREPARSED_DATA_H_
#define SERVICES_DEVICE_HID_HID_PREPARSED_DATA_H_

#include <windows.h>

// NOTE: <hidsdi.h> must be included before <hidpi.h>. clang-format will want to
// reorder them.
// clang-format off
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
// clang-format on

#include <cstdint>
#include <memory>
#include <vector>

#include "services/device/hid/hid_service_win.h"

namespace device {

class HidPreparsedData : public HidServiceWin::PreparsedData {
 public:
  static constexpr uint64_t kHidPreparsedDataMagic = 0x52444B2050646948;

  // Windows parses HID report descriptors into opaque _HIDP_PREPARSED_DATA
  // objects. The internal structure of _HIDP_PREPARSED_DATA is reserved for
  // internal system use. These layouts are inferred and may be wrong or
  // incomplete.
  // https://docs.microsoft.com/en-us/windows-hardware/drivers/hid/preparsed-data
  //
  // _HIDP_PREPARSED_DATA begins with a fixed-sized header containing
  // information about a single top-level HID collection. The header is
  // followed by a variable-sized array describing the fields that make up each
  // report.
  //
  // Input report items appear first in the array, followed by output report
  // items and feature report items. The number of items of each type is given
  // by |input_item_count|, |output_item_count| and |feature_item_count|. The
  // sum of these counts should equal |item_count|. The total size in bytes of
  // all report items is |size_bytes|.
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

    // Maximum output report size, in bytes. Includes the report ID byte. Zero
    // if there are no output reports.
    uint16_t output_report_byte_length;

    uint16_t unknown5;

    // Number of report items for feature reports. Includes unused items.
    uint16_t feature_item_count;

    // Total number of report items (input, output, and feature). Unused items
    // are excluded.
    uint16_t item_count;

    // Maximum feature report size, in bytes. Includes the report ID byte. Zero
    // if there are no feature reports.
    uint16_t feature_report_byte_length;

    // Total size of all report items, in bytes.
    uint16_t size_bytes;

    uint16_t unknown6;
  };

  struct PreparsedDataItem {
    struct ButtonData {
      int32_t logical_minimum;
      int32_t logical_maximum;
    };

    struct ValueData {
      uint8_t has_null;
      uint8_t reserved[3];
      int32_t logical_minimum;
      int32_t logical_maximum;
      int32_t physical_minimum;
      int32_t physical_maximum;
    };

    union Data {
      ButtonData button;
      ValueData value;
    };

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

    // Internal flags describing this item.
    uint32_t flags;

    uint32_t unknown2[8];

    // The usage range for this item.
    uint16_t usage_minimum;
    uint16_t usage_maximum;

    // The string descriptor index range associated with this item. If the item
    // has no string descriptors, |string_minimum| and |string_maximum| are set
    // to zero.
    uint16_t string_minimum;
    uint16_t string_maximum;

    // The designator index range associated with this item. If the item has no
    // designators, |designator_minimum| and |designator_maximum| are set to
    // zero.
    uint16_t designator_minimum;
    uint16_t designator_maximum;

    // The data index range associated with this item.
    uint16_t data_index_minimum;
    uint16_t data_index_maximum;

    // Button items and value items store their logical and physical ranges at
    // different offsets.
    Data data;

    // The unit definition for this item. The format for this definition is
    // described in the Device Class Definition for HID v1.11 section 6.2.2.7.
    // https://www.usb.org/document-library/device-class-definition-hid-111
    uint32_t unit;
    uint32_t unit_exponent;
  };
#pragma pack(pop)

  // Return a HidPreparsedData constructed from an open |device_handle|, or
  // nullptr if the handle is invalid or the device data could not be read.
  static std::unique_ptr<HidPreparsedData> Create(HANDLE device_handle);
  static std::unique_ptr<HidPreparsedData> CreateForTesting(
      PHIDP_PREPARSED_DATA preparsed_data,
      HIDP_CAPS capabilities);

  HidPreparsedData(PHIDP_PREPARSED_DATA preparsed_data,
                   HIDP_CAPS capabilities,
                   bool owns_preparsed_data);
  HidPreparsedData(const HidPreparsedData&) = delete;
  HidPreparsedData& operator=(const HidPreparsedData&) = delete;
  ~HidPreparsedData() override;

  // HidServiceWin::PreparsedData implementation.
  const HIDP_CAPS& GetCaps() const override;
  uint32_t GetCollectionType() const override;
  std::vector<ReportItem> GetReportItems(
      HIDP_REPORT_TYPE report_type) const override;

 private:
  const PHIDP_PREPARSED_DATA preparsed_data_;
  const HIDP_CAPS capabilities_;
  const bool owns_preparsed_data_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_PREPARSED_DATA_H_
