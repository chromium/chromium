// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_HISTOGRAM_CONTRIBUTION_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_HISTOGRAM_CONTRIBUTION_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace content {

class CONTENT_EXPORT AggregatableHistogramContribution {
 public:
  AggregatableHistogramContribution(absl::uint128 key, uint32_t value);
  AggregatableHistogramContribution(const AggregatableHistogramContribution&) =
      default;
  AggregatableHistogramContribution& operator=(
      const AggregatableHistogramContribution&) = default;
  AggregatableHistogramContribution(AggregatableHistogramContribution&&) =
      default;
  AggregatableHistogramContribution& operator=(
      AggregatableHistogramContribution&&) = default;
  ~AggregatableHistogramContribution() = default;

  absl::uint128 key() const { return key_; }

  uint32_t value() const { return value_; }

 private:
  absl::uint128 key_;
  uint32_t value_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_HISTOGRAM_CONTRIBUTION_H_
