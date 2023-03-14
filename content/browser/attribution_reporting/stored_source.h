// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORED_SOURCE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORED_SOURCE_H_

#include <stdint.h>

#include <vector>

#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/filters.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Contains attributes specific to a stored source.
class CONTENT_EXPORT StoredSource {
 public:
  using Id = base::StrongAlias<StoredSource, int64_t>;

  // Note that aggregatable reports are not subject to the attribution logic.
  enum class AttributionLogic {
    // Never send a report for this source even if it gets attributed.
    kNever = 0,
    // Attribute the source truthfully.
    kTruthfully = 1,
    // The browser generates fake reports for the source.
    kFalsely = 2,
    kMaxValue = kFalsely,
  };

  enum class ActiveState {
    kActive = 0,
    kInactive = 1,
    kReachedEventLevelAttributionLimit = 2,
    kMaxValue = kReachedEventLevelAttributionLimit,
  };

  static bool IsExpiryOrReportWindowTimeValid(
      base::Time expiry_or_report_window_time,
      base::Time source_time);

  StoredSource(CommonSourceInfo common_info,
               uint64_t source_event_id,
               attribution_reporting::DestinationSet,
               base::Time expiry_time,
               base::Time event_report_window_time,
               base::Time aggregatable_report_window_time,
               int64_t priority,
               attribution_reporting::FilterData,
               absl::optional<uint64_t> debug_key,
               attribution_reporting::AggregationKeys,
               AttributionLogic,
               ActiveState,
               Id source_id,
               int64_t aggregatable_budget_consumed);

  ~StoredSource();

  StoredSource(const StoredSource&);
  StoredSource(StoredSource&&);

  StoredSource& operator=(const StoredSource&);
  StoredSource& operator=(StoredSource&&);

  const CommonSourceInfo& common_info() const { return common_info_; }

  uint64_t source_event_id() const { return source_event_id_; }

  const attribution_reporting::DestinationSet& destination_sites() const {
    return destination_sites_;
  }

  base::Time expiry_time() const { return expiry_time_; }

  base::Time event_report_window_time() const {
    return event_report_window_time_;
  }

  base::Time aggregatable_report_window_time() const {
    return aggregatable_report_window_time_;
  }

  int64_t priority() const { return priority_; }

  const attribution_reporting::FilterData& filter_data() const {
    return filter_data_;
  }

  absl::optional<uint64_t> debug_key() const { return debug_key_; }

  const attribution_reporting::AggregationKeys& aggregation_keys() const {
    return aggregation_keys_;
  }

  AttributionLogic attribution_logic() const { return attribution_logic_; }

  ActiveState active_state() const { return active_state_; }

  Id source_id() const { return source_id_; }

  int64_t aggregatable_budget_consumed() const {
    return aggregatable_budget_consumed_;
  }

  const std::vector<uint64_t>& dedup_keys() const { return dedup_keys_; }

  const std::vector<uint64_t>& aggregatable_dedup_keys() const {
    return aggregatable_dedup_keys_;
  }

  void SetDedupKeys(std::vector<uint64_t> dedup_keys) {
    dedup_keys_ = std::move(dedup_keys);
  }

  void SetAggregatableDedupKeys(std::vector<uint64_t> aggregatable_dedup_keys) {
    aggregatable_dedup_keys_ = std::move(aggregatable_dedup_keys);
  }

 private:
  CommonSourceInfo common_info_;

  uint64_t source_event_id_;
  attribution_reporting::DestinationSet destination_sites_;
  base::Time expiry_time_;
  base::Time event_report_window_time_;
  base::Time aggregatable_report_window_time_;
  int64_t priority_;
  attribution_reporting::FilterData filter_data_;
  absl::optional<uint64_t> debug_key_;
  attribution_reporting::AggregationKeys aggregation_keys_;

  AttributionLogic attribution_logic_;

  ActiveState active_state_;

  Id source_id_;

  int64_t aggregatable_budget_consumed_;

  // Dedup keys associated with the source. Only set in values returned from
  // `AttributionStorage::GetActiveSources()`.
  std::vector<uint64_t> dedup_keys_;

  std::vector<uint64_t> aggregatable_dedup_keys_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORED_SOURCE_H_
