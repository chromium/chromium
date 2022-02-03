// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_

#include <stdint.h>

#include <string>

#include "content/browser/attribution_reporting/common_source_info.h"

namespace base {
class Time;
class Value;
}  // namespace base

namespace content {

// Calculates the report time for a conversion associated with a given
// source.
base::Time ComputeReportTime(const CommonSourceInfo& source,
                             base::Time trigger_time);

// Returns the number of report windows for the given source type.
int NumReportWindows(CommonSourceInfo::SourceType source_type);

// Calculates the report time for a given source and window index.
base::Time ReportTimeAtWindow(const CommonSourceInfo& source, int window_index);

uint64_t TriggerDataCardinality(CommonSourceInfo::SourceType source_type);

double RandomizedTriggerRate(CommonSourceInfo::SourceType source_type);

std::string SerializeAttributionJson(const base::Value& body,
                                     bool pretty_print = false);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
