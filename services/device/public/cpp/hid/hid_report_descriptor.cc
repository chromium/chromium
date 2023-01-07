// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_report_descriptor.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/stl_util.h"

namespace device {

namespace {

const int kBitsPerByte = 8;

}  // namespace

HidReportDescriptor::HidReportDescriptor(base::span<const uint8_t> bytes) {
  size_t header_index = 0;
  HidReportDescriptorItem* item = nullptr;
  while (header_index < bytes.size()) {
    items_.push_back(
        HidReportDescriptorItem::Create(bytes.subspan(header_index), item));
    header_index += items_.back()->GetSize();
  }
  collections_ = HidCollection::BuildCollections(items_);
}

HidReportDescriptor::~HidReportDescriptor() {}

void HidReportDescriptor::GetDetails(
    std::vector<mojom::HidCollectionInfoPtr>* top_level_collections,
    bool* has_report_id,
    size_t* max_input_report_bytes,
    size_t* max_output_report_bytes,
    size_t* max_feature_report_bytes) const {
  DCHECK(top_level_collections);
  DCHECK(has_report_id);
  DCHECK(max_input_report_bytes);
  DCHECK(max_output_report_bytes);
  DCHECK(max_feature_report_bytes);
  base::STLClearObject(top_level_collections);

  size_t max_input_report_bits = 0;
  size_t max_output_report_bits = 0;
  size_t max_feature_report_bits = 0;
  *has_report_id = false;
  for (const auto& collection : collections_) {
    size_t input_bits;
    size_t output_bits;
    size_t feature_bits;
    collection->GetMaxReportSizes(&input_bits, &output_bits, &feature_bits);
    top_level_collections->push_back(collection->ToMojo());
    if (collection->HasReportId())
      *has_report_id = true;
    max_input_report_bits = std::max(max_input_report_bits, input_bits);
    max_output_report_bits = std::max(max_output_report_bits, output_bits);
    max_feature_report_bits = std::max(max_feature_report_bits, feature_bits);
  }

  // Convert bits into bytes.
  *max_input_report_bytes = max_input_report_bits / kBitsPerByte;
  *max_output_report_bytes = max_output_report_bits / kBitsPerByte;
  *max_feature_report_bytes = max_feature_report_bits / kBitsPerByte;
}

}  // namespace device
