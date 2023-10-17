// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_CONSTANTS_H_

#include "base/time/time.h"

namespace extensions {
namespace alarms_api_constants {

// Minimum specifiable alarm period (in minutes) for unpacked extensions.
inline constexpr base::TimeDelta kDevDelayMinimum = base::Seconds(1);
// Minimum specifiable alarm period (in minutes) for packed/crx extensions.
inline constexpr base::TimeDelta kReleaseDelayMinimum = base::Minutes(1);

extern const char kWarningMinimumDevDelay[];
extern const char kWarningMinimumReleaseDelay[];
extern const char kWarningMinimumDevPeriod[];
extern const char kWarningMinimumReleasePeriod[];

}  // namespace alarms_api_constants
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_CONSTANTS_H_
