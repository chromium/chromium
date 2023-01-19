// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
#define CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_

#include <iosfwd>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

struct AttributionTriggerAndTime {
  AttributionTrigger trigger;
  base::Time time;
};

struct AttributionSimulatorCookie {
  net::CanonicalCookie cookie;
  GURL source_url;
};

struct AttributionDataClear {
  base::Time time;
  base::Time delete_begin;
  base::Time delete_end;
  // If null, matches all origins.
  absl::optional<base::flat_set<url::Origin>> origins;
  bool delete_rate_limit_data;

  AttributionDataClear(base::Time time,
                       base::Time delete_begin,
                       base::Time delete_end,
                       absl::optional<base::flat_set<url::Origin>> origins,
                       bool delete_rate_limit_data);

  ~AttributionDataClear();

  AttributionDataClear(const AttributionDataClear&);
  AttributionDataClear(AttributionDataClear&&);

  AttributionDataClear& operator=(const AttributionDataClear&);
  AttributionDataClear& operator=(AttributionDataClear&&);
};

using AttributionSimulationEvent = absl::variant<StorableSource,
                                                 AttributionTriggerAndTime,
                                                 AttributionSimulatorCookie,
                                                 AttributionDataClear>;

using AttributionSimulationEvents = std::vector<AttributionSimulationEvent>;

absl::optional<AttributionSimulationEvents> ParseAttributionSimulationInput(
    base::Value input,
    base::Time offset_time,
    std::ostream& error_stream);

}  // namespace content

#endif  // CONTENT_TEST_ATTRIBUTION_SIMULATOR_INPUT_PARSER_H_
