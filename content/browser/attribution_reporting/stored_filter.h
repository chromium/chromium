// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORED_FILTER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORED_FILTER_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/common/content_export.h"

namespace content {

// Contains attributes specific to a stored source.
class CONTENT_EXPORT StoredFilter {
 public:
  using Id = base::StrongAlias<StoredFilter, int64_t>;

  static std::optional<StoredFilter> Create(
      uint64_t epoch,
      url::Origin origin,
      double initial_budget,
      double consumed_budget);

  ~StoredFilter();

  StoredFilter(const StoredFilter&);
  StoredFilter(StoredFilter&&);

  StoredFilter& operator=(const StoredFilter&);
  StoredFilter& operator=(StoredFilter&&);

  uint64_t epoch() const { return epoch_; }
  const url::Origin& origin() const { return origin_; }
  double initial_budget() const { return initial_budget_; }
  double consumed_budget() const { return consumed_budget_; }
  
 private:
  StoredFilter(uint64_t epoch,
               url::Origin origin,
               double initial_budget,
               double consumed_budget);

  uint64_t epoch_;
  url::Origin origin_;
  double initial_budget_;
  double consumed_budget_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORED_SOURCE_H_
