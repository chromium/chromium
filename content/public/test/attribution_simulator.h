// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
#define CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_

#include <iosfwd>

namespace base {
class Value;
}  // namespace base

namespace content {

struct AttributionConfig;

// Simulates the Attribution Reporting API for a single user on sources and
// triggers specified in `input`. Returns the generated reports, if any, as a
// JSON document. On error, writes to `error_stream` and returns
// `base::ValueType::NONE`.
//
// Exits if `input` cannot be parsed.
base::Value RunAttributionSimulation(base::Value input,
                                     const AttributionConfig&,
                                     std::ostream& error_stream);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
