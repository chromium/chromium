// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ATTRIBUTION_REPORTING_H_
#define CONTENT_PUBLIC_BROWSER_ATTRIBUTION_REPORTING_H_

#include <stdint.h>

#include <limits>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

enum class AttributionNoiseMode {
  // Various aspects of the API are subject to noise:
  // - Sources are subject to randomized response
  // - Reports within a reporting window are shuffled
  // - Pending reports are randomly delayed when the browser comes online
  kDefault,
  // None of the above applies.
  kNone,
};

enum class AttributionDelayMode {
  // Reports are sent in reporting windows some time after attribution is
  // triggered.
  kDefault,
  // Reports are sent immediately after attribution is triggered.
  kNone,
};

// Controls rate limits for the API.
struct CONTENT_EXPORT AttributionRateLimitConfig {
  // The default rates used by th API.
  static const AttributionRateLimitConfig kDefault;

  // Returns true if this config is valid.
  [[nodiscard]] bool Validate() const;

  // Controls the rate-limiting time window for attribution.
  base::TimeDelta time_window = base::TimeDelta::Max();

  // Maximum number of distinct reporting origins that can register sources
  // for a given <source site, destination site> in `time_window`.
  int64_t max_source_registration_reporting_origins =
      std::numeric_limits<int64_t>::max();

  // Maximum number of distinct reporting origins that can create attributions
  // for a given <source site, destination site> in `time_window`.
  int64_t max_attribution_reporting_origins =
      std::numeric_limits<int64_t>::max();

  // Maximum number of attributions for a given <source site, destination
  // site, reporting origin> in `time_window`.
  int64_t max_attributions = std::numeric_limits<int64_t>::max();

  // When adding new members, the corresponding `Validate()` definition and
  // `operator==()` definition in `attribution_interop_parser_unittest.cc`
  // should also be updated.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ATTRIBUTION_REPORTING_H_
