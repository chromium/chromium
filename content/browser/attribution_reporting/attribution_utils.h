// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_

#include <string>

#include "base/containers/span.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Time;
class TimeDelta;
class ValueView;
}  // namespace base

namespace content {

class CommonSourceInfo;

// Calculates the report time for a conversion associated with a given
// source.
base::Time ComputeReportTime(base::Time source_time,
                             base::Time event_report_window_time,
                             base::Time trigger_time,
                             base::span<const base::TimeDelta> early_deadlines);

base::Time ComputeReportTime(const CommonSourceInfo& source,
                             base::Time event_report_window_time,
                             base::Time trigger_time);

// Returns the number of report windows for the given source type.
int NumReportWindows(attribution_reporting::mojom::SourceType);

// Calculates the report time for a given source and window index.
base::Time ReportTimeAtWindow(const CommonSourceInfo& source,
                              base::Time event_report_window_time,
                              int window_index);

// Calculates the last trigger time that could have produced `report_time`.
CONTENT_EXPORT base::Time LastTriggerTimeForReportTime(base::Time report_time);

CONTENT_EXPORT std::string SerializeAttributionJson(base::ValueView body,
                                                    bool pretty_print = false);

CONTENT_EXPORT base::Time ComputeReportWindowTime(
    absl::optional<base::Time> report_window_time,
    base::Time expiry_time);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
