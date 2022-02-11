// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
#define CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_

#include "content/public/browser/attribution_reporting.h"

namespace base {
class Value;
}  // namespace base

namespace content {

struct AttributionSimulationOptions {
  AttributionNoiseMode noise_mode = AttributionNoiseMode::kDefault;

  AttributionDelayMode delay_mode = AttributionDelayMode::kDefault;

  // If true, removes the `report_id` field from reports before output.
  //
  // This field normally contains a random GUID used by the reporting origin
  // to deduplicate reports in the event of retries. As such, it is a source
  // of nondeterminism in the output.
  bool remove_report_ids = false;
};

// Simulates the Attribution Reporting API for a single user on sources and
// triggers specified in `input`. Returns the generated reports, if any, as a
// JSON document.
//
// Exits if `input` cannot be parsed.
base::Value RunAttributionSimulationOrExit(
    base::Value input,
    const AttributionSimulationOptions& options);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
