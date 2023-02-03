// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
#define CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

struct AttributionTriggerAndTime {
  AttributionTrigger trigger;
  base::Time time;
  bool debug_permission = false;
};

struct AttributionSource {
  StorableSource source;
  bool debug_permission = false;
};

using AttributionSimulationEvent =
    absl::variant<AttributionSource, AttributionTriggerAndTime>;

using AttributionSimulationEvents = std::vector<AttributionSimulationEvent>;

base::expected<AttributionSimulationEvents, std::string>
ParseAttributionSimulationInput(base::Value::Dict input,
                                base::Time offset_time);

}  // namespace content

#endif  // CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
