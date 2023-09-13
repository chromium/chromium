// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_features.h"

namespace content::features {

// Kill-switch controlled by the field trial. When this feature is enabled,
// PrerenderHostRegistry doesn't query about the current memory footprint and
// bypasses the memory limit check, while it still checks the limit on the
// number of ongoing prerendering requests and memory pressure events to prevent
// excessive memory usage. See https://crbug.com/1382697 for details.
BASE_FEATURE(kPrerender2BypassMemoryLimitCheck,
             "Prerender2BypassMemoryLimitCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a new limit and scheduler for prerender triggers.
// See crbug.com/1464021 for more details.
BASE_FEATURE(kPrerender2NewLimitAndScheduler,
             "Prerender2NewLimitAndScheduler",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content::features
