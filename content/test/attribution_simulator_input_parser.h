// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
#define CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_

#include <iosfwd>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

struct AttributionTriggerAndTime {
  AttributionTrigger trigger;
  base::Time time;
};

using AttributionSimulationEvent =
    absl::variant<StorableSource, AttributionTriggerAndTime>;

// The value is the raw JSON associated with the event.
using AttributionSimulationEventAndValue =
    std::pair<AttributionSimulationEvent, base::Value>;

using AttributionSimulationEventAndValues =
    std::vector<AttributionSimulationEventAndValue>;

absl::optional<AttributionSimulationEventAndValues>
ParseAttributionSimulationInput(base::Value input,
                                base::Time offset_time,
                                std::ostream& error_stream);

}  // namespace content

#endif  // CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
