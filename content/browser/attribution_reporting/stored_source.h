// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORED_SOURCE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORED_SOURCE_H_

#include <stdint.h>

#include <vector>

#include "base/types/strong_alias.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/common/content_export.h"

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

  StoredSource(CommonSourceInfo common_info,
               AttributionLogic attribution_logic,
               ActiveState active_state,
               Id source_id,
               int64_t aggregatable_budget_consumed);

  ~StoredSource();

  StoredSource(const StoredSource&);
  StoredSource(StoredSource&&);

  StoredSource& operator=(const StoredSource&);
  StoredSource& operator=(StoredSource&&);

  const CommonSourceInfo& common_info() const { return common_info_; }

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
