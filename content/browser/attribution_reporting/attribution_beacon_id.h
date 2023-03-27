// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_BEACON_ID_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_BEACON_ID_H_

#include <stdint.h>

#include "base/types/strong_alias.h"

namespace content {

using BeaconId = base::StrongAlias<struct BeaconTag, int64_t>;

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_BEACON_ID_H_
