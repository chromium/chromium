// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_HID_HID_ITEM_STATE_TABLE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_HID_HID_ITEM_STATE_TABLE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "services/device/public/cpp/hid/hid_report_descriptor_item.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

class HidCollection;

// The item state table, used when parsing the HID report descriptor.
class HidItemStateTable {
 public:
  class HidGlobalItemState {
   public:
    HidGlobalItemState();
    HidGlobalItemState(const HidGlobalItemState&);
    ~HidGlobalItemState();

    // Global items. See section 6.2.2.7 of the HID specifications.
    uint32_t usage_page = mojom::kPageUndefined;
    int32_t logical_minimum = 0;
    int32_t logical_maximum = 0;
    int32_t physical_minimum = 0;
    int32_t physical_maximum = 0;
    uint32_t unit_exponent = 0;
    uint32_t unit = 0;
    uint32_t report_size = 0;
    uint32_t report_count = 0;
  };

  class HidLocalItemState {
   public:
    HidLocalItemState();
    HidLocalItemState(const HidLocalItemState&);
    ~HidLocalItemState();

    void Reset();

    // Local items. See section 6.2.2.8 of the HID specifications.
    std::vector<uint32_t> usages;
    uint32_t usage_minimum = 0;
    uint32_t usage_maximum = 0;
    uint32_t designator_index = 0;
    uint32_t designator_minimum = 0;
    uint32_t designator_maximum = 0;
    uint32_t string_index = 0;
    uint32_t string_minimum = 0;
    uint32_t string_maximum = 0;
    uint32_t delimiter = 0;
  };

  HidItemStateTable();
  ~HidItemStateTable();

  // Set the value of a local or global item.
  void SetItemValue(HidReportDescriptorItem::Tag tag, uint32_t value);

  // The collection that will be modified when main items are encountered.
  HidCollection* collection = nullptr;

  // The report ID is part of the global item state but is not affected by push
  // or pop items.
  uint32_t report_id = 0;

  // The global item state. The global state is represented as a stack in order
  // to handle push and pop items. The last element holds the current state.
  std::vector<HidGlobalItemState> global_stack;

  // The local item state.
  HidLocalItemState local;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_HID_HID_ITEM_STATE_TABLE_H_
