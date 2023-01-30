// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_BEACON_ID_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_BEACON_ID_H_

#include <stdint.h>

#include "base/types/strong_alias.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

using EventBeaconId = base::StrongAlias<struct EventBeaconTag, int64_t>;
using NavigationBeaconId =
    base::StrongAlias<struct NavigationBeaconTag, int64_t>;
using BeaconId = absl::variant<EventBeaconId, NavigationBeaconId>;

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_BEACON_ID_H_
