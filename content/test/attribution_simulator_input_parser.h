// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
#define CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_

#include <iosfwd>
#include <vector>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {
class Value;
}  // namespace base

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

absl::optional<AttributionSimulationEvents> ParseAttributionSimulationInput(
    base::Value input,
    base::Time offset_time,
    std::ostream& error_stream);

}  // namespace content

#endif  // CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
