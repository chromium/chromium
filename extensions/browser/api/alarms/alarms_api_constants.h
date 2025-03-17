// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_CONSTANTS_H_

#include <cinttypes>

#include "base/time/time.h"

namespace extensions {
namespace alarms_api_constants {

// Minimum specifiable alarm period (in minutes) for unpacked extensions.
inline constexpr base::TimeDelta kDevDelayMinimum = base::Seconds(1);
// Minimum specifiable alarm period (in minutes) for packed/crx MV2 extensions.
inline constexpr base::TimeDelta kMV2ReleaseDelayMinimum = base::Minutes(1);
// Minimum specifiable alarm period (in minutes) for packed/crx MV3 extensions.
// This is designed to align with the idle timeout of service workers.
inline constexpr base::TimeDelta kMV3ReleaseDelayMinimum = base::Seconds(30);

// Returns the minimum time delay (either one-time or periodic) for an extension
// with the given unpacked state and `manifest_version`.
base::TimeDelta GetMinimumDelay(bool is_unpacked, int manifest_version);

}  // namespace alarms_api_constants
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_CONSTANTS_H_
