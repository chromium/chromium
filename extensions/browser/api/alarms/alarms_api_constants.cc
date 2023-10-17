// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/alarms/alarms_api_constants.h"

namespace extensions {
namespace alarms_api_constants {

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

static_assert(kReleaseDelayMinimum == base::Minutes(1),
              "warning message must be updated");

}  // namespace alarms_api_constants
}  // namespace extensions
