// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_TRIGGER_DATA_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_TRIGGER_DATA_H_

#include <string>

#include "base/containers/flat_set.h"
#include "components/attribution_reporting/filters.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class CONTENT_EXPORT AttributionAggregatableTriggerData {
 public:
  static absl::optional<AttributionAggregatableTriggerData> Create(
      absl::uint128 key_piece,
      base::flat_set<std::string> source_keys,
      attribution_reporting::Filters filters,
      attribution_reporting::Filters not_filters);

  static AttributionAggregatableTriggerData CreateForTesting(
      absl::uint128 key_piece,
      base::flat_set<std::string> source_keys,
      attribution_reporting::Filters filters,
      attribution_reporting::Filters not_filters);

  ~AttributionAggregatableTriggerData();

  AttributionAggregatableTriggerData(const AttributionAggregatableTriggerData&);
  AttributionAggregatableTriggerData(AttributionAggregatableTriggerData&&);

  AttributionAggregatableTriggerData& operator=(
      const AttributionAggregatableTriggerData&);
  AttributionAggregatableTriggerData& operator=(
      AttributionAggregatableTriggerData&&);

  absl::uint128 key_piece() const { return key_piece_; }

  const base::flat_set<std::string>& source_keys() const {
    return source_keys_;
  }

  const attribution_reporting::Filters& filters() const { return filters_; }

  const attribution_reporting::Filters& not_filters() const {
    return not_filters_;
  }

 private:
  AttributionAggregatableTriggerData(
      absl::uint128 key_piece,
      base::flat_set<std::string> source_keys,
      attribution_reporting::Filters filters,
      attribution_reporting::Filters not_filters);

  absl::uint128 key_piece_;
  base::flat_set<std::string> source_keys_;
  attribution_reporting::Filters filters_;
  attribution_reporting::Filters not_filters_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_TRIGGER_DATA_H_
