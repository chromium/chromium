// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_PUBLIC_FIRST_RUN_LIVE_ACTIVITY_SCREEN_UTIL_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_PUBLIC_FIRST_RUN_LIVE_ACTIVITY_SCREEN_UTIL_H_

#import <optional>
#import <vector>

#import "ios/chrome/browser/first_run/live_activity/public/first_run_live_activity_constants.h"

enum class ScreenType;

// Maps `step` to its corresponding `ScreenType` for cold-start resumption, or
// returns `std::nullopt` for `FirstRunLiveActivityStep::kNone`.
std::optional<ScreenType> ScreenTypeForLiveActivityStep(
    FirstRunLiveActivityStep step);

// Returns true iff `user_facing_screens` matches the platform's baseline FRE
// sequence (steps 1 -> 2 -> 3 -> 4 on iPhone, or steps 1 -> 2 -> 3 on iPad
// where `ScreenType::kLensInteractivePromo` is omitted).
bool IsFreLiveActivityEligibleForScreens(
    const std::vector<ScreenType>& user_facing_screens);

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_PUBLIC_FIRST_RUN_LIVE_ACTIVITY_SCREEN_UTIL_H_
