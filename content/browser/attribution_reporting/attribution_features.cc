// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_features.h"

#include "base/feature_list.h"

namespace content {

BASE_FEATURE(kAttributionVerboseDebugReporting,
             "AttributionVerboseDebugReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace content
