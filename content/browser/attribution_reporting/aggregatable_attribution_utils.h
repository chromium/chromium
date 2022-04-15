// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"

namespace absl {
class uint128;
}  // namespace absl

namespace content {

class AggregatableHistogramContribution;
class AttributionAggregatableSource;
class AttributionAggregatableTrigger;
class AttributionFilterData;

// Creates histograms from the specified source and trigger data.
CONTENT_EXPORT std::vector<AggregatableHistogramContribution>
CreateAggregatableHistogram(const AttributionFilterData& source_filter_data,
                            const AttributionAggregatableSource& source,
                            const AttributionAggregatableTrigger& trigger);

// Returns a hex string representation of the 128-bit aggregatable key in big
// endian order.
CONTENT_EXPORT std::string HexEncodeAggregatableKey(absl::uint128 value);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
