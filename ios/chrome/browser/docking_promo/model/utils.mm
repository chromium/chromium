// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/model/utils.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"

namespace {

// Compare two optional TimeDelta and return the minimum of the two, but
// consider that std::nullopt is greater than any other TimeDelta (this
// is the opposite of operator <(...) which consider std::nullopt smaller
// than any defined value).
std::optional<base::TimeDelta> MinimumTimeDelta(
    std::optional<base::TimeDelta> lhs,
    std::optional<base::TimeDelta> rhs) {
  if (!lhs.has_value()) {
    return rhs;
  }

  if (!rhs.has_value()) {
    return lhs;
  }

  return std::min(*lhs, *rhs);
}

}  // namespace

BOOL IsDockingPromoForcedForDisplay() {
  NSString* forced_promo_name = experimental_flags::GetForcedPromoToDisplay();

  if ([forced_promo_name length] > 0) {
    std::optional<promos_manager::Promo> forced_promo =
        promos_manager::PromoForName(
            base::SysNSStringToUTF8(forced_promo_name));

    if (!forced_promo.has_value()) {
      return NO;
    }

    return forced_promo.value() == promos_manager::Promo::DockingPromo;
  }

  return NO;
}

BOOL CanShowDockingPromo(base::TimeDelta time_since_last_foreground) {
  if (IsDockingPromoForcedForDisplay()) {
    return YES;
  }

  if (IsChromeLikelyDefaultBrowser()) {
    return NO;
  }

  BOOL should_show_promo_for_new_user =
      IsFirstRunRecent(base::Days(2) + base::Seconds(1)) &&
      (time_since_last_foreground >
       InactiveThresholdForNewUsersUntilDockingPromoShown());

  BOOL should_show_promo_for_old_user =
      IsFirstRunRecent(base::Days(14) + base::Seconds(1)) &&
      (time_since_last_foreground >
       InactiveThresholdForOldUsersUntilDockingPromoShown());

  return should_show_promo_for_new_user || should_show_promo_for_old_user;
}

std::optional<base::TimeDelta> MinTimeSinceLastForeground(
    NSArray<SceneState*>* foregroundScenes) {
  std::optional<base::TimeDelta> minimum;
  for (SceneState* scene in foregroundScenes) {
    minimum = MinimumTimeDelta(
        minimum, GetTimeSinceMostRecentTabWasOpenForSceneState(scene));
  }
  return minimum;
}
