// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
#define CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_

#include <iosfwd>

#include "content/public/browser/attribution_config.h"
#include "content/public/browser/attribution_reporting.h"

namespace base {
class Value;
}  // namespace base

namespace content {

struct AttributionSimulationOutputOptions {
  // If true, removes the `report_id` field from reports before output.
  //
  // This field normally contains a random GUID used by the reporting origin
  // to deduplicate reports in the event of retries. As such, it is a source
  // of nondeterminism in the output.
  bool remove_report_ids = false;

  // If true, removes the `shared_info`, `aggregation_service_payloads` and
  // `source_registration_time` fields from aggregatable reports before output.
  //
  // These fields normally encode a random GUID or the absolute time and
  // therefore are sources of nondeterminism in the output.
  bool remove_assembled_report = false;
};

struct AttributionSimulationOptions {
  AttributionNoiseMode noise_mode = AttributionNoiseMode::kDefault;

  AttributionConfig config;

  AttributionDelayMode delay_mode = AttributionDelayMode::kDefault;

  AttributionSimulationOutputOptions output_options;
};

// Simulates the Attribution Reporting API for a single user on sources and
// triggers specified in `input`. Returns the generated reports, if any, as a
// JSON document. On error, writes to `error_stream` and returns
// `base::ValueType::NONE`.
//
// Exits if `input` cannot be parsed.
base::Value RunAttributionSimulation(
    base::Value input,
    const AttributionSimulationOptions& options,
    std::ostream& error_stream);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
