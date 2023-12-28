// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_collection.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "services/device/public/cpp/hid/hid_item_state_table.h"

namespace device {

namespace {
// The maximum value of the report size for a single item in a HID report is 32
// bits. From the Device Class Definition for HID v1.11, sec. 8.2: "An item
// field cannot span more than 4 bytes in a report. For example, a 32-bit item
// must start on a byte boundary to satisfy this condition."
static constexpr uint32_t kMaxItemReportSizeBits = 32;

// On Windows, HID report length is reported (in bytes) as a USHORT which
// imposes a practical limit of 2^16-1 bytes. Apply the same upper limit when
// computing the maximum report size.
static constexpr uint64_t kMaxReasonableReportLengthBits =
    std::numeric_limits<uint16_t>::max() * 8;

// Limit the maximum depth of the parser when inspecting nested collections.
static constexpr int kMaxReasonableCollectionDepth = 50;
}  // namespace

HidCollection::HidCollection(HidCollection* parent,
                             uint32_t usage_page,
                             uint32_t usage,
                             uint32_t type)
    : parent_(parent), usage_(usage, usage_page), collection_type_(type) {}

HidCollection::~HidCollection() = default;

// static
std::vector<std::unique_ptr<HidCollection>> HidCollection::BuildCollections(
    const std::vector<std::unique_ptr<HidReportDescriptorItem>>& items) {
  std::vector<std::unique_ptr<HidCollection>> collections;
  // This HID report descriptor parser implements a state machine described
  // in the HID specification. See section 6.2.2 Report Descriptor.
  HidItemStateTable state;
  int depth = 0;
  for (const auto& current_item : items) {
    switch (current_item->tag()) {
      case HidReportDescriptorItem::kTagCollection:
        // Add a new collection. Collections at the top-most level describe
        // separate components of the device and are often treated as separate
        // devices. Nested components represent logical collections of fields
        // within a report.
        ++depth;
        if (depth <= kMaxReasonableCollectionDepth)
          AddCollection(*current_item, collections, state);
        state.local.Reset();
        break;
      case HidReportDescriptorItem::kTagEndCollection:
        if (depth <= kMaxReasonableCollectionDepth) {
          // Mark the end of the current collection. Subsequent items describe
          // reports associated with the parent collection.
          if (state.collection)
            state.collection = state.collection->parent_;
        }
        state.local.Reset();
        if (depth > 0)
          --depth;
        break;
      case HidReportDescriptorItem::kTagInput:
      case HidReportDescriptorItem::kTagOutput:
      case HidReportDescriptorItem::kTagFeature:
        // Add a report item to an input, output, or feature report within the
        // current collection. The properties of the report item are determined
        // by the current descriptor item and the current item state table.
        // Changes to input, output, and feature reports are propagated to all
        // ancestor collections.
        if (state.collection) {
          auto* collection = state.collection.get();
          while (collection) {
            collection->AddReportItem(current_item->tag(),
                                      current_item->GetShortData(), state);
            collection = collection->parent_;
          }
        }
        state.local.Reset();
        break;
      case HidReportDescriptorItem::kTagPush:
        // Push a copy of the current global state onto the stack. If there is
        // no global state, the push has no effect and is ignored.
        if (!state.global_stack.empty())
          state.global_stack.push_back(state.global_stack.back());
        break;
      case HidReportDescriptorItem::kTagPop:
        // Pop the top item of the global state stack, returning to the
        // previously pushed state. If there is no such item, the pop has no
        // effect and is ignored.
        if (!state.global_stack.empty())
          state.global_stack.pop_back();
        break;
      case HidReportDescriptorItem::kTagReportId:
        // Update the current report ID. The report ID is global, but is not
        // affected by push and pop. Changes to the report ID are propagated to
        // all ancestor collections.
        if (state.collection) {
          state.report_id = current_item->GetShortData();
          auto* collection = state.collection.get();
          while (collection) {
            collection->report_ids_.push_back(state.report_id);
            collection = collection->parent_;
          }
        }
        break;
      case HidReportDescriptorItem::kTagUsagePage:
      case HidReportDescriptorItem::kTagLogicalMinimum:
      case HidReportDescriptorItem::kTagLogicalMaximum:
      case HidReportDescriptorItem::kTagPhysicalMinimum:
      case HidReportDescriptorItem::kTagPhysicalMaximum:
      case HidReportDescriptorItem::kTagUnitExponent:
      case HidReportDescriptorItem::kTagUnit:
      case HidReportDescriptorItem::kTagReportSize:
      case HidReportDescriptorItem::kTagReportCount:
      case HidReportDescriptorItem::kTagUsage:
      case HidReportDescriptorItem::kTagUsageMinimum:
      case HidReportDescriptorItem::kTagUsageMaximum:
      case HidReportDescriptorItem::kTagDesignatorIndex:
      case HidReportDescriptorItem::kTagDesignatorMinimum:
      case HidReportDescriptorItem::kTagDesignatorMaximum:
      case HidReportDescriptorItem::kTagStringIndex:
      case HidReportDescriptorItem::kTagStringMinimum:
      case HidReportDescriptorItem::kTagStringMaximum:
      case HidReportDescriptorItem::kTagDelimiter:
        // Update the value associated with a local or global item in the item
        // state table.
        state.SetItemValue(current_item->tag(), current_item->GetShortData(),
                           current_item->payload_size());
        break;
      default:
        break;
    }
  }
  return collections;
}

// static
void HidCollection::AddCollection(
    const HidReportDescriptorItem& item,
    std::vector<std::unique_ptr<HidCollection>>& collections,
    HidItemStateTable& state) {
  // Extract |usage| and |usage_page| from the current state. The usage page may
  // be set either by a global usage page, or in the high-order bytes of a local
  // usage value. When both are provided, the local usage value takes
  // precedence.
  uint32_t usage = state.local.usages.empty() ? 0 : state.local.usages.front();
  uint32_t usage_page = (usage >> 16) & 0xffff;
  if (usage_page == 0 && !state.global_stack.empty())
    usage_page = state.global_stack.back().usage_page;
  // Create the new collection. If it is a child of another collection, append
  // it to that collection's list of children. Otherwise, append it to the list
  // of top-level collections in |collections|.
  uint32_t collection_type = item.GetShortData();
  auto collection = std::make_unique<HidCollection>(
      state.collection, usage_page, usage, collection_type);
  if (state.collection) {
    state.collection->children_.push_back(std::move(collection));
    state.collection = state.collection->children_.back().get();
  } else {
    collections.push_back(std::move(collection));
    state.collection = collections.back().get();
  }
}

void HidCollection::AddChildForTesting(
    std::unique_ptr<HidCollection> collection) {
  children_.push_back(std::move(collection));
}

void HidCollection::AddReportItem(HidReportDescriptorItem::Tag tag,
                                  uint32_t report_info,
                                  const HidItemStateTable& state) {
  // Get the correct report map for the current report item (input, output,
  // or feature). The new item will be appended to a report in this report map.
  std::unordered_map<uint8_t, HidReport>* reports = nullptr;
  if (tag == HidReportDescriptorItem::kTagInput)
    reports = &input_reports_;
  else if (tag == HidReportDescriptorItem::kTagOutput)
    reports = &output_reports_;
  else if (tag == HidReportDescriptorItem::kTagFeature)
    reports = &feature_reports_;
  else
    return;
  // Fetch the report with the |report_id| matching this item, or insert a new
  // report into the map if it does not yet exist.
  HidReport* report = nullptr;
  auto find_it = reports->find(state.report_id);
  if (find_it == reports->end()) {
    auto emplace_result = reports->emplace(state.report_id, HidReport());
    report = &emplace_result.first->second;
  } else {
    report = &find_it->second;
  }
  // Create the report item and append it to the report.
  report->push_back(HidReportItem::Create(tag, report_info, state));
}

void HidCollection::GetMaxReportSizes(size_t* max_input_report_bits,
                                      size_t* max_output_report_bits,
                                      size_t* max_feature_report_bits) const {
  DCHECK(max_input_report_bits);
  DCHECK(max_output_report_bits);
  DCHECK(max_feature_report_bits);
  struct {
    const raw_ref<const std::unordered_map<uint8_t, HidReport>> reports;
    const raw_ref<size_t> max_report_bits;
  } report_lists[]{
      {ToRawRef(input_reports_), ToRawRef(*max_input_report_bits)},
      {ToRawRef(output_reports_), ToRawRef(*max_output_report_bits)},
      {ToRawRef(feature_reports_), ToRawRef(*max_feature_report_bits)},
  };
  auto collection_info = mojom::HidCollectionInfo::New();
  collection_info->usage =
      mojom::HidUsageAndPage::New(usage_.usage, usage_.usage_page);
  collection_info->report_ids.insert(collection_info->report_ids.end(),
                                     report_ids_.begin(), report_ids_.end());
  for (const auto& entry : report_lists) {
    *entry.max_report_bits = 0;
    for (const auto& report : *entry.reports) {
      uint64_t report_bits = 0;
      for (const auto& item : report.second) {
        uint64_t report_size = item->GetReportSize();
        // Report size can be at most 32 bits, but some devices specify larger
        // sizes. Warn if the size is too large, but still allow the report to
        // affect the maximum report size.
        if (report_size > kMaxItemReportSizeBits) {
          LOG(WARNING) << base::StringPrintf(
              "encountered report item with invalid report size (%" PRIu64
              ">%u)",
              report_size, kMaxItemReportSizeBits);
        }
        // Report size and report count are both 32-bit values. A 64-bit integer
        // type is needed to avoid overflow when computing the product.
        uint64_t report_count = item->GetReportCount();
        uint64_t item_bits = report_size * report_count;
        // Ignore this report if adding the size of this item would extend the
        // total report length beyond the reasonable maximum.
        if (item_bits > kMaxReasonableReportLengthBits ||
            report_bits > kMaxReasonableReportLengthBits - item_bits) {
          report_bits = 0;
          break;
        }
        report_bits += item_bits;
      }
      DCHECK_LE(report_bits, kMaxReasonableReportLengthBits);
      *entry.max_report_bits =
          std::max(*entry.max_report_bits, static_cast<size_t>(report_bits));
    }
  }
}

mojom::HidCollectionInfoPtr HidCollection::ToMojo() const {
  auto collection = mojom::HidCollectionInfo::New();
  struct {
    const raw_ref<const std::unordered_map<uint8_t, HidReport>> in;
    const raw_ref<std::vector<mojom::HidReportDescriptionPtr>> out;
  } report_lists[]{
      {ToRawRef(input_reports_), ToRawRef(collection->input_reports)},
      {ToRawRef(output_reports_), ToRawRef(collection->output_reports)},
      {ToRawRef(feature_reports_), ToRawRef(collection->feature_reports)},
  };

  collection->usage =
      mojom::HidUsageAndPage::New(usage_.usage, usage_.usage_page);
  collection->report_ids.insert(collection->report_ids.end(),
                                report_ids_.begin(), report_ids_.end());
  collection->collection_type = collection_type_;

  for (const auto& report_list : report_lists) {
    for (const auto& report : *report_list.in) {
      auto report_description = mojom::HidReportDescription::New();
      report_description->report_id = report.first;
      for (const auto& item : report.second)
        report_description->items.push_back(item->ToMojo());
      report_list.out->push_back(std::move(report_description));
    }
  }

  for (const auto& child : children_)
    collection->children.push_back(child->ToMojo());

  return collection;
}

}  // namespace device
