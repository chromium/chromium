// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_ATTRIBUTION_SIMULATOR_IMPL_H_
#define CONTENT_TEST_ATTRIBUTION_SIMULATOR_IMPL_H_

#include <vector>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/public/test/attribution_simulator.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

struct AttributionTriggerAndTime {
  AttributionTrigger trigger;
  base::Time time;
};

using AttributionSimulationEvent =
    absl::variant<StorableSource, AttributionTriggerAndTime>;

base::Value RunAttributionSimulation(
    std::vector<AttributionSimulationEvent> events,
    const AttributionSimulationOptions& options);

}  // namespace content

#endif  // CONTENT_TEST_ATTRIBUTION_SIMULATOR_IMPL_H_
