// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"

#include "base/check_op.h"

namespace content {

AggregatableHistogramContribution::AggregatableHistogramContribution(
    absl::uint128 key,
    uint32_t value)
    : key_(key), value_(value) {
  DCHECK_GT(value, 0u);
}

}  // namespace content
