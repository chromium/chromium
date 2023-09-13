// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_features.h"

namespace content::features {

// Enables a new limit and scheduler for prerender triggers.
// See crbug.com/1464021 for more details.
BASE_FEATURE(kPrerender2NewLimitAndScheduler,
             "Prerender2NewLimitAndScheduler",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content::features
