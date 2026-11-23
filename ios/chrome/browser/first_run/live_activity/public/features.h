// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_PUBLIC_FEATURES_H_

#import "base/feature_list.h"

// Feature flag controlling the First Run Experience (FRE) Onboarding Live
// Activity.
BASE_DECLARE_FEATURE(kFreLiveActivity);

// Parameter names and values for `kFreLiveActivity` Dynamic Island
// presentation mode, stale duration, and Short Dynamic Island grace duration
// when Chrome enters the background.
extern const char kFreLiveActivityDynamicIslandModeParam[];
extern const char kFreLiveActivityNoDynamicIsland[];
extern const char kFreLiveActivityShortDynamicIsland[];
extern const char kFreLiveActivityLongDynamicIsland[];
extern const char kFreLiveActivityStaleDurationSecondsParam[];
extern const char kFreLiveActivityShortDynamicIslandGraceSecondsParam[];

// Returns true when both `kIOSLiveActivityManager` and
// `kFreLiveActivity` are enabled.
bool IsFreLiveActivityEnabled();

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_PUBLIC_FEATURES_H_
