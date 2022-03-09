// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_

#include <string>

#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/common/content_export.h"

namespace base {
class Time;
class Value;
}  // namespace base

namespace content {

class AttributionFilterData;
class CommonSourceInfo;

// Calculates the report time for a conversion associated with a given
// source.
base::Time ComputeReportTime(const CommonSourceInfo& source,
                             base::Time trigger_time);

// Returns the number of report windows for the given source type.
int NumReportWindows(AttributionSourceType source_type);

// Calculates the report time for a given source and window index.
base::Time ReportTimeAtWindow(const CommonSourceInfo& source, int window_index);

std::string SerializeAttributionJson(const base::Value& body,
                                     bool pretty_print = false);

// Checks whether filters keys within `source` and `trigger` match.
// `negated` indicates that no filter data keys should have a match
// between source and trigger. Negating the result of this function
// should not be used to apply "not_filters" within this API.
CONTENT_EXPORT bool AttributionFilterDataMatch(
    const AttributionFilterData& source,
    const AttributionFilterData& trigger,
    bool negated = false);

CONTENT_EXPORT bool AttributionFiltersMatch(
    const AttributionFilterData& source_filter_data,
    const AttributionFilterData& trigger_filters,
    const AttributionFilterData& trigger_not_filters);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
