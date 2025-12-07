// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/group_by_origin_key.h"

#include <cstdint>
#include <map>
#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

GroupByOriginKeyMapper::GroupByOriginKeyMapper() = default;
GroupByOriginKeyMapper::~GroupByOriginKeyMapper() = default;

size_t GroupByOriginKeyMapper::LookupGroupByOriginId(
    const SingleStorageInterestGroup& ig,
    const blink::InterestGroup::ExecutionMode execution_mode) {
  if (execution_mode !=
      blink::InterestGroup::ExecutionMode::kGroupedByOriginMode) {
    return 0;
  }
  Key key;
  key.joining_origin = ig->joining_origin;
  key.bidding_origin = ig->interest_group.owner;
  if (base::FeatureList::IsEnabled(blink::features::kFledgeClickiness)) {
    const auto& maybe_view_and_click_counts_providers =
        ig->interest_group.view_and_click_counts_providers;
    if (!maybe_view_and_click_counts_providers.has_value() ||
        maybe_view_and_click_counts_providers->empty()) {
      key.view_and_click_counts_providers.insert(ig->interest_group.owner);
    } else {
      key.view_and_click_counts_providers =
          *maybe_view_and_click_counts_providers;
    }
  }
  auto& id = key_ids_[std::move(key)];
  if (id == 0) {
    id = key_ids_.size();
  }
  return id;
}

GroupByOriginKeyMapper::Key::Key() = default;
GroupByOriginKeyMapper::Key::Key(Key&&) = default;
GroupByOriginKeyMapper::Key::~Key() = default;
GroupByOriginKeyMapper::Key& GroupByOriginKeyMapper::Key::operator=(Key&&) =
    default;

}  // namespace content
