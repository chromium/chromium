// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_report_item.h"

#include "services/device/public/mojom/hid.mojom.h"

namespace device {

namespace {

mojom::HidUsageAndPagePtr ConvertUsageToMojo(uint32_t usage) {
  uint16_t usage_id = usage & 0xffff;
  uint16_t usage_page = (usage >> 16) & 0xffff;
  return mojom::HidUsageAndPage::New(usage_id, usage_page);
}

}  // namespace

HidReportItem::HidReportItem(HidReportDescriptorItem::Tag tag,
                             uint32_t short_data,
                             const HidItemStateTable& state)
    : tag_(tag),
      report_info_(
          *reinterpret_cast<HidReportDescriptorItem::ReportInfo*>(&short_data)),
      report_id_(state.report_id),
      local_(state.local),
      global_(state.global_stack.empty()
                  ? HidItemStateTable::HidGlobalItemState()
                  : state.global_stack.back()),
      is_range_(state.local.usage_minimum != state.local.usage_maximum) {
  if (state.local.string_index) {
    local_.string_minimum = state.local.string_index;
    local_.string_maximum = state.local.string_index;
  }
  if (state.local.designator_index) {
    local_.designator_minimum = state.local.designator_index;
    local_.designator_maximum = state.local.designator_index;
  }
}

HidReportItem::~HidReportItem() = default;

mojom::HidReportItemPtr HidReportItem::ToMojo() const {
  auto report_item = mojom::HidReportItem::New();
  report_item->is_range = is_range_;

  // Data associated with the Main item.
  report_item->is_constant = report_info_.data_or_constant;
  report_item->is_variable = report_info_.array_or_variable;
  report_item->is_relative = report_info_.absolute_or_relative;
  report_item->wrap = report_info_.wrap;
  report_item->is_non_linear = report_info_.linear;
  report_item->no_preferred_state = report_info_.preferred;
  report_item->has_null_position = report_info_.null;
  report_item->is_volatile = report_info_.is_volatile;
  report_item->is_buffered_bytes = report_info_.bit_field_or_buffer;

  // Local items.
  for (const auto& item : local_.usages)
    report_item->usages.push_back(ConvertUsageToMojo(item));
  report_item->usage_minimum = ConvertUsageToMojo(local_.usage_minimum);
  report_item->usage_maximum = ConvertUsageToMojo(local_.usage_maximum);
  report_item->designator_minimum = local_.designator_minimum;
  report_item->designator_maximum = local_.designator_maximum;
  report_item->string_minimum = local_.string_minimum;
  report_item->string_maximum = local_.string_maximum;

  // Global items.
  report_item->logical_minimum = global_.logical_minimum;
  report_item->logical_maximum = global_.logical_maximum;
  report_item->physical_minimum = global_.physical_minimum;
  report_item->physical_maximum = global_.physical_maximum;
  report_item->unit_exponent = global_.unit_exponent;
  report_item->unit = global_.unit;
  report_item->report_size = global_.report_size;
  report_item->report_count = global_.report_count;

  return report_item;
}

}  // namespace device
