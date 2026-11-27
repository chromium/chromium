// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/live_activity/public/first_run_live_activity_screen_util.h"

#import <algorithm>
#import <array>

#import "base/notreached.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ui/base/device_form_factor.h"

namespace {

constexpr auto kPhoneBaselineScreens = std::to_array({
    ScreenType::kSignIn,
    ScreenType::kHistorySync,
    ScreenType::kDefaultBrowserPromo,
    ScreenType::kLensInteractivePromo,
});

constexpr auto kTabletBaselineScreens = std::to_array({
    ScreenType::kSignIn,
    ScreenType::kHistorySync,
    ScreenType::kDefaultBrowserPromo,
});

}  // namespace

std::optional<ScreenType> ScreenTypeForLiveActivityStep(
    FirstRunLiveActivityStep step) {
  switch (step) {
    case FirstRunLiveActivityStep::kSignIn:
      return ScreenType::kSignIn;
    case FirstRunLiveActivityStep::kHistorySync:
      return ScreenType::kHistorySync;
    case FirstRunLiveActivityStep::kDefaultBrowserPromo:
      return ScreenType::kDefaultBrowserPromo;
    case FirstRunLiveActivityStep::kLensInteractivePromo:
      return ScreenType::kLensInteractivePromo;
    case FirstRunLiveActivityStep::kNone:
      return std::nullopt;
  }
  NOTREACHED();
}

bool IsFreLiveActivityEligibleForScreens(
    const std::vector<ScreenType>& user_facing_screens) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return std::ranges::equal(user_facing_screens, kTabletBaselineScreens);
  }
  return std::ranges::equal(user_facing_screens, kPhoneBaselineScreens);
}
