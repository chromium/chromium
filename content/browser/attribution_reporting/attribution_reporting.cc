// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/attribution_reporting.h"

#include "base/time/time.h"

namespace content {

// static
const AttributionRateLimitConfig AttributionRateLimitConfig::kDefault = {
    .time_window = base::Days(30),
    .max_source_registration_reporting_origins = 100,
    .max_attribution_reporting_origins = 10,
    .max_attributions = 100,
};

bool AttributionRateLimitConfig::Validate() const {
  if (time_window <= base::TimeDelta())
    return false;

  if (max_source_registration_reporting_origins <= 0)
    return false;

  if (max_attribution_reporting_origins <= 0)
    return false;

  if (max_attributions <= 0)
    return false;

  return true;
}

}  // namespace content
