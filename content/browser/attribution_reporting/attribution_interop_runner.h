// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_RUNNER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_RUNNER_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"

namespace content {

struct AttributionConfig;

constexpr char kEventLevelResultsKey[] = "event_level_results";
constexpr char kDebugEventLevelResultsKey[] = "debug_event_level_results";
constexpr char kAggregatableResultsKey[] = "aggregatable_results";
constexpr char kDebugAggregatableResultsKey[] = "debug_aggregatable_results";
constexpr char kVerboseDebugReportsKey[] = "verbose_debug_reports";
constexpr char kUnparsableRegistrationsKey[] = "unparsable_registrations";

// Simulates the Attribution Reporting API for a single user on sources and
// triggers specified in `input`. Returns the generated reports.
base::expected<base::Value::Dict, std::string> RunAttributionInteropSimulation(
    base::Value::Dict input,
    const AttributionConfig&);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_RUNNER_H_
