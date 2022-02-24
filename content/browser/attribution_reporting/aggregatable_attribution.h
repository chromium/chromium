// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_H_

#include <stdint.h>

#include <vector>

#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace content {

class CONTENT_EXPORT AggregatableHistogramContribution {
 public:
  AggregatableHistogramContribution(absl::uint128 key, uint32_t value);
  AggregatableHistogramContribution(
      const AggregatableHistogramContribution& other) = default;
  AggregatableHistogramContribution& operator=(
      const AggregatableHistogramContribution& other) = default;
  AggregatableHistogramContribution(AggregatableHistogramContribution&& other) =
      default;
  AggregatableHistogramContribution& operator=(
      AggregatableHistogramContribution&& other) = default;
  ~AggregatableHistogramContribution() = default;

  absl::uint128 key() const { return key_; }

  uint32_t value() const { return value_; }

 private:
  absl::uint128 key_;
  uint32_t value_;
};

// Struct which represents all attributes of an aggregatable attribution.
struct CONTENT_EXPORT AggregatableAttribution {
 public:
  using Id = base::StrongAlias<AggregatableAttribution, int64_t>;

  AggregatableAttribution(
      AttributionInfo attribution_info,
      base::Time report_time,
      std::vector<AggregatableHistogramContribution> contributions);
  AggregatableAttribution(const AggregatableAttribution& other);
  AggregatableAttribution& operator=(const AggregatableAttribution& other);
  AggregatableAttribution(AggregatableAttribution&& other);
  AggregatableAttribution& operator=(AggregatableAttribution&& other);
  ~AggregatableAttribution();

  // Returns the sum of the contributions (values) across all buckets.
  base::CheckedNumeric<int64_t> BudgetRequired() const;

  AttributionInfo attribution_info;
  // Might be null if not set yet.
  base::Time report_time;
  std::vector<AggregatableHistogramContribution> contributions;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_H_
