// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/live_activity/public/features.h"

#import "ios/chrome/browser/live_activity/public/features.h"

BASE_FEATURE(kFreLiveActivity, base::FEATURE_DISABLED_BY_DEFAULT);

const char kFreLiveActivityDynamicIslandModeParam[] = "dynamic_island_mode";
const char kFreLiveActivityNoDynamicIsland[] = "no_dynamic_island";
const char kFreLiveActivityShortDynamicIsland[] = "short_dynamic_island";
const char kFreLiveActivityLongDynamicIsland[] = "long_dynamic_island";
const char kFreLiveActivityStaleDurationSecondsParam[] =
    "stale_duration_seconds";
const char kFreLiveActivityShortDynamicIslandGraceSecondsParam[] =
    "short_dynamic_island_grace_seconds";

bool IsFreLiveActivityEnabled() {
  return IsIOSLiveActivityManagerEnabled() &&
         base::FeatureList::IsEnabled(kFreLiveActivity);
}
