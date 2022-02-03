// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
#define CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_

#include <vector>

#include "content/test/attribution_simulator_impl.h"

namespace base {
class Value;
}  // namespace base

namespace content {

std::vector<AttributionSimulationEvent> ParseAttributionSimulationInputOrExit(
    const base::Value& input);

}  // namespace content

#endif  // CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
