// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_features.h"

#include "base/feature_list.h"

namespace gaia::features {

// Switches account capabilities fetch to the "getAllVisible" URL that fetches
// all capabilities visible to Chrome instead of asking for a hardcoded list of
// capabilities that might be only partially available.
// Consult https://crbug.com/436151197 before enabling.
BASE_FEATURE(kGetAccountCapabilitiesUsesGetAllVisibleUrl,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace gaia::features
