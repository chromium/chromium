// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/attribution_simulator.h"

#include "base/values.h"
#include "content/test/attribution_simulator_impl.h"
#include "content/test/attribution_simulator_input_parser.h"

namespace content {

base::Value RunAttributionSimulationOrExit(
    const base::Value& input,
    const AttributionSimulationOptions& options) {
  return RunAttributionSimulation(ParseAttributionSimulationInputOrExit(input),
                                  options);
}

}  // namespace content
