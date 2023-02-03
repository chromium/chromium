// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
#define CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"

namespace content {

struct AttributionConfig;

// Simulates the Attribution Reporting API for a single user on sources and
// triggers specified in `input`. Returns the generated reports, if any, as a
// JSON document.
base::expected<base::Value::Dict, std::string> RunAttributionSimulation(
    base::Value::Dict input,
    const AttributionConfig&);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
