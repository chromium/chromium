// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_PARSER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_PARSER_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"

namespace content {

struct AttributionConfig;

// See //content/test/data/attribution_reporting/simulator/README.md and
// //content/test/data/attribution_reporting/interop/README.md for the input
// and output JSON schema.

base::expected<base::Value::Dict, std::string>
    AttributionSimulatorInputFromInteropInput(base::Value::Dict);

base::expected<base::Value::Dict, std::string>
    AttributionInteropOutputFromSimulatorOutput(base::Value::Dict);

base::expected<AttributionConfig, std::string> ParseAttributionConfig(
    const base::Value::Dict&);

// Returns a non-empty string on failure.
[[nodiscard]] std::string MergeAttributionConfig(const base::Value::Dict&,
                                                 AttributionConfig&);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_PARSER_H_
