// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/alarms/alarms_api_constants.h"

namespace extensions {
namespace alarms_api_constants {

// 0.016667 minutes  ~= 1s.
const double kDevDelayMinimum = 0.016667;

// Must use int for initializer so static_assert below will compile.  This can
// all be made better once C++17 inline variables are allowed.
constexpr int kReleaseDelayMinimumInitializer = 1;
const double kReleaseDelayMinimum = kReleaseDelayMinimumInitializer;

const char kWarningMinimumDevDelay[] =
    "Alarm delay is less than minimum of 1 minutes. In released .crx, alarm "
    "\"*\" will fire in approximately 1 minutes.";

const char kWarningMinimumReleaseDelay[] =
    "Alarm delay is less than minimum of 1 minutes. Alarm \"*\" will fire in "
    "approximately 1 minutes.";

const char kWarningMinimumDevPeriod[] =
    "Alarm period is less than minimum of 1 minutes. In released .crx, alarm "
    "\"*\" will fire approximately every 1 minutes.";

const char kWarningMinimumReleasePeriod[] =
    "Alarm period is less than minimum of 1 minutes. Alarm \"*\" will fire "
    "approximately every 1 minutes.";

static_assert(kReleaseDelayMinimumInitializer == 1,
              "warning message must be updated");

}  // namespace alarms_api_constants
}  // namespace extensions
