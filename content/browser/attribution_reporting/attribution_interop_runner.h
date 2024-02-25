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
struct AttributionInteropOutput;

// Simulates the Attribution Reporting API for a single user on sources and
// triggers specified in `input`. Returns the generated reports.
base::expected<AttributionInteropOutput, std::string>
RunAttributionInteropSimulation(base::Value::Dict input,
                                const AttributionConfig&);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_RUNNER_H_
